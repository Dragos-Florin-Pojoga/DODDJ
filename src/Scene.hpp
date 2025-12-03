#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <string>

#include "./Commons.hpp"
#include "./ComponentStore.hpp"
#include "./Entity.hpp"
#include "./Physics.hpp"
#include "./Renderer.hpp"
#include "./Transform2D.hpp"

class Scene {
public:
    Scene(std::string name = "Scene") : m_name(std::move(name)) {}
    ~Scene() = default;

    const std::string& name() const { return m_name; }

    void set_as_active_scene() {
        GLOBALS.current_entity_manager = &m_entities;
        GLOBALS.current_scene = this;
    }

    void destroyEntity(Entity e) {
        if (e == INVALID_ENTITY)
            return;
        if (m_transforms.has(e))
            m_transforms.remove(e);
        if (m_rigidbodies.has(e))
            m_rigidbodies.remove(e);
        if (m_circleColliders.has(e))
            m_circleColliders.remove(e);
        if (m_boxColliders.has(e))
            m_boxColliders.remove(e);
        if (m_renderables.has(e))
            m_renderables.remove(e);
        m_entities.destroy(e);
    }

    void updatePhysics(float dt) {
        if (m_physics) {
            m_physics->update(dt);
        }
    }

    void updateRendering(SDL_Renderer* renderer) {
        if (m_renderer) {
            m_renderer->draw(renderer);
        }
    }

    void update(float dt) {
        updatePhysics(dt);
        updateRendering(renderer);
    }

    // TODO: wrapper over entity with builder pattern

    // TODO: dynamic componentstores

    // private:
    std::string m_name;
    EntityManager m_entities;

    ComponentStore<Transform2D> m_transforms;
    ComponentStore<Rigidbody2D> m_rigidbodies;
    ComponentStore<CircleCollider> m_circleColliders;
    ComponentStore<BoxCollider> m_boxColliders;

    ComponentStore<Renderable> m_renderables;

    std::unique_ptr<PhysicsSystem> m_physics = std::make_unique<PhysicsSystem>(m_transforms, m_rigidbodies, m_circleColliders, m_boxColliders, 1.0f / 720.0f); // high rez / more stable physics
    std::unique_ptr<RenderSystem> m_renderer = std::make_unique<RenderSystem>(m_transforms, m_renderables);

    SDL_Renderer* renderer = NULL;
};