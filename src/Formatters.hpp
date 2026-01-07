#pragma once

#include <SDL3/SDL.h>
#include <iostream>

#include "./Commons.hpp"
#include "./Entity.hpp"
#include "./Renderer.hpp"
#include "./Transform2D.hpp"
#include "./Vec2D.hpp"
#include "./App.hpp"

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
        case Renderable::ZIndex::BACKGROUND: os << "BACKGROUND(" << static_cast<u32>(z) << ")"; break;
        case Renderable::ZIndex::DEFAULT: os << "DEFAULT(" << static_cast<u32>(z) << ")"; break;
        case Renderable::ZIndex::FOREGROUND: os << "FOREGROUND(" << static_cast<u32>(z) << ")"; break;
        case Renderable::ZIndex::UI: os << "UI(" << static_cast<u32>(z) << ")"; break;
        default: os << "?(" << static_cast<u32>(z) << ")"; break;
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

App* THE_APP = nullptr;

// This kind of printing is very hacky...
// A proper ECS should be able to get the components and also the current registry :/
std::ostream& operator<<(std::ostream& os, const Entity& e) {
    os << "Entity(" << static_cast<u32>(e) << ") ["
       << THE_APP->m_current_scene->m_entities.getName(e) << "] {"
       << *THE_APP->m_current_scene->m_transforms.get(e) << ", "
       << *THE_APP->m_current_scene->m_renderables.get(e) << " }";
    return os;
}

std::ostream& operator<<(std::ostream& os, const EntityManager& em) {
    os << "EntityManager{ NextID: " << static_cast<unsigned int>(em.m_next_id)
       << ", FreeIDs: " << em.m_free_ids.size()
       << ", NamedEntities: " << em.m_entity_names.size();

    for (const auto& pair : em.m_entity_names) {
        os << "\n- " << pair.first;
    }
    
    os << "\n}";
    return os;
}