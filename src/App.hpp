#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <random>
#include <string>

#include "./Commons.hpp"
#include "./Logging.hpp"
#include "./Scene.hpp"
#include "SDL3/SDL_render.h"

class App {
public: // Settings
    i32 m_window_width = 1400;
    i32 m_window_height = 800;

    const char* m_app_name = "DODDJ";
    const char* m_app_version = "1.0";
    const char* m_app_id = "com.doddj.engine";

    const char* m_window_title = "DODDJ";

    f32 m_font_size = 20.0f;

public:
    // Cornflower blue :)
    ImVec4 m_clear_color = ImVec4(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);

    Scene* m_current_scene = nullptr;

public:
    App() {}
    ~App() = default;

    SDL_AppResult init(i32 argc, char** argv) {
        srand(42);

        SDL_SetAppMetadata(m_app_name, m_app_version, m_app_id);

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            Logging::log_critical("Couldn't initialize SDL: ", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan,direct3d12,direct3d11");

        const f32 main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

        const i32 window_width = m_window_width * main_scale;
        const i32 window_height = m_window_height * main_scale;

        SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

        if (!SDL_CreateWindowAndRenderer(m_window_title, window_width, window_height, window_flags, &m_window, &m_renderer)) {
            Logging::log_critical("Couldn't create window/renderer: ", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        SDL_SetRenderVSync(m_renderer, SDL_RENDERER_VSYNC_ADAPTIVE);

        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(m_window);

        Logging::log_debug("SDL Renderer Info:");
        for (u32 i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
            Logging::log_debug(i, " : ", SDL_GetRenderDriver(i));
        }
        Logging::log_info("SDL Selected Renderer: ", SDL_GetRendererName(m_renderer));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        m_imgui_io = &ImGui::GetIO();
        (void)m_imgui_io;
        m_imgui_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        m_imgui_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        // m_imgui_io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        m_imgui_io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Does not work well on linux... wayland is out of the question :/

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        style.FontScaleDpi = main_scale;
        style.FontSizeBase = m_font_size;

        ImGui_ImplSDL3_InitForSDLRenderer(m_window, m_renderer);
        ImGui_ImplSDLRenderer3_Init(m_renderer);

        m_last_tick = SDL_GetTicks();

        SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);

        return downstream_init(argc, argv);
    }

    virtual SDL_AppResult downstream_init(i32 argc, char** argv) {
        return SDL_APP_CONTINUE;
    }

    SDL_AppResult iterate() {
        if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            return SDL_APP_CONTINUE;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("Stats");
            ImGui::Text("avg: %.3f ms/frame (%.1f FPS)", 1000.0f / m_imgui_io->Framerate, m_imgui_io->Framerate);
            ImGui::ColorEdit3("clear color", (f32*)&m_clear_color);
            ImGui::End();
        }

        SDL_SetRenderDrawColorFloat(m_renderer, m_clear_color.x, m_clear_color.y, m_clear_color.z, m_clear_color.w);
        SDL_RenderClear(m_renderer);

        const Uint64 current_tick = SDL_GetTicks();
        f32 dt = (f32)(current_tick - m_last_tick) / 1000.0f;
        m_last_tick = current_tick;

        if (dt > 0.15f) {
            dt = 0.15f;
        }

        downstream_iterate(dt);

        m_current_scene->update(dt);
        m_current_scene->render(dt, m_renderer, m_window_width, m_window_height);

        ImGui::Render();
        SDL_SetRenderScale(m_renderer, m_imgui_io->DisplayFramebufferScale.x, m_imgui_io->DisplayFramebufferScale.y);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
        if (m_imgui_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        SDL_RenderPresent(m_renderer);
        return SDL_APP_CONTINUE;
    }

    virtual void downstream_iterate(f32 dt) {}

    SDL_AppResult handle_event(SDL_Event* event) {
        ImGui_ImplSDL3_ProcessEvent(event);

        switch (event->type) {
            case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                if (event->window.windowID == SDL_GetWindowID(m_window)) {
                    return SDL_APP_SUCCESS;
                }
            }

            case SDL_EVENT_KEY_DOWN: {
                switch (event->key.key) {
                    case SDLK_ESCAPE: return SDL_APP_SUCCESS;
                }
            } break;
        }

        return handle_downstream_event(event);
    }

    virtual SDL_AppResult handle_downstream_event(SDL_Event* event) {
        return SDL_APP_CONTINUE;
    }

    void quit(SDL_AppResult result) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        // TODO: destroy textures
        // Realisitcally, this would require an actual rendering system
        // which this project currently lacks :/
        // ...
        // So, we'll just let the OS deal with it >:3

        SDL_DestroyRenderer(m_renderer);
        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

protected:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    
    ImGuiIO* m_imgui_io = nullptr;

    u64 m_last_tick = 0;
};
