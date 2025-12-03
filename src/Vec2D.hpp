#pragma once

#include <SDL3/SDL.h>
#include <cmath>

#include "./Commons.hpp"

// clang-format off

// Assumes a Y-Down coordinate system
//```
//         (0,-1)
//           ^
//  (-1,0) < 0 > (1,0)
//           V
//         (0,1)
//```
struct Vec2D {
    f32 x = 0.0f;
    f32 y = 0.0f;

    Vec2D() : x(0.0f), y(0.0f) {}
    Vec2D(f32 x, f32 y) : x(x), y(y) {}

    operator SDL_FPoint() const { return {x, y}; }

    Vec2D operator+(const Vec2D& v) const { return {x + v.x, y + v.y}; }
    Vec2D& operator+=(const Vec2D& v) { x += v.x; y += v.y; return *this; }

    Vec2D operator-(const Vec2D& v) const { return {x - v.x, y - v.y}; }
    Vec2D& operator-=(const Vec2D& v) { x -= v.x; y -= v.y; return *this; }

    Vec2D operator-() const { return {-x, -y}; }

    Vec2D operator*(f32 s) const { return {x * s, y * s}; }
    Vec2D& operator*=(f32 s) { x *= s; y *= s; return *this; }
    friend Vec2D operator*(f32 s, const Vec2D& v) { return {v.x * s, v.y * s}; }

    Vec2D operator/(f32 s) const { return {x / s, y / s}; }
    Vec2D& operator/=(f32 s) { x /= s; y /= s; return *this; }
    
    bool operator==(const Vec2D& v) const { return x == v.x && y == v.y; }
    bool operator!=(const Vec2D& v) const { return x != v.x || y != v.y; }

    f32 Magnitude() const { return std::sqrt(x * x + y * y); }
    f32 MagnitudeSquared() const { return x * x + y * y; }

    f32 Dot(const Vec2D& v) const { return x * v.x + y * v.y;}

    // Returns angle in degrees
    // 0 degrees is (1,0) [right], 90 degrees is (0,1) [down]
    f32 Angle() const { return std::atan2(y, x) * (180.0f / SDL_PI_F); }

    Vec2D& Normalize() {
        const f32 mag = Magnitude();
        if (mag > 0.0f) {
            *this /= mag;
        }
        return *this;
    }
    Vec2D Normalized() const {
        const f32 mag = Magnitude();
        if (mag > 0.0f) {
            return *this / mag;
        }
        return Vec2D(0.0f, 0.0f);
    }

    Vec2D& Clamp(const Vec2D& min_v, const Vec2D& max_v) {
        x = std::fmin(std::fmax(x, min_v.x), max_v.x);
        y = std::fmin(std::fmax(y, min_v.y), max_v.y);
        return *this;
    }
    Vec2D Clamped(const Vec2D& min_v, const Vec2D& max_v) const {
        return Vec2D(
            std::fmin(std::fmax(x, min_v.x), max_v.x),
            std::fmin(std::fmax(y, min_v.y), max_v.y)
        );
    }

    static const Vec2D zero;
    static const Vec2D one;
    static const Vec2D up;
    static const Vec2D down;
    static const Vec2D left;
    static const Vec2D right;
};

const Vec2D Vec2D::zero = {0.0f, 0.0f};
const Vec2D Vec2D::one = {1.0f, 1.0f};
const Vec2D Vec2D::up = {0.0f, -1.0f};
const Vec2D Vec2D::down = {0.0f, 1.0f};
const Vec2D Vec2D::left = {-1.0f, 0.0f};
const Vec2D Vec2D::right = {1.0f, 0.0f};
