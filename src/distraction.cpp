#include "distraction.hpp"
#include "physics.hpp"
#include "suspicion.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <shared_mutex>

namespace to
{
namespace
{
using Clock = std::chrono::steady_clock;
using LocationType = RE::PackageLocation::Type;
using Procedure = RE::PACKAGE_PROCEDURE_TYPE;
using WeaponState = RE::WEAPON_STATE;
using SitSleepState = RE::SIT_SLEEP_STATE;

constexpr std::size_t maxInvestigations = 32;
constexpr RE::FormID firstTravelPackage = 0x810;
constexpr RE::FormID inspectionHoldPackage = 0x830;
constexpr RE::FormID travelTemplate = 0x00016FAA;
constexpr RE::FormID holdTemplate = 0x000503D0;
constexpr std::uint32_t travelRadius = 42;

struct LandingSite {
    RE::ObjectRefHandle object;
    Vec landing;
    RE::NiPointer<RE::TESObjectREFR> marker;
};

struct PackageSlot {
    RE::TESPackage* travel{};
    RE::PackageLocation* location{};
    RE::ActorHandle owner;
    RE::FormID ownerID{};
    RE::TESPackage* offered{};
    std::shared_ptr<LandingSite> site;
    Clock::time_point reuseAfter{};
};

std::array<PackageSlot,maxInvestigations> packages;
std::shared_mutex packageMutex;
std::atomic_uint activeOffers{};
RE::TESPackage* holdPackage{};
RE::TESBoundObject* markerBase{};
RE::TESIdleForm* kneelIdle{};

struct PackageSelection {
    static RE::TESPackage* select(RE::Actor* actor)
    {
        auto* previous = original(actor);
        if (activeOffers.load(std::memory_order_acquire)==0) return previous;
        const std::shared_lock lock(packageMutex);
        const auto id = actor->GetFormID();
        for (const auto& slot : packages)
            if (slot.offered && slot.ownerID==id) return slot.offered;
        return previous;
    }

