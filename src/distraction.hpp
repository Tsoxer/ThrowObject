#pragma once

#include "distraction_rules.hpp"

namespace to
{
void loadInvestigationIdle();
void recoverInvestigationPackages();
bool canInvestigate(RE::Actor& actor);
bool isInvestigating(const RE::Actor& actor);
bool investigationsActive();
void queueNearbyInspection(RE::Actor& actor, RE::TESObjectREFR& object, bool alreadySpoke);
void cancelInspectionForHit(RE::Actor& actor, RE::TESObjectREFR& object);
void investigateLanding(RE::TESObjectREFR& object, Vec landing, std::span<const RE::ActorHandle> witnesses,
                        RE::FormID previousSpeaker);
void updateInvestigations(RE::PlayerCharacter& player, float dt, KneelDuration duration);
void clearInvestigations();
}
