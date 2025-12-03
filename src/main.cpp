#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <stack>
#include <unordered_map>
#include <vector>

#include "./Commons.hpp"
#include "./ComponentStore.hpp"
#include "./Entity.hpp"
#include "./Formatters.hpp"
#include "./GLOBALS.hpp"
#include "./Logging.hpp"
#include "./Physics.hpp"
#include "./Renderer.hpp"
#include "./SandSimulation.hpp"
#include "./Scene.hpp"
#include "./Textures.hpp"
#include "./Transform2D.hpp"

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

Scene main_scene("Main Scene");
Uint64 last_tick = 0;

ImGuiIO* io;
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

#include "Array2D.hpp"

World<5, 3> world;
SDL_Texture* world_texture = nullptr;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    srand(42); // TMP: FIXME: FIXED SEED

    SDL_SetAppMetadata("DODDJ", "1.0", "com.doddj.engine");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        Logging::log_critical("Couldn't initialize SDL: ", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan,direct3d12,direct3d11");

    const float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    const int window_width = 1400 * main_scale;
    const int window_height = 800 * main_scale;

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (!SDL_CreateWindowAndRenderer("DODDJ", window_width, window_height, window_flags, &window, &renderer)) {
        Logging::log_critical("Couldn't create window/renderer: ", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    main_scene.renderer = renderer;
    SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_DISABLED);

    world_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, world.width(), world.height());
    SDL_SetTextureScaleMode(world_texture, SDL_SCALEMODE_NEAREST);

    Entity sand_entity = main_scene.m_entities.create("SandSimulation");

    const float center_x = (window_width / 2.0f) * PIXELS_TO_METERS;
    const float center_y = (window_height / 2.0f) * PIXELS_TO_METERS;
    const float size_w = world.width() * PIXELS_TO_METERS * 4;
    const float size_h = world.height() * PIXELS_TO_METERS * 4;

    main_scene.m_transforms.add(sand_entity, Transform2D({center_x, center_y}, {size_w, size_h}, 0.0f));
    main_scene.m_renderables.add(sand_entity, Renderable{Renderable::Shape::QUAD, {1.0f, 1.0f, 1.0f, 1.0f}, world_texture, Renderable::ZIndex::DEFAULT});

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    Logging::log_info("SDL Renderer Info:");
    for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        Logging::log_info(i, " : ", SDL_GetRenderDriver(i));
    }
    Logging::log_info("SDL Selected Renderer: ", SDL_GetRendererName(renderer));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    (void)io;
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

    last_tick = SDL_GetTicks();

    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);

    Array2D<int, 10, 10> hmm;

    hmm(10, 3) = 1;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    world.update();
    world.renderToTexture(renderer, world_texture);

    {
        ImGui::Begin("Menu");
        ImGui::Text("avg: %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
        ImGui::ColorEdit3("clear color", (float*)&clear_color);
        ImGui::SliderInt("Slowdown factor", &SLOWDOWN_FACTOR, 1, 100);

        ImGui::SliderFloat2("Position", (float*)&main_scene.m_transforms.get(main_scene.m_entities.getByName("SandSimulation"))->position, -100, 100);

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

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
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
                case SDLK_ESCAPE: return SDL_APP_SUCCESS;
            }
        } break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (world_texture) {
        SDL_DestroyTexture(world_texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}