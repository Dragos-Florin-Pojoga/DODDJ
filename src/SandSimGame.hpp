#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

#include "./Commons.hpp"
#include "./App.hpp"
#include "./Scene.hpp"
#include "./Camera.hpp"
#include "./SandSimulation.hpp"
#include "./PhysicsWorld.hpp"
#include "./RigidbodyManager.hpp"
#include "imgui.h"
#include "GlobalAtomics.hpp"

static const char* particle_names[] = { "AIR", "STONE", "SAND", "WATER" };

inline void ImGui__SliderU32(const char* label, u32* v, u32 v_min, u32 v_max) {
    ImGui::SliderScalar(label, ImGuiDataType_U32, v, &v_min, &v_max);
}

class SandSimScene : public Scene {
public:
    SandSimScene() : Scene("SandSimScene") {}
    ~SandSimScene() = default;

    i32 m_brush_size = 3;
    i32 m_selected_particle = static_cast<i32>(ParticleID::SAND);

    i32 m_sim_rate = 1;

    void render_UI(f32 dt, SDL_Renderer* renderer) override {
        ImGui::Begin("Menu");
        ImGui::Text("Sim Speed: %.1f SPS", g_sim_sps.load());
        ImGui::Separator();
        
        ImGui::Text("RBs:%d |SMCs:%d |DPs:%d", g_rigidbody_count.load(), g_static_mesh_count.load(), g_stat_debris_count.load());
        ImGui::Text("Timings(ms): Mesh Gen:%d |Phys Update:%d", g_stat_mesh_ms.load(), g_stat_update_ms.load());
        ImGui::Separator();

        ImGui::SliderInt("Brush size", &m_brush_size, 1, 50);
        ImGui::Combo("Particle type", &m_selected_particle, particle_names, IM_ARRAYSIZE(particle_names));
        ImGui::Separator();
        
        ImGui::Text("Simulation Rate");
        bool fixed_steps = g_fixed_steps_mode.load();
        if (ImGui::Checkbox("Fixed steps mode", &fixed_steps)) {
            g_fixed_steps_mode.store(fixed_steps);
        }
        if (fixed_steps) {
            ImGui::SliderInt("Rate", &m_sim_rate, -200, 200);
            if (m_sim_rate == 0) m_sim_rate = 1;  // no div 0
            if (m_sim_rate > 0) {
                ImGui::Text("%d steps/frame", m_sim_rate);
            } else {
                ImGui::Text("1 step every %d frames", -m_sim_rate);
            }
        }
        ImGui::Separator();

        ImGui::Text("Water Spreading");
        ImGui__SliderU32("Max distance", &WATER_MAX_DIST, 1, 10);
        ImGui__SliderU32("Falloff factor", &WATER_SPREAD_FALLOFF, 1, 10);
        ImGui::Separator();
        
        ImGui::End();
        
        if (m_debug_render_cb) {
            m_debug_render_cb(renderer);
        }
    }

    std::function<void(SDL_Renderer*)> m_debug_render_cb;
};

class SandSimGame : public App {
public:
    SandSimGame() : App() {
        m_window_width = 1400;
        m_window_height = 1100;
        m_window_title = "SandSim";

        m_main_scene.m_camera.m_zoom = 2.7f;
        m_main_scene.m_camera.m_target = m_main_scene.m_camera.screenToWorld({m_window_width * 0.4f, m_window_height * 0.35f});
        
        m_physics_world = std::make_unique<PhysicsWorld>();
    }

    ~SandSimGame() {
        stop_simulation_thread();
    }

    SDL_AppResult downstream_init(i32 argc, char** argv) override {
        for (i32 i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--benchmark") == 0 && i + 1 < argc) {
                m_benchmark_mode = true;
                m_benchmark_iterations = static_cast<u32>(std::atoi(argv[i + 1]));
                Logging::log_info("Benchmark mode enabled: ", m_benchmark_iterations, " iterations");
            }
        }

        m_current_scene = &m_main_scene;

