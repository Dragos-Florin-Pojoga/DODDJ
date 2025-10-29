#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

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

static SDL_Window *window = NULL;

Scene main_scene("Main Scene");
Uint64 last_tick = 0;

float total_time = 0.0f;
int frame_count = 0;
float current_fps = 0.0f;
const float FPS_UPDATE_INTERVAL = 0.5f;


void populate_scene(Scene& scene) {
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

    // const int num_circles = 25000;
    // const int num_squares = 25000;
    // const float min_radius = 2.0f, max_radius = 3.0f;
    // const float min_size = 2.0f, max_size = 3.0f;

    const int num_circles = 100;
    const int num_squares = 100;
    const float min_radius = 20.0f, max_radius = 30.0f;
    const float min_size = 20.0f, max_size = 30.0f;

    const float min_vel = -100.0f, max_vel = 100.0f;

    

    for (int i = 0; i < num_circles; ++i) {
        float radius_px = min_radius + (rand() / (float)RAND_MAX) * (max_radius - min_radius);
        float radius = radius_px * PIXELS_TO_METERS;
        Vec2D pos = { (50.0f + (rand() / (float)RAND_MAX) * 700.0f) * PIXELS_TO_METERS, (100.0f + (rand() / (float)RAND_MAX) * 400.0f) * PIXELS_TO_METERS };
        Vec2D vel = { min_vel * PIXELS_TO_METERS + (rand() / (float)RAND_MAX) * ((max_vel - min_vel) * PIXELS_TO_METERS), min_vel * PIXELS_TO_METERS + (rand() / (float)RAND_MAX) * ((max_vel - min_vel) * PIXELS_TO_METERS) };
        Entity circle = scene.m_entities.create("CIRCLE" + std::to_string(i+1));
        scene.m_transforms.add(circle, Transform2D{ pos, Vec2D{2 * radius, 2 * radius}, 0.0f });
        scene.m_rigidbodies.add(circle, Rigidbody2D{ false, 1.0f, vel });
        scene.m_circleColliders.add(circle, CircleCollider{ radius, Vec2D::zero });
        scene.m_renderables.add(circle, Renderable{ Renderable::Shape::CIRCLE, SDL_FColor{0.2f, 0.2f, 1.0f, 1.0f}, TextureCache::getShape(scene.renderer, SpriteType::Circle), Renderable::ZIndex::DEFAULT });
    }
    for (int i = 0; i < num_squares; ++i) {
        float size_px = min_size + (rand() / (float)RAND_MAX) * (max_size - min_size);
        float size = size_px * PIXELS_TO_METERS;
        Vec2D pos = { (50.0f + (rand() / (float)RAND_MAX) * 700.0f) * PIXELS_TO_METERS, (100.0f + (rand() / (float)RAND_MAX) * 400.0f) * PIXELS_TO_METERS };
        Vec2D vel = { min_vel * PIXELS_TO_METERS + (rand() / (float)RAND_MAX) * ((max_vel - min_vel) * PIXELS_TO_METERS), min_vel * PIXELS_TO_METERS + (rand() / (float)RAND_MAX) * ((max_vel - min_vel) * PIXELS_TO_METERS) };
        Entity square = scene.m_entities.create("SQUARE" + std::to_string(i+1));
        scene.m_transforms.add(square, Transform2D{ pos, Vec2D{size, size}, 0.0f });
        scene.m_rigidbodies.add(square, Rigidbody2D{ false, 1.0f, vel });
        scene.m_boxColliders.add(square, BoxCollider{ Vec2D{size/2, size/2}, Vec2D::zero });
        scene.m_renderables.add(square, Renderable{ Renderable::Shape::QUAD, SDL_FColor{1.0f, 0.5f, 0.2f, 1.0f}, nullptr, Renderable::ZIndex::DEFAULT });
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    srand(42); // TMP: FIXME: FIXED SEED

    SDL_SetAppMetadata("DODDJ", "1.0", "com.doddj.engine");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Logging::log_critical("Couldn't initialize SDL: ", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("DODDJ", 800, 600, SDL_WINDOW_RESIZABLE, &window, &main_scene.renderer)) {
        Logging::log_critical("Couldn't create window/renderer: ", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    Logging::log_info("SDL Renderer Info:");
    for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        Logging::log_info(i, " : ", SDL_GetRenderDriver(i));
    }
    Logging::log_info("SDL Selected Renderer:", SDL_GetRendererName(main_scene.renderer));

    SDL_SetRenderLogicalPresentation(main_scene.renderer, 800, 600, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    main_scene.set_as_active_scene();

    populate_scene(main_scene);

    last_tick = SDL_GetTicks();

    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    const Uint64 current_tick = SDL_GetTicks();

    const float real_dt = (float)(current_tick - last_tick) / 1000.0f;
    last_tick = current_tick;

    total_time += real_dt;
    frame_count++;

    if (total_time >= FPS_UPDATE_INTERVAL) {
        current_fps = (float)frame_count / total_time;
        total_time = 0.0f;
        frame_count = 0;
    }

    Logging::log_debug("Frame Time: ", real_dt * 1000.0f, " ms | FPS: ", current_fps);

    SDL_SetRenderDrawColorFloat(main_scene.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE_FLOAT);
    SDL_RenderClear(main_scene.renderer);

    float dt = real_dt;
    if (dt > 0.1f) {
        dt = 0.1f;
    }
    main_scene.update(dt);

    SDL_RenderPresent(main_scene.renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        
        case SDL_EVENT_KEY_DOWN: {
            switch(event->key.key) {
                case SDLK_ESCAPE:
                    return SDL_APP_SUCCESS;
            }
        } break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{

}