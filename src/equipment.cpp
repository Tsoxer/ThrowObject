#include "equipment_game.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace to
{
namespace
{
static_assert(staffWeaponType==RE::WEAPON_TYPE::kStaff);
static_assert(crossbowWeaponType==RE::WEAPON_TYPE::kCrossbow);

Equipment weaponEquipmentOf(const RE::TESBoundObject* base)
{
    const auto* weapon = base ? base->As<RE::TESObjectWEAP>() : nullptr;
    return weapon ? weaponEquipment(weapon->GetWeaponType(),base->GetFormID()) : Equipment::none;
}

bool hasBackpackMetadata(const RE::TESObjectARMO& armor)
{
    if (std::ranges::any_of(armor.GetKeywords(),[](const auto* keyword) {
            const auto* id = keyword ? keyword->GetFormEditorID() : nullptr;
            return id && backpackKeyword(id);
        })) return true;

    // The usual backpack slot is shared with cloaks and lanterns. Use the
    // record/model identity as well, or an explicit keyword on custom slots.
    if (const auto* editor = armor.GetFormEditorID(); editor && backpackLabel(editor)) return true;
    if (std::ranges::any_of(armor.worldModels,[](const auto& model) {
            const auto* path = model.GetModel();
            if (!path) return false;
            const std::string_view view{path};
            const auto slash = view.find_last_of("/\\");
            return backpackLabel(slash==std::string_view::npos ? view : view.substr(slash+1));
        })) return true;
    if (!armor.HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kModBack)) return false;
    if (const auto* source = armor.GetFile(0); source &&
        std::ranges::equal(source->GetFilename(),std::string_view{"ccfsvsse001-backpacks.esl"},
                           std::ranges::equal_to{},equipmentAsciiLower)) return true;
    const auto* name = armor.GetFullName();
    return name && backpackLabel(name);
}

Equipment equipmentOf(const RE::TESBoundObject& base)
{
    if (const auto* armor = base.As<RE::TESObjectARMO>()) {
        const auto slots = static_cast<std::uint32_t>(armor->GetSlotMask());
        const auto regular = armorEquipment(slots);
        if (regular!=Equipment::none || !backpackSlots(slots) || armor->IsShield() ||
            (armor->GetFormFlags() & RE::TESObjectARMO::RecordFlags::kShield)) return regular;
        const auto* model = armor->templateArmor;
        return hasBackpackMetadata(*armor) || (model && model!=armor && hasBackpackMetadata(*model)) ?
            Equipment::backpack : Equipment::none;
    }
    return weaponEquipmentOf(&base);
}
}

bool isOneHandedWeapon(const RE::TESBoundObject* base)
{
    return weaponEquipmentOf(base)==Equipment::oneHanded;
}

bool isStaff(const RE::TESBoundObject* base)
{
    return weaponEquipmentOf(base)==Equipment::staff;
}

bool isCrossbow(const RE::TESBoundObject* base)
{
    return weaponEquipmentOf(base)==Equipment::crossbow;
}

bool isThrowableWeapon(const RE::TESBoundObject* base)
{
    return weaponEquipmentOf(base)!=Equipment::none;
}

bool hasEquipmentGroundDrop(const RE::TESBoundObject& base)
{
    const auto equipment = equipmentOf(base);
    return equipment==Equipment::staff || equipment==Equipment::crossbow || equipment==Equipment::backpack;
}

GripProfile carryGripProfile(const RE::TESBoundObject& base)
{
    if (base.Is(RE::FormType::Ammo)) {
        auto* data = RE::TESDataHandler::GetSingleton();
        return data && base.GetFormID()==data->LookupFormID(0x017720,"Dragonborn.esm") ?
            GripProfile::spear : GripProfile::standard;
    }
    if (isStaff(&base)) return GripProfile::staff;
    const auto* weapon = base.As<RE::TESObjectWEAP>();
    if (!weapon || !isOneHandedWeapon(&base)) return GripProfile::standard;

    bool iron{};
    for (const auto* model : std::array<const RE::TESObjectWEAP*,2>{weapon,weapon->templateWeapon}) {
        if (!model) continue;
        const auto form = model->GetFormID();
        if (form==0x000E3C16 || form==0x001019D4) return GripProfile::fittedMelee;
        if (const auto* path = model->GetModel()) {
            std::string lower{path};
            std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c) {
                return c=='/' ? '\\' : static_cast<char>(std::tolower(c));
            });
            const auto nameAt = lower.find_last_of('\\');
            const auto basename = std::string_view{lower}.substr(nameAt==std::string::npos ? 0 : nameAt+1);
            if (basename.find("pickaxe")!=std::string_view::npos) return GripProfile::fittedMelee;
            iron |= lower.starts_with("weapons\\iron\\") || lower.starts_with("meshes\\weapons\\iron\\");
        }
        iron |= form==0x00012EB7 || form==0x0001397E || form==0x00013790 || form==0x00013982;
    }
    return iron ? GripProfile::fittedMelee : GripProfile::standard;
}

