#pragma once

#include <SDL3/SDL.h>
#include <iostream>

#include "./Commons.hpp"
#include "./Entity.hpp"
#include "./GLOBALS.hpp"
#include "./Physics.hpp"
#include "./Renderer.hpp"
#include "./Transform2D.hpp"
#include "./Vec2D.hpp"

// clang-format off

std::ostream& operator<<(std::ostream& os, const Vec2D& v) {
    os << "Vec2D(" << v.x << ", " << v.y << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Transform2D& t) {
    os << "Transform2D{ Pos: " << t.position
       << ", Scale: " << t.scale
       << ", Rot: " << t.rotation << " }";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Entity& e) {
    os << "Entity(" << static_cast<unsigned int>(e) << ") ["
       << GLOBALS.current_entity_manager->getName(e) << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Rigidbody2D& rb) {
    os << "Rigidbody2D{ static: " << (rb.is_static ? "true" : "false")
       << ", invMass: " << rb.invMass
       << ", vel: " << rb.velocity
       << ", force: " << rb.force
       << ", rest: " << rb.restitution
       << ", sleeping: " << (rb.is_sleeping ? "true" : "false")
       << " }";
    return os;
}

std::ostream& operator<<(std::ostream& os, const CircleCollider& cc) {
    os << "CircleCollider{ radius: " << cc.radius
       << ", offset: " << cc.offset
       << " }";
    return os;
}

std::ostream& operator<<(std::ostream& os, const BoxCollider& bc) {
    os << "BoxCollider{ size: " << bc.size
       << ", offset: " << bc.offset
       << " }";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Renderable::Shape& s) {
    switch (s) {
        case Renderable::Shape::QUAD: os << "QUAD"; break;
        case Renderable::Shape::CIRCLE: os << "CIRCLE"; break;
        default: os << "UNKNOWN"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Renderable::ZIndex& z) {
    switch (z) {
        case Renderable::ZIndex::BACKGROUND: os << "BACKGROUND(0)"; break;
        case Renderable::ZIndex::DEFAULT: os << "DEFAULT(100)"; break;
        case Renderable::ZIndex::FOREGROUND: os << "FOREGROUND(200)"; break;
        case Renderable::ZIndex::UI: os << "UI(300)"; break;
        default: os << "UNKNOWN"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const SDL_FColor& c) {
    os << "Color(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Renderable& r) {
    os << "Renderable{ shape: " << r.shape
       << ", color: " << r.color
       << ", texture: " << (r.texture ? "YES" : "NO")
       << ", z_index: " << r.z_index
       << " }";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Manifold& m) {
    os << "Manifold{ colliding: " << (m.colliding ? "true" : "false")
       << ", A: " << m.a
       << ", B: " << m.b
       << ", normal: " << m.normal
       << ", penetration: " << m.penetration
       << " }";
    return os;
}

std::ostream& operator<<(std::ostream& os, const EntityManager& em) {
    os << "EntityManager{ NextID: " << static_cast<unsigned int>(em.m_next_id)
       << ", FreeIDs: " << em.m_free_ids.size()
       << ", NamedEntities: " << em.m_entity_names.size();

    for (const auto& pair : em.m_entity_names) {
        os << "\n  " << pair.first << ": '" << pair.second << "'";
    }
    
    os << " }";
    return os;
}