    static inline REL::Relocation<decltype(select)> original;
};

enum class Stage { leavingFurniture, walking, preparing, enteringKneel, kneeling, standing };

struct Investigation {
    PackageSlot* slot{};
    Stage stage{};
    Clock::time_point deadline{}, checkAt{};
    float standingHeight{};
    unsigned poseSamples{};
    bool resetTried{}, voicePending{}, walkingLogged{};
    bool originallyDrawn{}, sheatheRequested{}, instantTried{};
    bool getUpRequested{}, quickExitTried{};
};

struct PendingInspection {
    RE::ActorHandle actor;
    RE::ObjectRefHandle object;
    RE::FormID actorID{};
    std::shared_ptr<LandingSite> site;
    Vec quietAnchor;
    float age{}, quietTime{};
    bool alreadySpoke{};
};

std::array<Investigation,maxInvestigations> investigations;
std::array<PendingInspection,maxInvestigations> pendingInspections;

RE::FormID id(const RE::TESForm* form) { return form ? form->GetFormID() : 0; }

Clock::time_point after(float seconds)
{
    return Clock::now()+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>{seconds});
}

bool sameArea(RE::TESObjectREFR& a, RE::TESObjectREFR& b)
{
    return sameCarryArea(id(a.GetParentCell()),id(a.GetWorldspace()),id(b.GetParentCell()),id(b.GetWorldspace()));
}

bool usingFurniture(RE::Actor& actor)
{
    return actor.GetOccupiedFurniture() || actor.AsActorState()->GetSitSleepState()!=SitSleepState::kNormal;
}

std::optional<Vec> liveObjectCenter(RE::TESObjectREFR& object)
{
    const auto* root = object.Get3D();
    if (!root || object.IsDisabled() || object.IsDeleted()) return std::nullopt;
    if (auto center = liveBodyPosition(object)) return center;
    const auto center = vec(root->worldBound.center);
    return std::isfinite(center.dot(center)) ? std::optional{center} : std::nullopt;
}

void deleteMarker(LandingSite& site)
{
    if (site.marker && !site.marker->IsDeleted()) {
        if (!site.marker->IsDisabled()) site.marker->Disable();
        site.marker->SetDelete(true);
    }
    site.marker.reset();
}

void releaseMarker(const std::shared_ptr<LandingSite>& site)
{
    if (site && site.use_count()==1) deleteMarker(*site);
}

RE::TESObjectREFR* pathTarget(LandingSite& site, RE::TESObjectREFR& object)
{
    if ((vec(object.GetPosition())-site.landing).length()<=18.f) return &object;
    // A loose reference's stored position can lag behind its Havok body.
    if (!site.marker) {
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* cell = object.GetParentCell();
        if (!data || !markerBase || !cell) return nullptr;
        const auto destination = point(site.landing);
        auto* world = object.GetWorldspace();
        if (cell->IsExteriorCell()) {
            auto* tes = RE::TES::GetSingleton();
            cell = tes ? tes->GetCell(destination) : nullptr;
            if (!cell || !cell->IsExteriorCell() || cell->GetRuntimeData().worldSpace!=world) return nullptr;
        }
        if (!cell->IsAttached()) return nullptr;
        site.marker = data->CreateReferenceAtLocation(markerBase,destination,{},cell,world,
                                                       nullptr,nullptr,{},false,true).get();
    }
    return site.marker && !site.marker->IsDisabled() && !site.marker->IsDeleted() ? site.marker.get() : nullptr;
}

const RE::TESCustomPackageData* customData(const RE::TESPackage& package)
{
    if (package.packData.packType!=Procedure::kPackage) return nullptr;
    return skyrim_cast<const RE::TESCustomPackageData*>(package.data);
}

RE::PackageLocation* packageLocation(RE::TESPackage& package, RE::FormID templateID)
{
    const auto* custom = customData(package);
    if (!custom || id(custom->templateParent)!=templateID || !custom->data.data || !custom->data.uids) return nullptr;
    const auto& inputs = custom->data;
    for (std::uint16_t i=0;i<inputs.dataSize;++i) {
        if (inputs.uids[i]!=0 || !inputs.data[i] || inputs.data[i]->GetTypeName()!="Location") continue;
        if (auto* input = skyrim_cast<RE::BGSPackageDataLocation*>(inputs.data[i]))
            return input->pointer;
    }
    return nullptr;
}

bool runningPackage(RE::Actor& actor, const RE::TESPackage* package)
{
    const auto* process = actor.GetActorRuntimeData().currentProcess;
    return process && package && (process->currentPackage.package==package ||
        (process->middleHigh && process->middleHigh->runOncePackage.package==package));
}

void collectRetiredPackages(bool reset = false)
{
    const auto now = Clock::now();
    for (auto& slot : packages) {
        if (!slot.ownerID || slot.offered || (!reset && now<slot.reuseAfter)) continue;
        auto actor = slot.owner.get();
        if (actor && runningPackage(*actor,slot.travel)) {
            if (!reset || actor->IsInCombat() || actor->GetCurrentScene()) continue;
            actor->EvaluatePackage(true,true);
            if (runningPackage(*actor,slot.travel)) {
                SKSE::log::warn("Investigation {:08X}: engine still holds released travel package {:08X}",
                                slot.ownerID,id(slot.travel));
                continue;
            }
        }
        slot.location->locType = LocationType::kNearSelf;
        slot.location->data.object = nullptr;
        releaseMarker(slot.site);
        slot.site.reset();
        const std::unique_lock lock(packageMutex);
        slot.owner = {};
        slot.ownerID = 0;
    }
}

void offerPackage(PackageSlot& slot, RE::TESPackage* package)
{
    const std::unique_lock lock(packageMutex);
    const bool hadOffer = slot.offered!=nullptr;
    slot.offered = package;
    if (!hadOffer && package) activeOffers.fetch_add(1,std::memory_order_release);
    if (hadOffer && !package) activeOffers.fetch_sub(1,std::memory_order_release);
}

void stopKneeling(RE::Actor& actor, bool reset)
{
    if (!actor.Is3DLoaded() || actor.IsDead() || actor.IsDisabled() || actor.GetOccupiedFurniture()) return;
    if (reset && !actor.IsInCombat() && !actor.GetCurrentScene())
        actor.NotifyAnimationGraph("IdleForceDefaultState");
    else
        actor.NotifyAnimationGraph("IdleFurnitureExit");
}

void finish(Investigation& investigation, RE::Actor* actor)
{
    auto& slot = *investigation.slot;
    auto* released = slot.offered;
    offerPackage(slot,nullptr);
    slot.reuseAfter = after(.18f);
    if (actor) {
        if (investigation.stage==Stage::enteringKneel || investigation.stage==Stage::kneeling ||
            investigation.stage==Stage::standing)
            stopKneeling(*actor,true);
        if (released && actor->GetActorRuntimeData().currentProcess && !actor->IsDead() &&
            !actor->IsDisabled() && !actor->IsInCombat() && !actor->GetCurrentScene()) {
            actor->EvaluatePackage(true,false);
            if (actor->GetCurrentPackage()==released) actor->EvaluatePackage(true,true);
            if (investigation.originallyDrawn && investigation.sheatheRequested && canInvestigate(*actor) &&
                !actor->GetOccupiedFurniture() &&
                actor->AsActorState()->GetWeaponState()==WeaponState::kSheathed)
                actor->DrawWeaponMagicHands(true);
        }
        SKSE::log::info("Investigation {:08X}: released temporary package {:08X}, current {:08X}",
                        slot.ownerID,id(released),id(actor->GetCurrentPackage()));
    }
    investigation = {};
}

void prepareKneel(Investigation& investigation, RE::Actor& actor)
{
    offerPackage(*investigation.slot,holdPackage);
    actor.EvaluatePackage(true,false);
    actor.StopMoving(1.f);
    investigation.stage = Stage::preparing;
    investigation.deadline = after(2.60f);
    investigation.checkAt = after(.28f);
    investigation.resetTried = false;
    SKSE::log::info("Investigation {:08X}: preparing to kneel, hold package {:08X}",
                    actor.GetFormID(),id(holdPackage));
}

float pelvisHeight(RE::Actor& actor)
{
    auto* root = actor.Get3D();
    if (!root) return 0.f;
    const auto* pelvis = root->GetObjectByName("NPC Pelvis [Pelv]");
    if (!pelvis) pelvis = root->GetObjectByName("Bip01 Pelvis");
    return pelvis ? pelvis->world.translate.z-root->world.translate.z : 0.f;
}

bool requestKneel(Investigation& investigation, RE::Actor& actor, bool instant)
{
    const auto offset = investigation.slot->site->landing-vec(actor.GetPosition());
    if (offset.x*offset.x+offset.y*offset.y>1.f) actor.SetRotationZ(std::atan2(offset.x,offset.y));
    actor.StopMoving(1.f);
    const auto* eventName = instant ? "IdleKneelingEnterInstant" : "IdleKneelingEnter";
    bool accepted = actor.NotifyAnimationGraph(eventName);
    if (!accepted && !instant && kneelIdle) {
        auto* process = actor.GetActorRuntimeData().currentProcess;
        accepted = process && process->PlayIdle(&actor,kneelIdle,nullptr);
    }
    investigation.stage = Stage::enteringKneel;
    investigation.deadline = after(instant ? .85f : 1.65f);
    investigation.checkAt = after(.08f);
    investigation.poseSamples = 0;
    SKSE::log::info("Investigation {:08X}: {} {}, waiting for kneeling pose",
                    actor.GetFormID(),eventName,accepted ? "accepted" : "declined");
    return accepted;
}

void startKneelHold(Investigation& investigation, RE::Actor& actor, KneelDuration duration)
{
    static std::mt19937 random{std::random_device{}()};
    const float seconds = std::uniform_real_distribution<float>{duration.minimum,duration.maximum}(random);
    investigation.deadline = after(seconds);
    investigation.checkAt = after(.45f);
    investigation.stage = Stage::kneeling;
    SKSE::log::info("Investigation {:08X}: kneeling pose reached, hold {:.1f} s",
                    actor.GetFormID(),seconds);
}

void startMovement(Investigation& investigation, RE::Actor& actor, RE::TESObjectREFR& object, bool beside)
{
    auto& slot = *investigation.slot;
    if (!beside) {
        auto* destination = pathTarget(*slot.site,object);
        const auto handle = destination ? destination->CreateRefHandle() : RE::ObjectRefHandle{};
        if (!handle) {
            SKSE::log::info("Investigation {:08X}: no valid travel destination",actor.GetFormID());
            finish(investigation,&actor);
            return;
        }
        slot.location->rad = travelRadius;
        slot.location->data.refHandle = handle;
        slot.location->locType = LocationType::kNearReference;
    }
    // Idle-marker activities can have a running idle without an occupied
    // furniture reference. End that idle before assigning the approach.
    if (auto* process = actor.GetActorRuntimeData().currentProcess;
        process && process->high && process->high->currentProcessIdle)
        process->StopCurrentIdle(&actor,true);
    if (beside) {
        prepareKneel(investigation,actor);
        return;
    }
    investigation.stage = Stage::walking;
    investigation.deadline = after(16.f);
    investigation.checkAt = after(.35f);
    offerPackage(slot,slot.travel);
    SKSE::log::info("Investigation {:08X}: offering travel package {:08X} over {:08X} for object {:08X}",
                    actor.GetFormID(),id(slot.travel),id(actor.GetCurrentPackage()),object.GetFormID());
    actor.EvaluatePackage(true,false);
}

void startInvestigation(Investigation& investigation, RE::Actor& actor, RE::TESObjectREFR& object,
                        std::shared_ptr<LandingSite> site, bool beside, bool alreadySpoke)
{
    auto free = std::find_if(packages.begin(),packages.end(),[](const auto& slot) {
        return slot.location && !slot.ownerID;
    });
    if (free==packages.end()) {
        releaseMarker(site);
        return;
    }
    {
        const std::unique_lock lock(packageMutex);
        free->owner = actor.GetHandle();
        free->ownerID = actor.GetFormID();
    }
    free->site = std::move(site);
    investigation.slot = &*free;
    investigation.voicePending = !alreadySpoke;
    investigation.originallyDrawn = actor.AsActorState()->GetWeaponState()==WeaponState::kDrawn;
    if (!usingFurniture(actor)) {
        startMovement(investigation,actor,object,beside);
        return;
    }
    // Let Skyrim's get-up package run while our hold is ready to win the
    // next ordinary evaluation. This prevents sandboxing from reusing the station.
    offerPackage(*free,holdPackage);
    investigation.stage = Stage::leavingFurniture;
    investigation.checkAt = after(3.25f);
    investigation.deadline = after(7.f);
}

void updateFurnitureExit(Investigation& investigation, RE::Actor& actor, RE::TESObjectREFR& object,
                         Clock::time_point now)
{
    const auto furniture = actor.GetOccupiedFurniture();
    const auto state = actor.AsActorState()->GetSitSleepState();
    if (!furniture && state==SitSleepState::kNormal) {
        SKSE::log::info("Investigation {:08X}: furniture released",actor.GetFormID());
        startMovement(investigation,actor,object,
                      besideForInspection(vec(actor.GetPosition()),investigation.slot->site->landing));
        return;
    }
    if (now>=investigation.deadline) {
        SKSE::log::info("Investigation {:08X}: furniture exit timed out",actor.GetFormID());
        finish(investigation,&actor);
        return;
    }
    const bool gettingUp = state==SitSleepState::kWantToStand || state==SitSleepState::kWantToWake;
    if (furniture && !gettingUp && !investigation.getUpRequested && !investigation.quickExitTried) {
        investigation.getUpRequested = true;
        actor.InitiateGetUpPackage();
        SKSE::log::info("Investigation {:08X}: requested native furniture exit",actor.GetFormID());
    } else if (now>=investigation.checkAt && !investigation.quickExitTried) {
        investigation.quickExitTried = true;
        actor.StopInteractingQuick(true);
        SKSE::log::info("Investigation {:08X}: requested quick furniture exit",actor.GetFormID());
    }
}

void updatePending(RE::PlayerCharacter& player, float dt)
{
    for (auto& pending : pendingInspections) {
        if (!pending.actorID) continue;
        pending.age += dt;
        auto actor = pending.actor.get();
        auto object = pending.object.get();
        const auto center = object ? liveObjectCenter(*object) : std::nullopt;
        if (!actor || !center || actor->IsDead() || actor->IsInCombat() || actor->GetCurrentScene() ||
            isInvestigating(*actor) || !sameArea(player,*actor) || !sameArea(*actor,*object) || pending.age>6.f) {
            SKSE::log::info("Investigation {:08X}: canceled before package selection",pending.actorID);
            releaseMarker(pending.site);
            pending = {};
            continue;
        }
        if ((*center-pending.quietAnchor).length()<=4.f) pending.quietTime += dt;
        else {
            pending.quietAnchor = *center;
            pending.quietTime = 0.f;
        }
        const auto landing = pending.site && (*center-pending.site->landing).length()<=30.f ?
            pending.site->landing : *center;
        const bool beside = besideForInspection(vec(actor->GetPosition()),landing);
        bool staggering{};
        actor->GetGraphVariableBool("IsStaggering",staggering);
        const auto route = inspectionRoute(pending.age,pending.quietTime,beside,staggering);
        if (route==InspectionRoute::wait || !canInvestigate(*actor)) continue;

        const auto free = std::find_if(investigations.begin(),investigations.end(),[](const auto& entry) { return !entry.slot; });
        if (free==investigations.end()) continue;
        auto site = std::move(pending.site);
        if (!site || (site->landing-landing).length()>4.f) {
            releaseMarker(site);
            site = std::make_shared<LandingSite>(LandingSite{pending.object,landing});
        }
        startInvestigation(*free,*actor,*object,std::move(site),route==InspectionRoute::kneel,pending.alreadySpoke);
        pending = {};
    }
}
}