bool throwableBase(RE::TESBoundObject& base)
{
    return base.IsInventoryObject() &&
        (!base.Is(RE::FormType::Weapon,RE::FormType::Armor) || equipmentOf(base)!=Equipment::none);
}

EquipmentFlight equipmentFlight(RE::TESBoundObject& base)
{
    if (!base.Is(RE::FormType::Weapon,RE::FormType::Armor)) return {1.f,1.f};
    const auto* armor = base.As<RE::TESObjectARMO>();
    const bool soft = armor && (armor->IsClothing() || armor->HasKeywordString("ArmorMaterialLeather") ||
        armor->HasKeywordString("ArmorMaterialHide") || armor->HasKeywordString("ArmorMaterialFur") ||
        armor->HasKeywordString("ArmorMaterialStudded") || armor->HasKeywordString("ArmorMaterialForsworn"));
    return equipmentFlight(equipmentOf(base),soft);
}

std::optional<float> ordinaryWeaponDamage(RE::PlayerCharacter& player, RE::Actor& target, RE::TESObjectREFR& object)
{
    auto* base = object.GetBaseObject();
    if (!isOneHandedWeapon(base)) return std::nullopt;
    RE::InventoryEntryData entry{base,1};
    entry.AddExtraList(&object.extraList);
    // Use the ordinary item damage from the real instance, including its
    // tempering and the player's applicable weapon skill and perks.
    const float damage = player.GetDamage(&entry);
    if (!std::isfinite(damage) || damage<0.f) return std::nullopt;
    if (!player.GetActorRuntimeData().currentProcess || !target.GetActorRuntimeData().currentProcess)
        return damage;

    // The current CommonLibSSE-NG signature includes the left-hand flag.
    // Read only the resistance ratio. Total damage can include situational
    // sneak, critical or power multipliers, which throws must never inherit.
    using Populate = void(RE::HitData*,RE::Actor*,RE::Actor*,RE::InventoryEntryData*,bool);
    static const REL::Relocation<Populate> populate{REL::ID(44001)};
    RE::HitData hit{};
    hit.flags.set(RE::HitData::Flag::kPredictDamage,RE::HitData::Flag::kIgnoreCritical);
    populate(&hit,&player,&target,&entry,false);
    return resistedSwingDamage(damage,hit.physicalDamage,hit.resistedPhysicalDamage,hit.resistedTypedDamage);
}

void applyWeaponEnchantment(RE::PlayerCharacter& player, RE::Actor& target, RE::TESObjectREFR& object)
{
    auto* base = object.GetBaseObject();
    auto* weapon = base ? base->As<RE::TESObjectWEAP>() : nullptr;
    auto* magicTarget = target.AsMagicTarget();
    if (!weapon || !magicTarget || object.IsDeleted() || object.IsDisabled()) return;
    const auto* instance = object.extraList.GetByType<RE::ExtraEnchantment>();
    auto* enchantment = instance && instance->enchantment ? instance->enchantment : weapon->formEnchanting;
    if (!enchantment || enchantment->GetDelivery()!=RE::MagicSystem::Delivery::kTouch) return;
    const float capacity = instance && instance->enchantment ? instance->charge : weapon->amountofEnchantment;
    const auto* charge = object.extraList.GetByType<RE::ExtraCharge>();
    const bool chargeLimited = capacity>0.f || charge;
    const float remaining = charge ? charge->charge : capacity;
    if (chargeLimited && (!std::isfinite(remaining) || remaining<=0.f)) return;
    const float cost = chargeLimited ? enchantment->CalculateMagickaCost(&player) : 0.f;
    if (!std::isfinite(cost) || cost<0.f) return;

    bool applied = false;
    for (auto* effect : enchantment->effects) {
        if (!effect || !effect->baseEffect || target.IsDead()) continue;
        auto magnitude = effect->GetMagnitude();
        if (!std::isfinite(magnitude)) continue;
        RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModSpellMagnitude,
                                          &player,enchantment,&target,&magnitude);
        if (!std::isfinite(magnitude)) continue;
        RE::MagicTarget::AddTargetData application{};
        application.caster = &player;
        application.magicItem = enchantment;
        application.effect = effect;
        application.source = weapon;
        application.explosionPoint = target.GetPosition();
        application.magnitude = magnitude;
        application.unk40 = 1.f;
        application.castingSource = RE::MagicSystem::CastingSource::kRightHand;
        if (magicTarget->AddTarget(application)) applied = true;
    }
    if (!applied || !chargeLimited || cost<=0.f || object.IsDeleted() || object.IsDisabled()) return;
    auto* updated = object.extraList.GetByType<RE::ExtraCharge>();
    if (!updated) {
        auto* created = new RE::ExtraCharge;
        created->charge = remaining;
        if (!object.extraList.Add(created)) {
            delete created;
            return;
        }
        updated = created;
    }
    updated->charge = std::max(0.f,updated->charge-cost);
    object.AddChange(RE::TESObjectREFR::ChangeFlags::kItemExtraData);
}
}
