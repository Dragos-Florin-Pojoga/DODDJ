#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>
#include <box2d/box2d.h>

#include <memory>
#include <string>

#include "./Commons.hpp"
#include "./Entity.hpp"
#include "./Camera.hpp"
#include "./Renderer.hpp"

class Scene {
public:
    Scene(std::string name = "Scene")
    : m_name(std::move(name)) {
        // b2WorldDef worldDef = b2DefaultWorldDef();
        // worldDef.gravity = b2Vec2(0.0f, 9.8f); // Positive DOWN
        // m_physics_world_id = b2CreateWorld(&worldDef);
    }
    ~Scene() {
        // b2DestroyWorld(m_physics_world_id);
    }

    const std::string& name() const { return m_name; }

    void update(f32 dt) {
        // const f32 m_physics_time_step = 1.0f / 60.0f;
        // for (f32 acc = dt; acc >= m_physics_time_step; acc -= m_physics_time_step) {
        //     b2World_Step(m_physics_world_id, m_physics_time_step, m_physics_substep_count);
        // }
        // b2World_Step(m_physics_world_id, dt, m_physics_substep_count);
        m_camera.update(dt);
    }

    void render(f32 dt, SDL_Renderer* renderer, i32 screenW, i32 screenH) {
        m_camera.setScreenSize(screenW, screenH);
        m_renderer->draw(renderer, m_camera);
        render_UI(dt, renderer);
    }

    virtual void render_UI(f32 dt, SDL_Renderer* renderer) {}

    std::string m_name;
    EntityManager m_entities;

    // b2WorldId m_physics_world_id;
    Camera m_camera;

    std::unique_ptr<RenderSystem> m_renderer = std::make_unique<RenderSystem>(m_transforms, m_renderables);
    ComponentStore<Transform2D> m_transforms;
    ComponentStore<Renderable> m_renderables;

    i32 m_physics_substep_count = 4;
};