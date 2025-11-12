#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <vector>
#include <unordered_map>
#include <stack>
#include <algorithm>

#include "./Commons.hpp"
#include "./Logging.hpp"
#include "./GLOBALS.hpp"
#include "./Transform2D.hpp"
#include "./Entity.hpp"
#include "./ComponentStore.hpp"
#include "./Physics.hpp"
#include "./Renderer.hpp"
#include "./Scene.hpp"
#include "./Formatters.hpp"
#include "./Textures.hpp"

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

Scene main_scene("Main Scene");
Uint64 last_tick = 0;

int num_circles = 100;
int num_squares = 100;
float min_radius = 20.0f, max_radius = 30.0f;
float min_size = 20.0f, max_size = 30.0f;
float min_vel = -100.0f, max_vel = 100.0f;
bool add_physics = true;

void populate_scene(Scene& scene) {
    // FIXME: Extremely hacky "clear"
    for (size_t e = scene.m_entities.create(); e != (size_t)INVALID_ENTITY; --e) {
        scene.destroyEntity((Entity)e);
    }

    Entity ground = scene.m_entities.create("GROUND");
    scene.m_transforms.add(ground, Transform2D{ Vec2D{400 * PIXELS_TO_METERS, 580 * PIXELS_TO_METERS}, Vec2D{800 * PIXELS_TO_METERS, 40 * PIXELS_TO_METERS}, 0.0f });
    scene.m_rigidbodies.add(ground, Rigidbody2D{ true, 0.0f, Vec2D::zero });
    scene.m_boxColliders.add(ground, BoxCollider{ Vec2D{400 * PIXELS_TO_METERS, 20 * PIXELS_TO_METERS}, Vec2D::zero });
    scene.m_renderables.add(ground, Renderable{ Renderable::Shape::QUAD, SDL_FColor{0.2f, 0.8f, 0.2f, 1.0f}, nullptr, Renderable::ZIndex::DEFAULT });

    Entity left_wall = scene.m_entities.create("LEFT_WALL");
    scene.m_transforms.add(left_wall, Transform2D{ Vec2D{20 * PIXELS_TO_METERS, 300 * PIXELS_TO_METERS}, Vec2D{40 * PIXELS_TO_METERS, 600 * PIXELS_TO_METERS}, 0.0f });
    scene.m_rigidbodies.add(left_wall, Rigidbody2D{ true, 0.0f, Vec2D::zero });
    scene.m_boxColliders.add(left_wall, BoxCollider{ Vec2D{20 * PIXELS_TO_METERS, 300 * PIXELS_TO_METERS}, Vec2D::zero });
    scene.m_renderables.add(left_wall, Renderable{ Renderable::Shape::QUAD, SDL_FColor{0.2f, 0.8f, 0.2f, 1.0f}, nullptr, Renderable::ZIndex::DEFAULT });

    Entity right_wall = scene.m_entities.create("RIGHT_WALL");
    scene.m_transforms.add(right_wall, Transform2D{ Vec2D{780 * PIXELS_TO_METERS, 300 * PIXELS_TO_METERS}, Vec2D{40 * PIXELS_TO_METERS, 600 * PIXELS_TO_METERS}, 0.0f });
    scene.m_rigidbodies.add(right_wall, Rigidbody2D{ true, 0.0f, Vec2D::zero });
    scene.m_boxColliders.add(right_wall, BoxCollider{ Vec2D{20 * PIXELS_TO_METERS, 300 * PIXELS_TO_METERS}, Vec2D::zero });
    scene.m_renderables.add(right_wall, Renderable{ Renderable::Shape::QUAD, SDL_FColor{0.2f, 0.8f, 0.2f, 1.0f}, nullptr, Renderable::ZIndex::DEFAULT });

    for (int i = 0; i < num_circles; ++i) {
        float radius_px = min_radius + (rand() / (float)RAND_MAX) * (max_radius - min_radius);
        float radius = radius_px * PIXELS_TO_METERS;
        Vec2D pos = { (50.0f + (rand() / (float)RAND_MAX) * 700.0f) * PIXELS_TO_METERS, (100.0f + (rand() / (float)RAND_MAX) * 400.0f) * PIXELS_TO_METERS };
        Vec2D vel = { min_vel * PIXELS_TO_METERS + (rand() / (float)RAND_MAX) * ((max_vel - min_vel) * PIXELS_TO_METERS), min_vel * PIXELS_TO_METERS + (rand() / (float)RAND_MAX) * ((max_vel - min_vel) * PIXELS_TO_METERS) };
        Entity circle = scene.m_entities.create("CIRCLE" + std::to_string(i+1));
        scene.m_transforms.add(circle, Transform2D{ pos, Vec2D{2 * radius, 2 * radius}, 0.0f });
        if (add_physics) {
            scene.m_rigidbodies.add(circle, Rigidbody2D{ false, 1.0f, vel });
            scene.m_circleColliders.add(circle, CircleCollider{ radius, Vec2D::zero });
        }
        const float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        const float g = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        const float b = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        scene.m_renderables.add(circle, Renderable{ Renderable::Shape::CIRCLE, SDL_FColor{r,g,b, 1.0f}, TextureCache::getShape(scene.renderer, SpriteType::Circle), Renderable::ZIndex::DEFAULT });
    }
    for (int i = 0; i < num_squares; ++i) {
        float size_px = min_size + (rand() / (float)RAND_MAX) * (max_size - min_size);
        float size = size_px * PIXELS_TO_METERS;
        Vec2D pos = { (50.0f + (rand() / (float)RAND_MAX) * 700.0f) * PIXELS_TO_METERS, (100.0f + (rand() / (float)RAND_MAX) * 400.0f) * PIXELS_TO_METERS };
        Vec2D vel = { min_vel * PIXELS_TO_METERS + (rand() / (float)RAND_MAX) * ((max_vel - min_vel) * PIXELS_TO_METERS), min_vel * PIXELS_TO_METERS + (rand() / (float)RAND_MAX) * ((max_vel - min_vel) * PIXELS_TO_METERS) };
        Entity square = scene.m_entities.create("SQUARE" + std::to_string(i+1));
        scene.m_transforms.add(square, Transform2D{ pos, Vec2D{size, size}, 0.0f });
        if (add_physics) {
            scene.m_rigidbodies.add(square, Rigidbody2D{ false, 1.0f, vel });
            scene.m_boxColliders.add(square, BoxCollider{ Vec2D{size/2, size/2}, Vec2D::zero });
        }
        const float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        const float g = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        const float b = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        scene.m_renderables.add(square, Renderable{ Renderable::Shape::QUAD, SDL_FColor{r,g,b, 1.0f}, nullptr, Renderable::ZIndex::DEFAULT });
    }
}

