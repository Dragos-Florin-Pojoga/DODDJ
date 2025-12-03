#pragma once

#include <SDL3/SDL.h>

#include "Logging.hpp"
#include "Vec2D.hpp"

// Coordinate space for operations
enum class Space {
    Self,  // Apply operation relative to the transform's local coordinate system
    World, // Apply operation relative to the world coordinate system
};

struct Transform2D {
public:
    Vec2D position;
    Vec2D scale;
    float rotation; // Rotation in degrees

    Transform2D() : position(Vec2D::zero), rotation(0.0f), scale(Vec2D::one) {}

    Transform2D(const Vec2D& pos, const Vec2D& scl = Vec2D::one, float rot = 0.0f) : position(pos), scale(scl), rotation(rot) {}

    void Translate(const Vec2D& translation, Space relativeTo = Space::Self) {
        if (relativeTo == Space::World) {
            position += translation;
        } else {
            // TODO: apply scale ?
            position += GetRight() * translation.x;
            position += GetUp() * translation.y;
        }
    }

    // Rotates by 'degrees' around its center (to the right)
    void Rotate(float degrees) { rotation += degrees; }

    // Normalizes rotation to [0, 360) degrees
    void NormalizeRotation() {
        rotation = std::fmod(rotation, 360.0f);
        if (rotation < 0.0f) {
            rotation += 360.0f;
        }
    }

    // Rotates the transform so its 'right' vector points at the target position
    // @param target The world-space position to look at
    void LookAt(const Vec2D& target) {
        if (target == position) {
            Logging::log_error("Transform2D::LookAt called with target equal to position; rotation will be unchanged.");
        }
        const Vec2D direction = (target - position).Normalized();
        rotation = direction.Angle();
    }

    // Gets the transform's local X-axis (right) in world space (normalized)
    Vec2D GetRight() const {
        const float rad = rotation * (SDL_PI_F / 180.0f);
        return Vec2D(std::cos(rad), std::sin(rad));
    }

    // Gets the transform's local "up" vector (negative Y) in world space (normalized)
    // This is the 'right' vector rotated -90 degrees (counter-clockwise).
    Vec2D GetUp() const {
        const float rad = rotation * (SDL_PI_F / 180.0f);
        return Vec2D(std::sin(rad), -std::cos(rad));
    }

    // Gets the transform's local "down" vector (positive Y) in world space (normalized)
    // This is the 'right' vector rotated +90 degrees (clockwise).
    Vec2D GetDown() const {
        const float rad = rotation * (SDL_PI_F / 180.0f);
        return Vec2D(-std::sin(rad), std::cos(rad));
    }

    // Gets the transform's local "left" vector (negative X) in world space (normalized)
    // This is the 'right' vector rotated 180 degrees (negated).
    Vec2D GetLeft() const {
        const float rad = rotation * (SDL_PI_F / 180.0f);
        return Vec2D(-std::cos(rad), -std::sin(rad));
    }
};
