#pragma once

#include "core.hpp"

#include <numbers>

namespace to
{
inline constexpr float weaponSpinRate = 2.5f * 2.f * std::numbers::pi_v<float>;

inline Vec weaponLengthAxis(Vec dimensions)
{
    if (dimensions.x>0.f && dimensions.x>=std::max(dimensions.y,dimensions.z)) return {1,0,0};
    if (dimensions.y>0.f && dimensions.y>=dimensions.z) return {0,1,0};
    if (dimensions.z>0.f) return {0,0,1};
    return {};
}

inline Vec weaponAngularVelocity(Vec velocity, Vec lengthAxis)
{
    auto forward = Vec{velocity.x,velocity.y,0}.unit();
    if (forward.length()==0.f) forward = Vec{lengthAxis.x,lengthAxis.y,0}.unit();
    if (forward.length()==0.f) forward = {0,1,0};
    const Vec sideways{-forward.y,forward.x,0};
    const auto length = lengthAxis.unit();
    const auto acrossWeapon = [&](Vec axis) { return axis-length*axis.dot(length); };
    // Prefer the side axis of travel, but keep the axis perpendicular to the
    // weapon's length so it tumbles even when released sideways.
    auto axis = acrossWeapon(sideways);
    if (axis.length()<.01f) axis = acrossWeapon(forward);
    return axis.unit()*weaponSpinRate;
}
}
