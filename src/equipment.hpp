#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace to
{
enum class Equipment { none, oneHanded, headwear, gloves, boots, staff, crossbow, backpack };
enum class GripProfile { standard, fittedMelee, staff, spear };
constexpr float thrownSwingDamageScale = .5f;
constexpr std::uint32_t staffWeaponType = 8;
constexpr std::uint32_t crossbowWeaponType = 9;

constexpr bool oneHandedWeapon(std::uint32_t type, std::uint32_t baseForm = 0)
{
    // The standard Skyrim mining tool is usable one-handed even when the
    // loaded weapon's animation type doesn't identify it as a melee axe.
    if (baseForm==0x000E3C16) return true;
    // WEAP DNAM animation types: sword, dagger, war axe and mace.
    return type>=1 && type<=4;
}

constexpr Equipment weaponEquipment(std::uint32_t type, std::uint32_t baseForm = 0)
{
    if (type==staffWeaponType) return Equipment::staff;
    if (type==crossbowWeaponType) return Equipment::crossbow;
    return oneHandedWeapon(type,baseForm) ? Equipment::oneHanded : Equipment::none;
}

enum class RestingPickupLayer { other, clutter, weapon };

constexpr bool canRestoreRestingPickup(RestingPickupLayer layer, bool loose, bool fixed, bool supportedWeapon)
{
    if (supportedWeapon) return layer==RestingPickupLayer::weapon || layer==RestingPickupLayer::clutter;
    return loose && fixed && layer==RestingPickupLayer::clutter;
}

constexpr Equipment armorEquipment(std::uint32_t slots)
{
    // ARMO BOD2 slots. A helmet can also cover hair or the circlet slot.
    constexpr std::uint32_t head = (1u<<0)|(1u<<1)|(1u<<11);
    constexpr std::uint32_t hands = (1u<<3)|(1u<<4);
    constexpr std::uint32_t feet = (1u<<7)|(1u<<8);
    constexpr std::uint32_t excluded = (1u<<2)|(1u<<9)|(1u<<16)|(1u<<19)|(1u<<22)|(1u<<26);
    if (slots&excluded) return Equipment::none;
    switch (static_cast<int>(bool(slots&head)) + 2*bool(slots&hands) + 4*bool(slots&feet)) {
    case 1: return Equipment::headwear;
    case 2: return Equipment::gloves;
    case 4: return Equipment::boots;
    default: return Equipment::none;
    }
}

constexpr bool backpackSlots(std::uint32_t slots)
{
    constexpr std::uint32_t bodyOrShield = (1u<<2)|(1u<<9);
    return slots && !(slots&bodyOrShield);
}

constexpr char equipmentAsciiLower(char c)
{
    return c>='A' && c<='Z' ? static_cast<char>(c-'A'+'a') : c;
}

constexpr bool backpackLabel(std::string_view label)
{
    for (const std::string_view word : {"backpack","back pack","rucksack"}) {
        if (std::search(label.begin(),label.end(),word.begin(),word.end(),
                        [](char a, char b) { return equipmentAsciiLower(a)==b; })!=label.end()) return true;
    }
    return false;
}

constexpr bool backpackKeyword(std::string_view editorID)
{
    return editorID=="BagTypeBackpack" || editorID=="ArmorBackpack" || editorID=="ClothingBackpack" ||
           editorID=="CampfireBackpack" || editorID=="FrostfallBackpack" ||
           editorID=="_WetBackpackSmall" || editorID=="_WetBackpackMed" ||
           editorID=="_WetBackpackLarge" || editorID=="_WetBackpackHeavy";
}

struct EquipmentFlight { float speed, drag; };

constexpr EquipmentFlight equipmentFlight(Equipment equipment, bool soft)
{
    switch (equipment) {
    case Equipment::oneHanded: return {0.96f,0.92f};
    case Equipment::staff: return {0.89f,1.08f};
    case Equipment::crossbow: return {0.87f,1.15f};
    case Equipment::backpack: return soft ? EquipmentFlight{0.85f,1.35f} : EquipmentFlight{0.85f,1.23f};
    case Equipment::headwear: return soft ? EquipmentFlight{0.86f,1.32f} : EquipmentFlight{0.88f,1.12f};
    case Equipment::gloves: return soft ? EquipmentFlight{0.88f,1.4f} : EquipmentFlight{0.93f,1.08f};
    case Equipment::boots: return soft ? EquipmentFlight{0.87f,1.30f} : EquipmentFlight{0.85f,1.14f};
    default: return {1.f,1.f};
    }
}

inline float resistedSwingDamage(float ordinary, float physical, float resistedPhysical, float resistedTyped)
{
    if (!std::isfinite(physical) || physical<=0.f || !std::isfinite(resistedPhysical) ||
        !std::isfinite(resistedTyped)) return ordinary;
    const auto resisted = std::max(0.f,resistedPhysical)+std::max(0.f,resistedTyped);
    return ordinary*(1.f-std::clamp(resisted/physical,0.f,1.f));
}

}
