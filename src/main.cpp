#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include <vector>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;


std::vector<SDL_Vertex> vertices;
std::vector<int> indices;

const auto center = SDL_FPoint{400, 300};
const float radius = 300;


SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    SDL_SetAppMetadata("DODDJ", "1.0", "com.doddj.engine");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("DODDJ", 800, 600, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Log("Available render drivers:");
    for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        SDL_Log("%d : %s", i, SDL_GetRenderDriver(i));
    }
    SDL_Log("Selected : %s", SDL_GetRendererName(renderer));

    SDL_SetRenderLogicalPresentation(renderer, 800, 600, SDL_LOGICAL_PRESENTATION_LETTERBOX); // ratio preserving autoscaling

    vertices.reserve(6 * 3);
    indices.reserve(1 + 6);

    vertices.push_back({ center, SDL_FColor{0.5f, 0.5f, 0.5f, 1.0f}, SDL_FPoint{0,0} });

    for (int i = 0; i < 6; ++i) {
        vertices.emplace_back();
    }

    for (int i = 1; i <= 6; ++i) {
        const int next = (i % 6) + 1;
        indices.emplace_back(0);
        indices.emplace_back(i);
        indices.emplace_back(next);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE_FLOAT);
    SDL_RenderClear(renderer);

    const double now = ((double)SDL_GetTicks()) / 1000.0;
    const float red = (0.5 + 0.5 * SDL_sin(now));
    const float green = (0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 2 / 3));
    const float blue = (0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 4 / 3));

    vertices[1].color = { red, green, blue };
    vertices[2].color = { green, blue, red };
    vertices[3].color = { blue, red, green };
    vertices[5].color = { red, blue, green };
    vertices[4].color = { green, red, blue };
    vertices[6].color = { blue, green, red };

    for (int i = 1; i < 7; ++i) {
        const float angle = SDL_PI_F / 3.0f * i + SDL_PI_F * SDL_sin(now) / 12;
        const float x = center.x + radius * SDL_cosf(angle);
        const float y = center.y + radius * SDL_sinf(angle);
        vertices[i].position = SDL_FPoint{x, y};
    }
    
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), vertices.size(), indices.data(), indices.size());

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{

}