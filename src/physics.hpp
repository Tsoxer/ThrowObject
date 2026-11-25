#pragma once
#include "config.hpp"

#include <optional>

namespace to
{
Vec vec(const RE::NiPoint3& point);
RE::NiPoint3 point(Vec v);
RE::bhkRigidBody* uniqueBody(RE::NiAVObject* root);
bool canHandleBody(RE::bhkRigidBody& body, bool looseInstance);
std::optional<float> livePickupDistance(const RE::TESObjectREFR& ref, RE::bhkRigidBody& body,
                                        const RE::NiAVObject& root, Vec player);
float estimatedArea(RE::TESBoundObject& item, float scale);
ItemKind itemKind(RE::FormType type);
bool clearRelease(RE::PlayerCharacter& player, Vec release, float radius, Vec direction,
                  const RE::hkpCollidable* playerBody = nullptr);
bool clipsThrower(RE::PlayerCharacter& player, Vec center, float radius, Vec velocity);
RE::TESObjectREFR* physicsPickupTarget(RE::PlayerCharacter& player, float distance);
RE::NiPointer<RE::Actor> toleranceAimTarget(RE::PlayerCharacter& player, float distance);
Vec firstPersonReleaseCenter(RE::PlayerCharacter& player, Vec center, float radius, int attempt);
std::optional<ThrowRelease> firstPersonThrow(RE::PlayerCharacter& player, RE::TESObjectREFR& item,
                                            Vec heldCenter, float radius, float area, const Settings& settings);
Vec launchVelocity(RE::PlayerCharacter& player, RE::TESObjectREFR& item, Vec release,
                   float area, const Settings& settings);
bool initializeReleasedPhysics(RE::PlayerCharacter& player, RE::TESObjectREFR& ref, Vec velocity,
                               float area, const Settings& settings);
void updateFlights(RE::PlayerCharacter& player, float dt, const Settings& settings);
void stopTracking(const RE::TESObjectREFR& ref);
bool flightActive();
void clearFlights();
void loadReactionTopic();
}
