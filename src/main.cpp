#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "./SandSimGame.hpp"
#include "./Formatters.hpp"

constexpr bool REDIRECT_STDOUT_TO_FILE = false;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    if constexpr (REDIRECT_STDOUT_TO_FILE) {
        freopen("stdout.txt","w",stdout);
        SDL_SetLogOutputFunction([](void *userdata, int category, SDL_LogPriority priority, const char *message) {
            puts(message);
        }, nullptr);
    }

    App* app = new SandSimGame();
    *appstate = app;
    THE_APP = app;
    return app->init(argc, argv);
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    App* app = (App*)appstate;
    return app->iterate();
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    App* app = (App*)appstate;
    return app->handle_event(event);
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    App* app = (App*)appstate;
    app->quit(result);
}