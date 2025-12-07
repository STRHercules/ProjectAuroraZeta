// Minimal 2D vector for positions and velocities.
#pragma once

namespace Engine {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    Vec2() = default;
    Vec2(float xIn, float yIn) : x(xIn), y(yIn) {}

    Vec2& operator+=(const Vec2& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
};

inline Vec2 operator*(const Vec2& v, float scalar) { return Vec2{v.x * scalar, v.y * scalar}; }
inline Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2{a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2{a.x - b.x, a.y - b.y}; }

}  // namespace Engine
