#pragma once

#include "core.hpp"

namespace to
{
struct KneelDuration { float minimum{4.f}, maximum{7.f}; };

constexpr KneelDuration migrateKneelDefaults(KneelDuration value, unsigned defaultsRevision)
{
    // Older versions wrote the complete range even when saving an unrelated setting.
    if (defaultsRevision==0 && value.minimum==6.f && value.maximum==9.f) return {};
    return value;
}

inline KneelDuration kneelDuration(KneelDuration value)
{
    const KneelDuration defaults;
    value.minimum = std::isfinite(value.minimum) ? std::clamp(value.minimum,1.f,60.f) : defaults.minimum;
    value.maximum = std::isfinite(value.maximum) ? std::clamp(value.maximum,value.minimum,60.f) :
        std::max(defaults.maximum,value.minimum);
    return value;
}

inline bool hearsThrow(Vec from, Vec to, Vec bottom, Vec top, float radius)
{
    return approachCapsule(from,to,bottom,top).distance<=radius;
}

inline bool eligibleThrowWitness(bool missOnly, std::span<const std::uint32_t> hits, std::uint32_t actor)
{
    return !missOnly || std::ranges::find(hits,actor)==hits.end();
}

constexpr bool readyToInvestigate(bool madeGroundContact, float quietTime, float contactAge, bool finishing)
{
    // A sleeping body can stop delivering Havok contact events. Also cap the
    // wait after a ground impact so sliding clutter still makes a distraction.
    return finishing || quietTime>=.22f || (madeGroundContact && (quietTime>=.10f || contactAge>=.85f));
}

inline bool besideForInspection(Vec actor, Vec object)
{
    const auto offset = object-actor;
    return offset.x*offset.x+offset.y*offset.y<=80.f*80.f && offset.z>=-40.f && offset.z<=72.f;
}

enum class InspectionRoute { wait, kneel, approach };

inline bool kneelingPose(float standingHeight, float currentHeight)
{
    // Compare the pelvis to the actor's root so world elevation and scale
    // do not affect confirmation. A normal standing idle can shift a little.
    return std::isfinite(standingHeight) && std::isfinite(currentHeight) &&
        standingHeight>0.f && currentHeight>0.f && currentHeight<=standingHeight*.82f;
}

constexpr InspectionRoute inspectionRoute(float age, float quietTime, bool beside, bool staggering)
{
    if (age<.42f || quietTime<.16f || (staggering && age<1.25f)) return InspectionRoute::wait;
    return beside ? InspectionRoute::kneel : InspectionRoute::approach;
}
}