void loadInvestigationIdle()
{
    auto* handler = RE::TESDataHandler::GetSingleton();
    if (!handler) return;
    std::size_t loaded{};
    for (std::size_t i=0;i<packages.size();++i) {
        auto* package = handler->LookupForm<RE::TESPackage>(firstTravelPackage+static_cast<RE::FormID>(i),"ThrowObject.esm");
        if (!package || package->IsDeleted()) continue;
        auto* location = packageLocation(*package,travelTemplate);
        if (!location || location->locType!=LocationType::kNearSelf) continue;
        packages[i].travel = package;
        packages[i].location = location;
        ++loaded;
    }
    using Flag = RE::PACKAGE_DATA::GeneralFlag;
    if (auto* package = handler->LookupForm<RE::TESPackage>(inspectionHoldPackage,"ThrowObject.esm");
        package && !package->IsDeleted() &&
        !package->packData.packFlags.any(Flag::kWeaponDrawn,Flag::kWeaponsUnequipped,Flag::kIgnoreCombat)) {
        if (const auto* location = packageLocation(*package,holdTemplate);
            location && location->locType==LocationType::kNearPackageStartLocation)
            holdPackage = package;
    }
    markerBase = RE::TESForm::LookupByID<RE::TESObjectSTAT>(0x0000003B); // XMarker
    if (markerBase && markerBase->IsDeleted()) markerBase = nullptr;
    kneelIdle = RE::TESForm::LookupByID<RE::TESIdleForm>(0x000E8E52); // IdleKneelingEnter
    if (kneelIdle && kneelIdle->IsDeleted()) kneelIdle = nullptr;
    SKSE::log::info("Investigation setup: {}/{} travel packages, neutral hold {:08X}, kneel fallback {:08X}",
                    loaded,packages.size(),id(holdPackage),id(kneelIdle));
    if (!loaded || !holdPackage) {
        SKSE::log::warn("Investigation package setup incomplete; check ThrowObject.esm and Skyrim.esm");
        return;
    }
    // Skyrim asks Character for its top alias package before its regular list.
    // Chain the installed handler and answer only for a currently claimed actor.
    REL::Relocation<std::uintptr_t> character{RE::VTABLE_Character[0]};
    PackageSelection::original = character.write_vfunc(0x49,PackageSelection::select);
}

