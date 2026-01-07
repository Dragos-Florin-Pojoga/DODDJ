#pragma once

#include <cstring>

#include "./Commons.hpp"
#include "./App.hpp"
#include "./Scene.hpp"
#include "./Camera.hpp"
#include "./SandSimulation.hpp"
#include "imgui.h"

static const char* particle_names[] = { "AIR", "STONE", "SAND", "WATER" };

class SandSimScene : public Scene {
public:
    SandSimScene() : Scene("SandSimScene") {}
    ~SandSimScene() = default;

    i32 m_brush_size = 3;
    i32 m_selected_particle = static_cast<i32>(ParticleID::SAND);

    void render_UI(f32 dt, SDL_Renderer* renderer) override {
        ImGui::Begin("Menu");
        ImGui::SliderInt("Slowdown factor", &SLOWDOWN_FACTOR, 1, 100);
        ImGui::SliderInt("Brush size", &m_brush_size, 1, 50);
        ImGui::Combo("Particle type", &m_selected_particle, particle_names, IM_ARRAYSIZE(particle_names));
        ImGui::End();
    }
};

class SandSimGame : public App {
public:
    SandSimGame() : App() {
        m_window_width = 1400;
        m_window_height = 1100;
        m_window_title = "SandSim";

        m_main_scene.m_camera.m_zoom = 0.7f;
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

        const float size_w = m_sand_world.width() / PIXELS_PER_METER * m_sand_pixel_size;
        const float size_h = m_sand_world.height() / PIXELS_PER_METER * m_sand_pixel_size;

        Entity sand_entity = m_main_scene.m_entities.create("SandSimulation");

        m_main_scene.m_transforms.add(sand_entity, Transform2D({0, 0}, {size_w, size_h}, 0.0f));
        m_main_scene.m_renderables.add(sand_entity, Renderable{Renderable::Shape::QUAD, Renderable::ZIndex::DEFAULT, m_sand_world_texture});

        Logging::log_debug(m_main_scene.m_entities);

        return SDL_APP_CONTINUE;
    }

    void downstream_iterate(f32 dt) override {
        // Continuous painting while left mouse held
        if (m_left_mouse_held) {
            paint(m_mouse_x, m_mouse_y);
        }

        if (m_benchmark_mode) {
            const f32 center_x = static_cast<f32>(m_sand_world.width()) / 2.0f;
            const f32 center_y = static_cast<f32>(m_sand_world.height()) / 2.0f * 0.3f;
            const f32 t = static_cast<f32>(m_benchmark_current_iteration) * 0.02f;

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
            const i32 sand_x = static_cast<i32>(center_x + std::cos(-t + M_PI) * sand_radius);
            const i32 sand_y = static_cast<i32>(center_y + std::sin(-t + M_PI) * sand_radius * 0.5f);
            for (i32 dx = -5; dx <= 5; ++dx) {
                for (i32 dy = -5; dy <= 5; ++dy) {
                    m_sand_world.setParticle(static_cast<u32>(sand_x + dx), static_cast<u32>(sand_y + dy), ParticleID::SAND);
                }
            }

            ++m_benchmark_current_iteration;
            if (m_benchmark_current_iteration >= m_benchmark_iterations) {
                Logging::log_info("Benchmark complete: ", m_benchmark_iterations, " iterations");
                SDL_Event quit_event{};
                quit_event.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&quit_event);
            }
        }
        
        m_sand_world.update();
        m_sand_world.renderToTexture(m_sand_world_texture);
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
                    m_sand_world.clear();
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
        
        const f32 size_w = m_sand_world.width() / PIXELS_PER_METER * m_sand_pixel_size;
        const f32 size_h = m_sand_world.height() / PIXELS_PER_METER * m_sand_pixel_size;
        
        // world position to texture pixel coords
        // World coords: center of texture at (0,0), extends from -size/2 to +size/2
        // Texture coords: (0,0) at top-left, (width-1, height-1) at bottom-right
        const f32 tex_x_f = (world_pos.x / size_w + 0.5f) * m_sand_world.width();
        const f32 tex_y_f = (world_pos.y / size_h + 0.5f) * m_sand_world.height();
        
        const i32 center_x = static_cast<i32>(tex_x_f);
        const i32 center_y = static_cast<i32>(tex_y_f);
        
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
    const u32 m_sand_pixel_size = 4;

    // Benchmark mode
    bool m_benchmark_mode = false;
    u32 m_benchmark_iterations = 0;
    u32 m_benchmark_current_iteration = 0;
};