ImGuiIO* io;
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    srand(42); // TMP: FIXME: FIXED SEED

    SDL_SetAppMetadata("DODDJ", "1.0", "com.doddj.engine");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        Logging::log_critical("Couldn't initialize SDL: ", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan,direct3d12,direct3d11");

    const float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    const int window_width = 800 * main_scale;
    const int window_height = 600 * main_scale;

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (!SDL_CreateWindowAndRenderer("DODDJ", window_width, window_height, window_flags, &window, &renderer)) {
        Logging::log_critical("Couldn't create window/renderer: ", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    main_scene.renderer = renderer;
    SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_DISABLED);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    Logging::log_info("SDL Renderer Info:");
    for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        Logging::log_info(i, " : ", SDL_GetRenderDriver(i));
    }
    Logging::log_info("SDL Selected Renderer: ", SDL_GetRendererName(renderer));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO(); (void)io;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    // io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Does not work well on linux... wayland is out of the question :/

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    style.FontSizeBase = 20.0f;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    main_scene.set_as_active_scene();

    populate_scene(main_scene);

    last_tick = SDL_GetTicks();

    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    {
        ImGui::Begin("Menu");
        ImGui::Text("avg: %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
        ImGui::ColorEdit3("clear color", (float*)&clear_color);

        ImGui::SliderInt("num_circles", &num_circles, 0, 100000);
        ImGui::SliderFloat("min_radius", &min_radius, 0.1f, 40.0f);
        ImGui::SliderFloat("max_radius", &max_radius, 0.1f, 40.0f);

        ImGui::SliderInt("num_squares", &num_squares, 0, 100000);

        ImGui::SliderFloat("min_size", &min_size, 0.1f, 40.0f);
        ImGui::SliderFloat("max_size", &max_size, 0.1f, 40.0f);

        ImGui::SliderFloat("min_vel", &min_vel, -500.0f, 500.0f);
        ImGui::SliderFloat("max_vel", &max_vel, -500.0f, 500.0f);

        ImGui::Checkbox("Physics ?", &add_physics);

        if (ImGui::Button("Regenerate")) {
            populate_scene(main_scene);
        }

        ImGui::End();
    }

    SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    SDL_RenderClear(renderer);

    const Uint64 current_tick = SDL_GetTicks();
    float dt = (float)(current_tick - last_tick) / 1000.0f;
    last_tick = current_tick;

    if (dt > 0.1f) {
        dt = 0.1f;
    }
    main_scene.update(dt);

    ImGui::Render();
    SDL_SetRenderScale(renderer, io->DisplayFramebufferScale.x, io->DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);

    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            if (event->window.windowID == SDL_GetWindowID(window)) {
                return SDL_APP_SUCCESS;
            }
        }
        
        case SDL_EVENT_KEY_DOWN: {
            switch (event->key.key) {
                case SDLK_ESCAPE:
                    return SDL_APP_SUCCESS;
            }
        } break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}