bool canInvestigate(RE::Actor& actor)
{
    const auto* process = actor.GetActorRuntimeData().currentProcess;
    const auto* state = actor.AsActorState();
    auto* root = actor.Get3D();
    const bool humanoid = root && (actor.IsHumanoid() || root->GetObjectByName("NPC R Hand [RHnd]"));
    if (!humanoid || !process || !process->high || actor.IsDisabled() || actor.IsDead() ||
        actor.IsGhost() || actor.IsInCombat() || actor.GetCurrentScene() || actor.IsInKillMove() ||
        actor.IsInRagdollState() || process->IsArrested() || state->IsSwimming() || state->IsUnconscious())
        return false;
    RE::NiPointer<RE::Actor> mount;
    return !actor.IsOnMount() || !actor.GetMount(mount);
}

bool isInvestigating(const RE::Actor& actor)
{
    return std::any_of(investigations.begin(),investigations.end(),[&](const auto& item) {
        return item.slot && item.slot->ownerID==actor.GetFormID();
    });
}

bool investigationsActive()
{
    return std::any_of(packages.begin(),packages.end(),[](const auto& slot) { return slot.ownerID!=0; }) ||
        std::any_of(pendingInspections.begin(),pendingInspections.end(),[](const auto& item) { return item.actorID!=0; });
}

