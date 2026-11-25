#pragma once

#include "bindings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace to
{
enum class Phase { idle, pickup, lifting, held, throwing, released };
enum class Event { requestPickup, grab, pickupEnd, requestThrow, release, throwEnd, drop, cancel };

constexpr bool acceptsInput(Action action, Phase phase, bool hasOwnedItem)
{
    if (action == Action::pickUp) return phase == Phase::idle;
    if (!hasOwnedItem) return false;
    return phase == Phase::lifting || phase == Phase::held || phase == Phase::throwing;
}

constexpr Phase transition(Phase phase, Event event)
{
    if (event == Event::cancel) return Phase::idle;
    switch (phase) {
    case Phase::idle: return event == Event::requestPickup ? Phase::pickup : phase;
    case Phase::pickup: return event == Event::grab ? Phase::lifting : phase;
    case Phase::lifting:
        if (event == Event::drop) return Phase::idle;
        return event == Event::pickupEnd ? Phase::held : phase;
    case Phase::held:
        if (event == Event::drop) return Phase::idle;
        return event == Event::requestThrow ? Phase::throwing : phase;
    case Phase::throwing: return event == Event::release ? Phase::released : phase;
    case Phase::released: return event == Event::throwEnd ? Phase::idle : phase;
    }
    return Phase::idle;
}

constexpr int fps = 30, grabFrame = 22, pickupEndFrame = 45, releaseFrame = 15, throwEndFrame = 28;
constexpr float pickupTimeout = 2.2f;

constexpr bool sameCarryArea(std::uint32_t sourceCell, std::uint32_t sourceWorld,
                             std::uint32_t playerCell, std::uint32_t playerWorld)
{
    return sourceCell && playerCell && (sourceCell==playerCell || (sourceWorld && sourceWorld==playerWorld));
}

constexpr Phase afterPickupTimeout(Phase phase, float elapsed, bool parked)
{
    if (elapsed<=pickupTimeout) return phase;
    if (phase==Phase::lifting && parked) return Phase::held;
    if (phase==Phase::pickup || phase==Phase::lifting) return Phase::idle;
    return phase;
}

struct Vec {
    float x{}, y{}, z{};
    Vec operator+(Vec rhs) const { return {x+rhs.x,y+rhs.y,z+rhs.z}; }
    Vec operator-(Vec rhs) const { return {x-rhs.x,y-rhs.y,z-rhs.z}; }
    Vec operator*(float rhs) const { return {x*rhs,y*rhs,z*rhs}; }
    float dot(Vec rhs) const { return x*rhs.x+y*rhs.y+z*rhs.z; }
    float length() const { return std::sqrt(dot(*this)); }
    Vec unit() const { const auto n = length(); return n > 1e-5f ? *this*(1.0f/n) : Vec{}; }
};

inline float pickupReach(Vec player, Vec liveBody, float radius)
{
    // Allow reach to the body surface near the center without letting a large
    // or loosely bounded mesh extend pickup arbitrarily.
    const auto allowance = std::isfinite(radius) ? std::clamp(radius,0.f,12.f) : 0.f;
    return std::max((liveBody-player).length()-allowance,0.f);
}

// The projected center has zero horizontal and vertical numerator. Their
// row normals determine the direction, and the W row gives the facing sign.
inline Vec screenCenterDirection(Vec horizontal, Vec vertical, Vec depth)
{
    const Vec axis{horizontal.y*vertical.z-horizontal.z*vertical.y,
                   horizontal.z*vertical.x-horizontal.x*vertical.z,
                   horizontal.x*vertical.y-horizontal.y*vertical.x};
    const auto direction = axis.unit();
    const auto facing = direction.dot(depth.unit());
    if (!std::isfinite(facing) || std::abs(facing)<.5f) return {};
    return direction*(facing<0.f ? -1.f : 1.f);
}

enum class FirstPersonOrigin { crosshair, forward, held, right, diagonal };
struct ThrowRelease { Vec center, velocity; FirstPersonOrigin origin{}; };

template<class Probe>
std::optional<ThrowRelease> selectFirstPersonRelease(Probe probe)
{
    for (const auto origin : {FirstPersonOrigin::crosshair,FirstPersonOrigin::forward,
                              FirstPersonOrigin::held,FirstPersonOrigin::right,FirstPersonOrigin::diagonal}) {
        if (auto launch = probe(origin)) {
            launch->origin = origin;
            return launch;
        }
    }
    return std::nullopt;
}

// Require the object to leave the release area, then approach the player
// from outside the contact radius.
inline bool returningImpact(bool cleared, float age, float speedTowardPlayer, float previousDistance,
                            float contactDistance, float clearance)
{
    return cleared && age>=.18f && speedTowardPlayer>=1.f &&
           previousDistance>clearance && contactDistance<=clearance;
}

inline float returnKnockdown(float weight, bool metal, float speed)
{
    if (!(speed>=5.f && weight>=(metal ? 4.f : 8.f))) return 0.f;
    return std::clamp(weight*speed*.035f,1.f,6.f);
}

inline float projectedExtent(Vec half, Vec normal)
{
    return half.x*std::abs(normal.x)+half.y*std::abs(normal.y)+half.z*std::abs(normal.z);
}

inline Vec gripOpening(Vec knuckle, Vec thumb, Vec index, Vec middle)
{
    const auto palm = knuckle*.55f;
    const auto opening = (thumb+(index+middle)*.5f)*.5f;
    const auto offset = (opening-palm)*.80f;
    const auto limit = knuckle.length()*.50f;
    return palm+offset.unit()*std::min(limit,offset.length());
}

inline Vec hoverOffset(Vec half, const std::array<Vec,3>& meshAxes, Vec outward,
                       std::span<const Vec> handPoints)
{
    const auto extent = [&](Vec normal) {
        return projectedExtent(half,{normal.dot(meshAxes[0]),normal.dot(meshAxes[1]),normal.dot(meshAxes[2])});
    };
    // Small items stay centered at the opening. Larger bounds get only the
    // extra outward clearance they need away from the arm and shoulder.
    const auto sideways = outward*std::clamp((extent(outward)-5.f)*.32f,0.f,6.f);
    const Vec up{0,0,1};
    constexpr float gap = 1.5f;
    const auto riseFor = [&](Vec normal) {
        float handSupport = 0.f;
        for (auto point : handPoints) handSupport = std::max(handSupport,point.dot(normal));
        return (handSupport+extent(normal)+gap-sideways.dot(normal))/normal.z;
    };
    float rise = std::min(riseFor(up),riseFor((up+outward*.65f).unit()));
    // A tilted object's near face can clear the hand lower than its lowest
    // far corner. Use an up-and-out facing box plane when it allows that.
    for (auto axis : meshAxes) {
        const auto normal = axis.z<0.f ? axis*-1.f : axis;
        const float facing = normal.dot(outward);
        if (normal.z>.5f && facing>.08f) {
            const float weight = std::clamp((normal.z-.5f)/.15f,0.f,1.f) *
                                 std::clamp((facing-.08f)/.20f,0.f,1.f);
            rise = std::lerp(rise,std::min(rise,riseFor(normal)),weight);
        }
    }
    return sideways+up*std::max(gap,rise);
}

struct CapsuleApproach { float fraction, distance; Vec axisPoint; };

inline CapsuleApproach approachCapsule(Vec from, Vec to, Vec bottom, Vec top)
{
    const auto path = to-from;
    const auto axis = top-bottom;
    const auto relative = from-bottom;
    const float a = path.dot(path), b = path.dot(axis), c = axis.dot(axis);
    const float d = path.dot(relative), e = axis.dot(relative);
    const float denominator = a*c-b*b;
    auto clip = [](float t) { return std::clamp(t,0.f,1.f); };
    float along = denominator>1e-5f ? clip((b*e-c*d)/denominator) : 0.f;
    float height = c>1e-5f ? clip((b*along+e)/c) : 0.f;
    if (a>1e-5f) along = clip((b*height-d)/a);
    if (c>1e-5f) height = clip((b*along+e)/c);
    const auto point = bottom+axis*height;
    return {along,(from+path*along-point).length(),point};
}

enum class ItemKind { miscellaneous, food, bottle, book, scroll, ingredient, gem, key };

inline float drag(float weight, float area, ItemKind kind, float tuning)
{
    // Game balance approximation to Cd*A/m. Area is in square metres.
    constexpr std::array coefficients{0.95f,0.95f,0.75f,1.25f,1.20f,1.10f,0.65f,0.68f};
    const float inertia = std::clamp(weight, 0.025f, 35.0f);
    const float ratio = coefficients[static_cast<int>(kind)] * std::clamp(area, 0.0004f, 0.95f) / inertia;
    return std::clamp(2.45f * ratio, 0.015f, 1.65f) * tuning;
}

constexpr float throwSpeedMultiplier = 1.25f;
constexpr float throwDragMultiplier = .60f;

inline Vec applyDrag(Vec velocity, float dragPerMetre, float dt)
{
    // Exact integration of quadratic drag over a step at fixed direction.
    return velocity * (1.0f/(1.0f + dragPerMetre*velocity.length()*dt));
}

constexpr float throwRangeMultiplier = 2.f;

// Stretch only horizontal travel. Evaluating drag in the original space gives
// each item the same vertical arc and drag curve over the same flight time.
inline Vec doubleThrowRange(Vec value)
{
    return {value.x*throwRangeMultiplier,value.y*throwRangeMultiplier,value.z};
}

inline Vec referenceThrowSpace(Vec value)
{
    return {value.x/throwRangeMultiplier,value.y/throwRangeMultiplier,value.z};
}

inline Vec cursorVelocity(Vec forward, float speed)
{
    // Preserve the equivalent launch speed without changing the view pitch
    // by scaling the horizontal components alone.
    return forward*doubleThrowRange(forward*speed).length();
}

inline Vec applyThrowDrag(Vec velocity, float dragPerMetre, float dt)
{
    return doubleThrowRange(applyDrag(referenceThrowSpace(velocity),dragPerMetre,dt));
}

inline Vec aim(Vec hand, Vec target, Vec forward, float speed)
{
    auto direction = (target-hand).unit();
    if (direction.dot(forward) < 0.08f) direction = forward.unit();
    return direction * speed;
}

inline std::optional<Vec> ballisticCorrection(Vec delta, Vec forward, float speed, float gravity, float dragPerMetre)
{
    const auto horizontal = Vec{delta.x,delta.y,0};
    const auto distance = horizontal.length();
    if (distance < 0.5f || distance > 24.f || delta.unit().dot(forward)<0.08f) return std::nullopt;
    const auto axis = horizontal.unit();
    const auto pitch = std::atan2(delta.z,distance);
    float low = pitch, high = std::min(1.54f,pitch+0.24f);
    if (high<=low) return std::nullopt;
    auto atPitch = [&](float angle) { return axis*(std::cos(angle)*speed)+Vec{0,0,std::sin(angle)*speed}; };
    const auto heightError = [&](float angle) {
        constexpr float dt = 1.f/120.f;
        auto velocity = atPitch(angle);
        Vec position{};
        for (int step=0; step<240; ++step) {
            velocity.z -= gravity*dt;
            velocity = applyDrag(velocity,dragPerMetre,dt);
            const auto next = position+velocity*dt;
            if (next.dot(axis)>=distance) {
                const auto f = (distance-position.dot(axis))/(next-position).dot(axis);
                return position.z+f*(next.z-position.z)-delta.z;
            }
            position = next;
        }
        return -1.f;
    };
    // Compensate only reachable targets. Keep the direct aim for a distant
    // crosshair or one that would call for an exaggerated lob.
    if (heightError(high)<0) return std::nullopt;
    for (int i=0;i<12;++i) {
        const auto mid = (low+high)*0.5f;
        if (heightError(mid)<0) low=mid;
        else high=mid;
    }
    return atPitch((low+high)*0.5f);
}

inline Vec ballisticAim(Vec hand, Vec target, Vec forward, float speed, float gravity, float dragPerMetre)
{
    return ballisticCorrection(target-hand,forward,speed,gravity,dragPerMetre).value_or(
        aim(hand,target,forward,speed));
}

inline Vec ballisticActorThrow(Vec displacement, Vec forward, float speed, float gravity, float dragPerMetre)
{
    return doubleThrowRange(ballisticAim({},referenceThrowSpace(displacement),referenceThrowSpace(forward).unit(),
                                        speed,gravity,dragPerMetre));
}

inline float damage(float weight, float impact, float nominalSpeed, float scale, float cap)
{
    if (impact < 1.0f || nominalSpeed <= 0.0f) return 0.0f;
    const float energy = std::clamp(impact/nominalSpeed,0.08f,1.65f);
    const float base = 1.15f + 5.2f * std::sqrt(std::clamp(weight,0.0f,35.0f));
    return std::clamp(base * energy * energy * scale,0.0f,cap);
}
}
