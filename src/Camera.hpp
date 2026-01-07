#pragma once

#include <box2d/box2d.h>
#include <SDL3/SDL.h>

#include "Commons.hpp"

constexpr float PIXELS_PER_METER = 32.0f;
constexpr float METERS_PER_PIXEL = 1.0f / PIXELS_PER_METER;

// No rotation for now
// b2Vec2 = World space (in meters)
// SDL_FPoint = Screen space (in pixels)
// This will be handled by the renderer (SDL_RenderTextureRotated)
class Camera {
public:
    b2Vec2 m_position = {0.0f, 0.0f};
    b2Vec2 m_target = {0.0f, 0.0f};
    
    f32 m_zoom = 1.0f;
    
    f32 m_follow_speed = 10.0f;

    i32 m_screen_width = 1;
    i32 m_screen_height = 1;

    void setScreenSize(i32 w, i32 h) {
        m_screen_width = w;
        m_screen_height = h;
    }

    SDL_FPoint worldToScreen(const b2Vec2& world) const {
        const f32 x = (world.x - m_position.x) * PIXELS_PER_METER * m_zoom;
        const f32 y = (world.y - m_position.y) * PIXELS_PER_METER * m_zoom;

        return { x + m_screen_width * 0.5f, y + m_screen_height * 0.5f };
    }

    b2Vec2 screenToWorld(const SDL_FPoint& screen) const {
        const f32 x = (screen.x - m_screen_width * 0.5f) / (PIXELS_PER_METER * m_zoom);
        const f32 y = (screen.y - m_screen_height * 0.5f) / (PIXELS_PER_METER * m_zoom);

        return { x + m_position.x, y + m_position.y };
    }

    void update(f32 dt) {
        m_position += (m_target - m_position) * m_follow_speed * dt;
    }
};