void queueNearbyInspection(RE::Actor& actor, RE::TESObjectREFR& object, bool alreadySpoke)
{
    if (!PackageSelection::original.address() || actor.IsDead() || actor.IsInCombat() ||
        actor.GetCurrentScene() || isInvestigating(actor)) return;
    const auto id = actor.GetFormID();
    if (std::any_of(pendingInspections.begin(),pendingInspections.end(),[&](const auto& item) { return item.actorID==id; }))
        return;
    const auto free = std::find_if(pendingInspections.begin(),pendingInspections.end(),[](const auto& item) { return !item.actorID; });
    if (free==pendingInspections.end()) return;
    const auto center = liveObjectCenter(object);
    if (!center) return;
    *free = {actor.GetHandle(),object.GetHandle(),id,{},*center,0.f,0.f,alreadySpoke};
    SKSE::log::info("Investigation {:08X}: queued object {:08X}",id,object.GetFormID());
}

void cancelInspectionForHit(RE::Actor& actor, RE::TESObjectREFR& object)
{
    const auto actorID = actor.GetFormID();
    const auto handle = object.GetHandle();
    for (auto& pending : pendingInspections) {
        if (pending.actorID!=actorID || pending.object!=handle) continue;
        releaseMarker(pending.site);
        pending = {};
    }
    for (auto& active : investigations) {
        if (active.slot && active.slot->ownerID==actorID && active.slot->site &&
            active.slot->site->object==handle) finish(active,&actor);
    }
}