        m_sand_world_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, m_sand_world.width(), m_sand_world.height());

        if (!m_sand_world_texture) {
            Logging::log_critical("Failed to create texture: ", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        SDL_SetTextureScaleMode(m_sand_world_texture, SDL_SCALEMODE_NEAREST);
        const f32 size_w = static_cast<f32>(m_sand_world.width()) / PIXELS_PER_METER;
        const f32 size_h = static_cast<f32>(m_sand_world.height()) / PIXELS_PER_METER;

        Entity sand_entity = m_main_scene.m_entities.create("SandSimulation");
        m_main_scene.m_transforms.add(sand_entity, Transform2D({size_w * 0.5f, size_h * 0.5f}, {size_w, size_h}, 0.0f));
        m_main_scene.m_renderables.add(sand_entity, Renderable{Renderable::Shape::QUAD, Renderable::ZIndex::DEFAULT, m_sand_world_texture});

        Logging::log_debug(m_main_scene.m_entities);

        start_simulation_thread();
        
        // TODO: FIXME: this is actually more than just debug rendering :/
        m_main_scene.m_debug_render_cb = [this](SDL_Renderer* renderer) {
            // lock to prevent flickering
            std::lock_guard<std::mutex> lock(m_physics_mutex);

            // TEMP: FIXME: this is NOT debug rendering
            // draw debris
            m_physics_world->render_debris(renderer, m_main_scene.m_camera);

            if (m_debug_draw) {
                m_physics_world->render_debug(renderer, m_main_scene.m_camera);
            }
        };

        return SDL_APP_CONTINUE;
    }

    void start_simulation_thread() {
        g_sim_running.store(true, std::memory_order_release);
        m_sim_thread = std::thread(&SandSimGame::simulation_thread_proc, this);
    }

    void simulation_thread_proc() {
        u64 last_step_count = g_sim_step_count.load();
        auto last_sps_update = std::chrono::steady_clock::now();

        while (g_sim_running.load(std::memory_order_acquire)) {
            if (g_fixed_steps_mode.load(std::memory_order_acquire)) { // fixed step mode
                std::unique_lock<std::mutex> lock(m_step_mutex);

                m_step_cv.wait(lock, [this] {
                    return g_steps_remaining.load(std::memory_order_acquire) > 0 || 
                           !g_sim_running.load(std::memory_order_acquire) ||
                           !g_fixed_steps_mode.load(std::memory_order_acquire);
                });
                
                if (!g_sim_running.load(std::memory_order_acquire)) {
                    break;
                }
                
                if (g_fixed_steps_mode.load(std::memory_order_acquire)) { // consume step
                    g_steps_remaining.fetch_sub(1, std::memory_order_release);
                }
            }

            if (m_benchmark_mode) {
                run_benchmark_iteration();
            }

            ////////////////////////////
            // displacement & physics //
            ////////////////////////////

            // static terrain mesh generation
            const auto start_mesh = std::chrono::high_resolution_clock::now();
            const auto chains = m_sand_world.mesh_world_parallel();
            const auto end_mesh = std::chrono::high_resolution_clock::now();
            
            // step sand simulation
            m_sand_world.update();

            {
                std::lock_guard<std::mutex> lock(m_physics_mutex);

                // extract rigidbody pixels (remove from world)
                m_rigidbody_manager.extract_all(m_sand_world);
                
                // update static terrain mesh for physics
                const auto start_update = std::chrono::high_resolution_clock::now();
                m_physics_world->update_terrain_mesh(chains);
                const auto end_update = std::chrono::high_resolution_clock::now();
                
                // update counters
                g_rigidbody_count.store(static_cast<i32>(m_physics_world->get_dynamic_body_count()), std::memory_order_release);
                g_static_mesh_count.store(m_physics_world->get_terrain_shape_count(), std::memory_order_release);
                
                // step physics (FIXED STEP)
                // TODO: see if varying step might be better?
                m_physics_world->step(1.0f / 60.0f);
                
                const u64 debris_count = m_physics_world->debris_count();
                
                // restore rigidbody pixels & handle displacement
                // manual iteration to get body info for "top ejection"
                for (const auto& [id, info] : m_rigidbody_manager.get_bodies()) {
                    if (!b2Body_IsValid(info.body_id)) {
                        continue;
                    }
                    
                    // restore pixels for this body
                    const auto displaced = m_rigidbody_manager.restore_body_pixels(id, m_sand_world);
                    
                    // Calculate spawn height
                    const b2Transform xf = b2Body_GetTransform(info.body_id);
                    const f32 hx = info.width * 0.5f;
                    const f32 hy = info.height * 0.5f;
                    const b2Vec2 corners[4] = {{-hx, -hy}, {hx, -hy}, {hx, hy}, {-hx, hy}};

                    f32 min_y = 1e9f;
                    for(i32 i = 0; i < 4; ++i) {
                        const b2Vec2 v = b2TransformPoint(xf, corners[i]);
                        if(v.y < min_y) {
                            min_y = v.y;
                        }
                    }

                    const f32 top_y = min_y - (2.0f / PIXELS_PER_METER);
                    
                    // create debris for displaced particles
                    for (const auto& [px, py, type] : displaced) {
                        f32 vx = (fast_rand() % 100 - 50) / 25.0f; // soft spread (+/- 2.0)
                        f32 vy = -1.0f - (fast_rand() % 50) / 25.0f; // soft upward pop (-1.0 to -3.0)
                        
                        // spawn at pixel's X, but Body's Top Y
                        m_physics_world->create_debris(px / PIXELS_PER_METER, top_y, vx, vy,type);
                    }
                }
                
                // update debris (settling)
                m_physics_world->update_debris(m_sand_world);
                
                auto mesh_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_mesh - start_mesh).count();
                auto update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_update - start_update).count();
                
                g_stat_mesh_ms.store(static_cast<i32>(mesh_ms), std::memory_order_relaxed);
                g_stat_update_ms.store(static_cast<i32>(update_ms), std::memory_order_relaxed);
                g_stat_debris_count.store(static_cast<i32>(debris_count), std::memory_order_relaxed);
                g_stat_chains.store(static_cast<i32>(chains.size()), std::memory_order_relaxed);
            }
            ////////////////////////////

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_sps_update).count();
            if (elapsed >= 1000) {
                u64 steps_this_period = g_sim_step_count.load() - last_step_count;
                g_sim_sps.store(static_cast<f32>(steps_this_period) * 1000.0f / static_cast<f32>(elapsed), std::memory_order_release);
                last_step_count = g_sim_step_count.load();
                last_sps_update = now;
            }
        }
    }

    void stop_simulation_thread() {
        g_sim_running.store(false, std::memory_order_release);

        // wake thread if waiting
        {
            std::lock_guard<std::mutex> lock(m_step_mutex);
            g_steps_remaining.store(1, std::memory_order_release);
        }

        m_step_cv.notify_one();
        
        if (m_sim_thread.joinable()) {
            m_sim_thread.join();
        }
    }

    void run_benchmark_iteration() {
        const f32 center_x = static_cast<f32>(m_sand_world.width()) / 2.0f;
        const f32 center_y = static_cast<f32>(m_sand_world.height()) / 2.0f * 0.3f;
        const u32 iter = m_benchmark_current_iteration.fetch_add(1, std::memory_order_relaxed);
        const f32 t = static_cast<f32>(iter) * 0.02f;

        // Water spawner orbits clockwise
        const f32 water_radius = 80.0f;
        const i32 water_x = static_cast<i32>(center_x + std::cos(t) * water_radius);
        const i32 water_y = static_cast<i32>(center_y + std::sin(t) * water_radius * 0.5f);
        for (i32 dx = -5; dx <= 5; ++dx) {
            for (i32 dy = -5; dy <= 5; ++dy) {
                m_sand_world.setParticle(static_cast<u32>(water_x + dx), static_cast<u32>(water_y + dy), ParticleID::WATER);
            }
        }

        // Sand spawner orbits counter-clockwise
        const f32 sand_radius = 100.0f;
        const i32 sand_x = static_cast<i32>(center_x + std::cos(-t + SDL_PI_F) * sand_radius);
        const i32 sand_y = static_cast<i32>(center_y + std::sin(-t + SDL_PI_F) * sand_radius * 0.5f);
        for (i32 dx = -5; dx <= 5; ++dx) {
            for (i32 dy = -5; dy <= 5; ++dy) {
                m_sand_world.setParticle(static_cast<u32>(sand_x + dx), static_cast<u32>(sand_y + dy), ParticleID::SAND);
            }
        }

        if (iter >= m_benchmark_iterations) {
            Logging::log_info("Benchmark complete: ", m_benchmark_iterations, " iterations");
            SDL_Event quit_event{};
            quit_event.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quit_event);
        }
    }

    void downstream_iterate(f32 dt) override {
        // Continuous painting while left mouse held
        if (m_left_mouse_held) {
            paint(m_mouse_x, m_mouse_y);
        }

        static bool was_fixed = false;
        bool is_fixed = g_fixed_steps_mode.load(std::memory_order_acquire);
        
        // fixed step mode
        if (is_fixed) {
            const i32 rate = m_main_scene.m_sim_rate;
            if (rate > 0) { // steps per frame
                {
                    std::lock_guard<std::mutex> lock(m_step_mutex);
                    g_steps_remaining.store(rate, std::memory_order_release);
                }
                m_step_cv.notify_one();
            } else { // 1 step every -rate frames
                ++m_frame_counter;
                if (m_frame_counter >= -rate) {
                    m_frame_counter = 0;
                    {
                        std::lock_guard<std::mutex> lock(m_step_mutex);
                        g_steps_remaining.store(1, std::memory_order_release);
                    }
                    m_step_cv.notify_one();
                }
            }
        } else {
            m_frame_counter = 0; // reset when not in fixed mode
            if (was_fixed) {
                m_step_cv.notify_one(); // wake up thread
            }
        }
        was_fixed = is_fixed;
        
        // prevent rendering while sand is extracted
        if (m_sand_world_texture) {
            std::lock_guard<std::mutex> lock(m_physics_mutex);
            m_sand_world.renderToTexture(m_sand_world_texture);
        }
    }

    SDL_AppResult handle_downstream_event(SDL_Event* event) override {
        // Don't handle mouse events if ImGui wants to capture them
        const bool imgui_wants_mouse = ImGui::GetIO().WantCaptureMouse;
        
        switch (event->type) {
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                if (event->button.button == SDL_BUTTON_RIGHT && !imgui_wants_mouse) {
                    m_right_mouse_held = true;
                }
                if (event->button.button == SDL_BUTTON_LEFT && !imgui_wants_mouse) {
                    m_left_mouse_held = true;
                    paint(event->button.x, event->button.y);
                }
            } break;

            case SDL_EVENT_MOUSE_BUTTON_UP: {
                if (event->button.button == SDL_BUTTON_RIGHT) {
                    m_right_mouse_held = false;
                }
                if (event->button.button == SDL_BUTTON_LEFT) {
                    m_left_mouse_held = false;
                }
            } break;

            case SDL_EVENT_MOUSE_WHEEL: {
                if (m_right_mouse_held) {
                    const f32 zoom_speed = 0.1f;
                    m_main_scene.m_camera.m_zoom += event->wheel.y * zoom_speed;
                    m_main_scene.m_camera.m_zoom = std::max(0.1f, std::min(10.0f, m_main_scene.m_camera.m_zoom));
                } else {
                    m_main_scene.m_brush_size += static_cast<i32>(event->wheel.y);
                    m_main_scene.m_brush_size = std::max(1, std::min(50, m_main_scene.m_brush_size));
                }
            } break;

            case SDL_EVENT_MOUSE_MOTION: {
                m_mouse_x = event->motion.x;
                m_mouse_y = event->motion.y;
                if (m_right_mouse_held) {
                    const f32 sensitivity = 0.1f;
                    m_main_scene.m_camera.m_target.x -= event->motion.xrel * sensitivity / m_main_scene.m_camera.m_zoom;
                    m_main_scene.m_camera.m_target.y -= event->motion.yrel * sensitivity / m_main_scene.m_camera.m_zoom;
                }
            } break;

            case SDL_EVENT_KEY_DOWN: {
                if (event->key.key == SDLK_R) {
                    std::lock_guard<std::mutex> lock(m_physics_mutex);
                    m_sand_world.clear();
                    m_rigidbody_manager.clear();
                    m_physics_world->reset();
                }
                
                // B = spawn crate (box)
                if (event->key.key == SDLK_B) {
                    std::lock_guard<std::mutex> lock(m_physics_mutex);
                    
                    b2Vec2 world_pos = m_main_scene.m_camera.screenToWorld({m_mouse_x, m_mouse_y});
                    
                    // bounds in meters
                    const f32 max_w = m_sand_world.width() / PIXELS_PER_METER;
                    const f32 max_h = m_sand_world.height() / PIXELS_PER_METER;
                    
                    // box size in meters
                    const f32 box_size = 1.0f;
                    const f32 half_size = box_size * 0.5f;
                    
                    world_pos.x = std::clamp(world_pos.x, half_size + 0.1f, max_w - half_size - 0.1f);
                    world_pos.y = std::clamp(world_pos.y, half_size + 0.1f, max_h - half_size - 0.1f);
                    
                    // create physics body
                    b2BodyId bodyId = m_physics_world->create_box(world_pos.x, world_pos.y, box_size, box_size);
                    
                    // register with manager to track pixels
                    m_rigidbody_manager.register_body(bodyId, box_size, box_size, ParticleID::WOOD);
                }

                // D = toggle debug draw
                if (event->key.key == SDLK_D) {
                    m_debug_draw = !m_debug_draw;
                }
                // E = temporary eraser mode (hold)
                if (event->key.key == SDLK_E && !m_eraser_mode) {
                    m_eraser_mode = true;
                    m_saved_particle = m_main_scene.m_selected_particle;
                    m_main_scene.m_selected_particle = static_cast<i32>(ParticleID::AIR);
                }
                // Q = previous particle type (skip AIR)
                if (event->key.key == SDLK_Q) {
                    m_main_scene.m_selected_particle = (m_main_scene.m_selected_particle - 2 + 3) % 3 + 1;
                }
                // W = next particle type (skip AIR)
                if (event->key.key == SDLK_W) {
                    m_main_scene.m_selected_particle = (m_main_scene.m_selected_particle) % 3 + 1;
                }
            } break;

            case SDL_EVENT_KEY_UP: {
                // Release eraser mode
                if (event->key.key == SDLK_E && m_eraser_mode) {
                    m_eraser_mode = false;
                    m_main_scene.m_selected_particle = m_saved_particle;
                }
            } break;
        }
        return SDL_APP_CONTINUE;
    }

    void paint(f32 screen_x, f32 screen_y) {
        const b2Vec2 world_pos = m_main_scene.m_camera.screenToWorld({screen_x, screen_y});
        
        const f32 size_w = static_cast<f32>(m_sand_world.width()) / PIXELS_PER_METER;
        const f32 size_h = static_cast<f32>(m_sand_world.height()) / PIXELS_PER_METER;
        
        const i32 center_x = static_cast<i32>(world_pos.x * PIXELS_PER_METER);
        const i32 center_y = static_cast<i32>(world_pos.y * PIXELS_PER_METER);
        
        const i32 brush_radius = m_main_scene.m_brush_size - 1;  // size 1 = radius 0 = single pixel
        const ParticleID particle = static_cast<ParticleID>(m_main_scene.m_selected_particle);
        
        // Draw a filled circle
        for (i32 dy = -brush_radius; dy <= brush_radius; ++dy) {
            for (i32 dx = -brush_radius; dx <= brush_radius; ++dx) {
                if (dx * dx + dy * dy <= brush_radius * brush_radius) {
                    const i32 px = center_x + dx;
                    const i32 py = center_y + dy;
                    if (px >= 0 && px < static_cast<i32>(m_sand_world.width()) &&
                        py >= 0 && py < static_cast<i32>(m_sand_world.height())) {
                        m_sand_world.setParticle(static_cast<u32>(px), static_cast<u32>(py), particle);
                    }
                }
            }
        }
    }

    bool m_right_mouse_held = false;
    bool m_left_mouse_held = false;
    bool m_eraser_mode = false;
    i32 m_saved_particle = 0;
    f32 m_mouse_x = 0, m_mouse_y = 0;
    SandSimScene m_main_scene;
    SDL_Texture* m_sand_world_texture = nullptr;

    SandWorld<7, 5> m_sand_world;

    // Simulation thread
    std::thread m_sim_thread;

    // Fixed steps mode synchronization
    i32 m_frame_counter{0};  // Counts frames for slowdown mode
    std::mutex m_step_mutex;
    std::condition_variable m_step_cv;

    // Benchmark mode
    bool m_benchmark_mode = false;
    u32 m_benchmark_iterations = 0;
    std::atomic<u32> m_benchmark_current_iteration{0};
    
    // Physics
    std::unique_ptr<PhysicsWorld> m_physics_world;
    RigidbodyManager m_rigidbody_manager;
    ThreadPool m_mesh_thread_pool;
    std::mutex m_physics_mutex;
    bool m_debug_draw = false;
};
