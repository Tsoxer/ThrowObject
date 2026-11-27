#pragma once

#include "weapon_spin.hpp"
#include "equipment.hpp"

#include <limits>

namespace to
{
struct WeaponGrip {
    Vec handle{};
    std::array<Vec,3> rows{Vec{1,0,0},Vec{0,1,0},Vec{0,0,1}};
};

inline float weaponVisualLocalScale(float worldScale, float handScale)
{
    return std::isfinite(handScale) && handScale>0.f ? worldScale/handScale : worldScale;
}

inline std::array<Vec,3> shaftGripRotation(Vec shaft)
{
    shaft = shaft.unit();
    const Vec face = std::abs(shaft.y)<.95f ? Vec{0,1,0} : Vec{0,0,1};
    const auto up = (face-shaft*face.dot(shaft)).unit();
    const Vec right{shaft.y*up.z-shaft.z*up.y,shaft.z*up.x-shaft.x*up.z,shaft.x*up.y-shaft.y*up.x};
    return {Vec{right.x,shaft.x,up.x},Vec{right.y,shaft.y,up.y},Vec{right.z,shaft.z,up.z}};
}

inline std::array<Vec,3> daggerGripRotation(Vec wristFromElbow)
{
    auto forearm = wristFromElbow.unit();
    if (forearm.length()==0.f) forearm = {0,0,1};
    // Keep the shaft across the palm and clear of the wrist-to-elbow line.
    const Vec thumbSide = std::abs(forearm.x)<.95f ? Vec{1,0,0} : Vec{0,1,0};
    return shaftGripRotation(thumbSide-forearm*thumbSide.dot(forearm));
}

inline std::optional<WeaponGrip> weaponGrip(Vec center, Vec half)
{
    auto shaft = weaponLengthAxis(half);
    const auto extent = half.dot(shaft);
    if (!(extent>0.f)) return std::nullopt;

    // Most weapon meshes already have their equipped pivot in the handle.
    // For other pivots, infer the butt from the bounds and grip near it.
    if (center.dot(shaft)<-extent*.5f) shaft = shaft*-1.f;
    const auto butt = center.dot(shaft)-extent;
    const auto zeroAlongShaft = -butt/(2.f*extent);
    const auto handleAlongShaft = zeroAlongShaft>=.04f && zeroAlongShaft<=.38f ? 0.f : butt+.36f*extent;
    const Vec transverse{std::abs(center.x)>half.x ? center.x : 0.f,
                         std::abs(center.y)>half.y ? center.y : 0.f,
                         std::abs(center.z)>half.z ? center.z : 0.f};
    const auto handle = transverse+shaft*(handleAlongShaft-transverse.dot(shaft));

    // The game's Weapon node points down the mesh's +Y axis. Map alternate
    // mesh axes to +Y without mirroring the blade or head.
    const Vec up = shaft.z!=0.f ? Vec{0,-1,0} : Vec{0,0,1};
    const Vec right{shaft.y*up.z-shaft.z*up.y,shaft.z*up.x-shaft.x*up.z,shaft.x*up.y-shaft.y*up.x};
    return WeaponGrip{handle,{right,shaft,up}};
}

using GripTriangle = std::array<Vec,3>;

struct GripSection { Vec center; float width; };

inline std::optional<GripSection> gripSection(std::span<const GripTriangle> triangles, Vec shaft, float cut)
{
    constexpr float limit = std::numeric_limits<float>::infinity();
    Vec low{limit,limit,limit};
    Vec high{-limit,-limit,-limit};
    for (const auto& triangle : triangles) {
        for (std::size_t i=0;i<triangle.size();++i) {
            const auto a = triangle[i];
            const auto b = triangle[(i+1)%triangle.size()];
            const float from = a.dot(shaft);
            const float to = b.dot(shaft);
            if (from==to || cut<std::min(from,to) || cut>std::max(from,to)) continue;
            const auto p = a+(b-a)*((cut-from)/(to-from));
            low = {std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
            high = {std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
    }
    if (!std::isfinite((high-low).length())) return std::nullopt;
    return GripSection{(low+high)*.5f,(high-low).length()};
}

inline std::optional<WeaponGrip> meleeModelGrip(Vec center, Vec half,
                                               std::span<const GripTriangle> triangles)
{
    if (triangles.empty()) return weaponGrip(center,half);
    constexpr float infinity = std::numeric_limits<float>::infinity();
    Vec low{infinity,infinity,infinity};
    Vec high{-infinity,-infinity,-infinity};
    for (const auto& triangle : triangles)
        for (const auto p : triangle) {
            low = {std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
            high = {std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
    const auto modelCenter = (low+high)*.5f;
    const auto modelHalf = (high-low)*.5f;
    // If only a small portion of the model exposed CPU vertices, use the
    // normal weapon socket fit based on the complete item bounds.
    if (modelHalf.length()<half.length()*.65f) return weaponGrip(center,half);
    auto grip = weaponGrip(modelCenter,modelHalf);
    if (!grip) return weaponGrip(center,half);
    const auto shaft = grip->rows[1];
    const auto extent = std::abs(modelHalf.dot(shaft));
    const float butt = modelCenter.dot(shaft)-extent;
    // The world mesh of a replacer can have a different axis from the
    // record bounds. Use its normal equipped grip when it crosses the hilt
    // end, then center the handle across the palm at that same axial point.
    const float cut = butt<0.f && butt>=-.76f*extent ? 0.f : grip->handle.dot(shaft);
    if (const auto section = gripSection(triangles,shaft,cut))
        grip->handle = section->center;
    return grip;
}

inline std::optional<WeaponGrip> fittedWeaponGrip(Vec center, Vec half, GripProfile profile,
                                                 std::span<const GripTriangle> triangles)
{
    if (profile==GripProfile::fittedMelee) return meleeModelGrip(center,half,triangles);
    const auto ordinary = weaponGrip(center,half);
    if (!ordinary || profile==GripProfile::standard) return ordinary;
    auto shaft = ordinary->rows[1];
    const auto extent = std::abs(half.dot(shaft));
    float low = center.dot(shaft)-extent;
    float high = center.dot(shaft)+extent;
    if (!triangles.empty()) {
        float modelLow = std::numeric_limits<float>::infinity();
        float modelHigh = -modelLow;
        for (const auto& triangle : triangles)
            for (const auto p : triangle) {
                modelLow = std::min(modelLow,p.dot(shaft));
                modelHigh = std::max(modelHigh,p.dot(shaft));
            }
        if (modelHigh-modelLow>extent*.65f) {
            low = modelLow;
            high = modelHigh;
        }
        const auto widthAt = [&](float fraction) {
            const auto section = gripSection(triangles,shaft,std::lerp(low,high,fraction));
            return section ? section->width : 0.f;
        };
        const float butt = std::max(widthAt(.06f),widthAt(.15f));
        const float head = std::max(widthAt(.85f),widthAt(.94f));
        if (butt>head*1.35f+.65f) {
            shaft = shaft*-1.f;
            const float formerLow = low;
            low = -high;
            high = -formerLow;
        }
    }
    const float cut = std::lerp(low,high,.45f);
    const auto section = gripSection(triangles,shaft,cut);
    const auto handle = section ? section->center : ordinary->handle+shaft*(cut-ordinary->handle.dot(shaft));
    const Vec up = shaft.z!=0.f ? Vec{0,-1,0} : Vec{0,0,1};
    const Vec right{shaft.y*up.z-shaft.z*up.y,shaft.z*up.x-shaft.x*up.z,shaft.x*up.y-shaft.y*up.x};
    return WeaponGrip{handle,{right,shaft,up}};
}
}