void investigateLanding(RE::TESObjectREFR& object, Vec landing, std::span<const RE::ActorHandle> witnesses,
                        RE::FormID previousSpeaker)
{
    if (!object.Is3DLoaded() || object.IsDisabled() || !std::isfinite(landing.length())) return;
    const auto site = std::make_shared<LandingSite>(LandingSite{object.GetHandle(),landing});
    for (const auto& witness : witnesses) {
        auto actor = witness.get();
        if (!actor || isInvestigating(*actor) || !sameArea(*actor,object)) continue;
        const auto actorID = actor->GetFormID();
        const bool spoke = actorID==previousSpeaker;
        queueNearbyInspection(*actor,object,spoke);
        const auto queued = std::find_if(pendingInspections.begin(),pendingInspections.end(),[&](const auto& entry) {
            return entry.actorID==actorID && entry.object==site->object;
        });
        if (queued!=pendingInspections.end()) {
            releaseMarker(queued->site);
            queued->site = site;
            queued->alreadySpoke |= spoke;
        }
    }
}

void updateInvestigations(RE::PlayerCharacter& player, float dt, KneelDuration duration)
{
    collectRetiredPackages();
    updatePending(player,dt);
    const auto now = Clock::now();
    int voiceStarts = 2;
    std::optional<bool> hidden;
    for (auto& investigation : investigations) {
        if (!investigation.slot) continue;
        auto& slot = *investigation.slot;
        auto actor = slot.owner.get();
        auto object = slot.site->object.get();
        if (!actor || !object || !object->Is3DLoaded() || object->IsDisabled() || object->IsDeleted() ||
            !canInvestigate(*actor) || !sameArea(player,*actor) || !sameArea(*actor,*object) ||
            (investigation.stage!=Stage::leavingFurniture && actor->GetOccupiedFurniture())) {
            finish(investigation,actor.get());
            continue;
        }
        if (investigation.voicePending && voiceStarts>0) {
            --voiceStarts;
            investigation.voicePending = false;
            if (!hidden) hidden = hiddenSneaking(player);
            if (*hidden) playSuspicion(*actor,true);
        }
        const auto* current = actor->GetCurrentPackage();
        const bool ownsMovement = current==slot.offered;
        switch (investigation.stage) {
        case Stage::leavingFurniture:
            updateFurnitureExit(investigation,*actor,*object,now);
            break;
        case Stage::walking: {
            const auto offset = slot.site->landing-vec(actor->GetPosition());
            if (ownsMovement && !investigation.walkingLogged) {
                investigation.walkingLogged = true;
                SKSE::log::info("Investigation {:08X}: travel package selected, distance {:.0f}",slot.ownerID,offset.length());
            }
            if (offset.x*offset.x+offset.y*offset.y<=60.f*60.f && std::abs(offset.z)<=48.f) {
                prepareKneel(investigation,*actor);
            } else if (now>=investigation.deadline) {
                SKSE::log::info("Investigation {:08X}: approach timed out, package {:08X}, distance {:.0f}",
                                slot.ownerID,id(current),offset.length());
                finish(investigation,actor.get());
            } else if (now>=investigation.checkAt && !ownsMovement && !investigation.resetTried) {
                investigation.resetTried = true;
                actor->EvaluatePackage(true,true);
                SKSE::log::info("Investigation {:08X}: forced travel evaluation, now on {:08X}",
                                slot.ownerID,id(actor->GetCurrentPackage()));
            }
            break;
        }
        case Stage::preparing: {
            if (now<investigation.checkAt) break;
            if (now>=investigation.deadline) {
                SKSE::log::info("Investigation {:08X}: could not reach a stationary, sheathed inspection stance",
                                slot.ownerID);
                finish(investigation,actor.get());
                break;
            }
            if (!ownsMovement && !investigation.resetTried) {
                investigation.resetTried = true;
                actor->EvaluatePackage(true,true);
                actor->StopMoving(1.f);
                investigation.checkAt = after(.28f);
                break;
            }
            const auto weapon = actor->AsActorState()->GetWeaponState();
            if (weapon!=WeaponState::kSheathed) {
                if (weapon==WeaponState::kDrawn && !investigation.sheatheRequested) {
                    investigation.sheatheRequested = true;
                    actor->DrawWeaponMagicHands(false);
                    SKSE::log::info("Investigation {:08X}: waiting for weapon to sheathe",slot.ownerID);
                }
                break;
            }
            if (!ownsMovement) break;
            if (actor->IsMoving()) {
                actor->StopMoving(1.f);
                break;
            }
            investigation.standingHeight = pelvisHeight(*actor);
            if (!std::isfinite(investigation.standingHeight) || investigation.standingHeight<=0.f) {
                SKSE::log::info("Investigation {:08X}: no usable pelvis node for kneeling pose",slot.ownerID);
                finish(investigation,actor.get());
            } else if (!requestKneel(investigation,*actor,false)) {
                investigation.instantTried = true;
                if (!requestKneel(investigation,*actor,true)) finish(investigation,actor.get());
            }
            break;
        }
        case Stage::enteringKneel: {
            if (actor->AsActorState()->GetWeaponState()!=WeaponState::kSheathed) {
                finish(investigation,actor.get());
                break;
            }
            if (now<investigation.checkAt) break;
            investigation.checkAt = after(.08f);
            const float height = pelvisHeight(*actor);
            if (kneelingPose(investigation.standingHeight,height) && !actor->IsMoving()) ++investigation.poseSamples;
            else investigation.poseSamples = 0;
            if (investigation.poseSamples>=2) {
                startKneelHold(investigation,*actor,duration);
            } else if (now>=investigation.deadline) {
                if (investigation.instantTried) {
                    SKSE::log::info("Investigation {:08X}: no kneeling pose ({:.1f} to {:.1f})",
                                    slot.ownerID,investigation.standingHeight,height);
                    finish(investigation,actor.get());
                } else {
                    investigation.instantTried = true;
                    if (!requestKneel(investigation,*actor,true)) finish(investigation,actor.get());
                }
            }
            break;
        }
        case Stage::kneeling:
            if (now>=investigation.deadline) {
                stopKneeling(*actor,false);
                investigation.stage = Stage::standing;
                investigation.deadline = after(.60f);
            } else if (!ownsMovement && now>=investigation.checkAt && actor->IsMoving()) {
                investigation.checkAt = after(.45f);
                actor->EvaluatePackage(true,false);
                actor->StopMoving(1.f);
            }
            break;
        case Stage::standing:
            if (now>=investigation.deadline) finish(investigation,actor.get());
            break;
        }
    }
}

