#pragma once

#include "equipment.hpp"

#include <optional>

namespace to
{
bool isOneHandedWeapon(const RE::TESBoundObject* base);
bool isStaff(const RE::TESBoundObject* base);
bool isCrossbow(const RE::TESBoundObject* base);
bool isThrowableWeapon(const RE::TESBoundObject* base);
bool hasEquipmentGroundDrop(const RE::TESBoundObject& base);
GripProfile carryGripProfile(const RE::TESBoundObject& base);
bool throwableBase(RE::TESBoundObject& base);
EquipmentFlight equipmentFlight(RE::TESBoundObject& base);
std::optional<float> ordinaryWeaponDamage(RE::PlayerCharacter& player, RE::Actor& target, RE::TESObjectREFR& object);
void applyWeaponEnchantment(RE::PlayerCharacter& player, RE::Actor& target, RE::TESObjectREFR& object);
}