void clearInvestigations()
{
    for (auto& pending : pendingInspections) {
        releaseMarker(pending.site);
        pending = {};
    }
    for (auto& investigation : investigations) {
        if (!investigation.slot) continue;
        auto actor = investigation.slot->owner.get();
        finish(investigation,actor.get());
    }
    collectRetiredPackages(true);
    // On save or load, also remove waypoints held by a displaced package
    // while it waits for the engine to relinquish its last reference.
    for (auto& slot : packages)
        if (slot.site) deleteMarker(*slot.site);
}

void recoverInvestigationPackages()
{
    auto* processes = RE::ProcessLists::GetSingleton();
    if (!processes || !PackageSelection::original.address()) return;
    processes->ForEachHighActor([](RE::Actor* actor) {
        using Result = RE::BSContainer::ForEachResult;
        if (!actor || actor->IsDead() || actor->IsInCombat() || actor->GetCurrentScene()) return Result::kContinue;
        const bool orphaned = runningPackage(*actor,holdPackage) || std::any_of(packages.begin(),packages.end(),[&](const auto& slot) {
            return runningPackage(*actor,slot.travel);
        });
        if (orphaned) {
            actor->EvaluatePackage(true,true);
            SKSE::log::info("Investigation {:08X}: re-evaluated an investigation package found on load",actor->GetFormID());
        }
        return Result::kContinue;
    });
}
}
