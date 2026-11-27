#include "physics.hpp"
#include "distraction.hpp"
#include "input_filter.hpp"
#include "gameplay_input.hpp"
#include "equipment_game.hpp"
#include "held_recovery.hpp"
#include "carry_visual_game.hpp"
#include "weapon_grip.hpp"

#include <atomic>
#include <filesystem>
#include <numbers>
#include <fstream>
#include <mutex>
#include <optional>
#include <spdlog/sinks/base_sink.h>

namespace to
{
namespace
{
constexpr REL::Version runtime{1,6,1170,0};
constexpr RE::FormID carryGlobal = 0x800;
constexpr std::uint32_t heldRecord = 0x444C4548;  // HELD
constexpr std::uint32_t serializationID = 0x4A424F54;  // TOBJ
constexpr std::uint32_t heldRecordVersion = 1;
constexpr char handName[] = "NPC R Hand [RHnd]";
constexpr std::array<std::string_view,6> clips{
    "PickUp","Hold","Throw","CarryWalk","CarryJog","CarrySprint"
};
constexpr std::array<std::string_view,5> extraMenus{
    "Dialogue Menu","InventoryMenu","Journal Menu","MapMenu","Crafting Menu"
};

struct Held {
    RE::NiPointer<RE::TESObjectREFR> target;
    RE::FormID id{}, base{}, cell{}, world{};
    RE::NiPointer<RE::NiAVObject> mesh;
    RE::NiPointer<RE::NiNode> socket;
    RE::NiPointer<RE::NiNode> parent;
    RE::NiPoint3 origin{}, boundsCenter{}, boundsHalf{};
    std::optional<WeaponGrip> weaponGrip;
    GripProfile gripProfile{GripProfile::standard};
    float radius{}, area{}, scale{1.f}, gripPitch{}, gripYaw{}, weaponWorldScale{1.f};
    bool parked{}, suspended{}, sneakingThrow{}, dagger{};
};
struct PendingPhysics {
    RE::ObjectRefHandle handle;
    RE::FormID cell{}, world{};
    Vec velocity;
    float area{}, elapsed{};
    bool sneakingThrow{}, hiddenAtRelease{};
};

Held item;
Recovery restore;
PendingPhysics pending;
Phase phase{Phase::idle};
RE::TESGlobal* carry{};
bool ready{}, firstAtStart{};
float elapsed{};
std::atomic_uint8_t markers{};
GameplayInput inputs;
std::uint64_t carryGeneration{};
RE::FormID nativeGrabID{};
bool inputHookInstalled{};
bool actionStarted{};

bool firstPerson()
{
    auto* camera = RE::PlayerCamera::GetSingleton();
    return camera && camera->IsInFirstPerson();
}
RE::FormID id(const RE::TESForm* form) { return form ? form->GetFormID() : 0; }

void carryPose(bool active)
{
    const float value = active ? 1.f : 0.f;
    if (carry && carry->value!=value) carry->value = value;
}
bool graph(RE::PlayerCharacter& player, const char* name)
{
    const bool accepted = player.NotifyAnimationGraph(RE::BSFixedString(name));
    SKSE::log::info("Animation Injector event {}: {}",name,accepted ? "accepted" : "not handled");
    return accepted;
}
void detachSocket()
{
    if (item.parent && item.socket && item.socket->parent==item.parent.get())
        item.parent->DetachChild(item.socket.get());
    item.parent.reset();
}
void detach()
{
    detachSocket();
    item.socket.reset();
    item.mesh.reset();
}

void suspendCarry(RE::PlayerCharacter& player, const char* reason)
{
    if (!item.suspended) SKSE::log::info("Pausing carry visual for {:08X}: {}",item.id,reason);
    item.suspended = true;
    carryPose(false);
    detachSocket();
    if (phase==Phase::lifting || phase==Phase::throwing) {
        phase = Phase::held;
        elapsed = 0;
        actionStarted = false;
        markers.store(0);
        if (!firstAtStart) graph(player,"TO_Cancel");
    }
}
void reset(RE::PlayerCharacter* player, bool cancelClip, bool unpark = true)
{
    const auto previous = phase;
    phase = transition(phase,Event::cancel);
    elapsed = 0;
    actionStarted = false;
    markers.store(0);
    carryPose(false);
    detach();
    if (unpark && item.parked) {
        if (auto ref = item.target.get(); ref && !ref->IsDeleted() && id(ref->GetBaseObject())==item.base) {
            if (ref->IsDisabled()) {
                SKSE::log::info("Restoring parked world reference {:08X} after cancellation",item.id);
                ref->Enable(false);
                if (ref->IsDisabled()) restore = {item.id,item.base};
            }
        } else {
            restore = {item.id,item.base};
        }
    }
    item = {};
    if (cancelClip && player && previous!=Phase::idle && !firstAtStart) graph(*player,"TO_Cancel");
}
void recover(bool discardStale)
{
    if (!restore.id) return;
    const auto saved = restore;
    auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(saved.id);
    const auto result = recoverHeldReference(restore,ref ? id(ref->GetBaseObject()) : 0,
                                             ref && ref->IsDeleted(),discardStale,[ref] {
        if (ref->IsDisabled()) ref->Enable(false);
        return !ref->IsDisabled();
    });
    if (result==RecoveryResult::stale)
        SKSE::log::warn("Discarded stale held reference {:08X} (expected base {:08X})",saved.id,saved.base);
    if (result==RecoveryResult::restored)
        SKSE::log::info("Recovered held world reference {:08X} from save",saved.id);
}

const char* gameplayBlock(RE::PlayerCharacter* player)
{
    if (!player || !player->Is3DLoaded()) return "player 3D unavailable";
    auto* ui = RE::UI::GetSingleton();
    if (!ui || ui->GameIsPaused()) return "game paused or UI unavailable";
    if (ui->IsMenuOpen(RE::Console::MENU_NAME)) return "console open";
    for (auto name : extraMenus)
        if (ui->IsMenuOpen(name)) return "game menu open";
    if (player->IsGrabbing()) return "Skyrim is holding a world reference";
    if (player->IsDead() || player->IsInRagdollState() || player->IsInKillMove())
        return "player unable to act";
    if (player->AsActorState()->IsSwimming()) return "player swimming";
    if (player->AsActorState()->IsWeaponDrawn()) return "weapon drawn";
    if (player->GetCurrentScene() || player->GetOccupiedFurniture()) return "player in scene or furniture";
    return nullptr;
}

const char* pickupBlock(RE::PlayerCharacter& player, RE::TESObjectREFR* ref, const Settings& settings)
{
    if (!ref) return "no crosshair world reference";
    if (ref->IsDisabled() || ref->IsDeleted() || ref->IsActivationBlocked()) return "reference unavailable";
    if (!sameCarryArea(id(ref->GetParentCell()),id(ref->GetWorldspace()),
                       id(player.GetParentCell()),id(player.GetWorldspace()))) return "reference outside the player's area";
    auto* base = ref->GetBaseObject();
    if (!base || !throwableBase(*base))
        return "base form is not an eligible inventory object";
    if (ref->HasQuestObject()) return "engine marks reference as quest-bound";
    auto* root = ref->Get3D();
    const bool loose = ref->IsDynamicForm();
    auto* body = uniqueBody(root);
    if (!body || !canHandleBody(*body,loose,base)) return "no supported single Havok body";
    // Placed inventory drops retain a runtime reference, including after
    // a release or reload. Their loaded clutter body is the movement check.
    if (!loose && !ref->CanBeMoved())
        return "engine marks the placed reference as immovable";
    const auto feet = vec(player.GetPosition());
    const auto current = livePickupDistance(*ref,*body,*root,feet);
    if (!current) return "live pickup position unavailable";
    const auto stored = (vec(ref->GetPosition())-feet).length();
    if (settings.log || *current>settings.pickupDistance || stored>settings.pickupDistance)
        SKSE::log::info("Pickup reach {:08X}: live {:.1f}, stored origin {:.1f}, limit {:.1f} game units",
                       id(ref),*current,stored,settings.pickupDistance);
    return *current<=settings.pickupDistance ? nullptr : "out of pickup range at live body";
}

RE::NiNode* anchor(RE::PlayerCharacter& player)
{
    const bool first = firstPerson();
    if (auto* actor = player.Get3D(first)) {
        if (auto* hand = actor->GetObjectByName(handName))
            if (auto* node = hand->AsNode()) return node;
    }
    // A sheathed first-person skeleton can have its hand node unloaded.
    auto* camera = RE::PlayerCamera::GetSingleton();
    return first && camera ? camera->cameraRoot.get() : nullptr;
}

float hoverScale(const Settings& settings)
{
    return item.scale * std::min(settings.handSize,36.0f/std::max(item.radius,36.0f));
}

void placeWeaponVisual(RE::NiNode& hand, const Settings& settings, const WeaponGrip& grip,
                       const std::optional<RE::NiPoint3>& palmOpening, float yaw)
{
    RE::NiTransform pose;
    if (const auto* weapon = hand.GetObjectByName("Weapon"); weapon && weapon->parent==&hand) {
        pose = weapon->local;
    } else {
        // Match the palm socket in the shipped third-person hold pose.
        pose.translate = {0.f,-3.1f,7.2f};
        pose.rotate = RE::NiMatrix3(RE::NiPoint3{0,1,0},{0,0,1},{1,0,0});
    }
    if (item.gripProfile==GripProfile::staff || item.gripProfile==GripProfile::spear) {
        const RE::NiPoint3 direction{.23f*std::cos(yaw)+.12f*std::sin(yaw),
                                     -.23f*std::sin(yaw)+.12f*std::cos(yaw),1.f};
        const auto [x,y,z] = shaftGripRotation(vec(hand.world.rotate.Transpose()*direction));
        pose.rotate = RE::NiMatrix3(point(x),point(y),point(z));
    } else if (item.dagger) {
        Vec forearm;
        if (const auto* elbow = hand.parent)
            forearm = vec(hand.world.rotate.Transpose()*(hand.world.translate-elbow->world.translate));
        const auto [x,y,z] = daggerGripRotation(forearm);
        pose.rotate = RE::NiMatrix3(point(x),point(y),point(z));
    }
    if (palmOpening) pose.translate = *palmOpening;
    const auto& [right,shaft,up] = grip.rows;
    constexpr float radians = std::numbers::pi_v<float>/180.f;
    RE::NiMatrix3 angle;
    angle.EulerAnglesToAxesZXY(settings.angles[0]*radians,settings.angles[1]*radians,settings.angles[2]*radians);
    pose.rotate = angle*pose.rotate*RE::NiMatrix3(point(right),point(shaft),point(up));
    pose.scale = weaponVisualLocalScale(item.weaponWorldScale,hand.world.scale);
    const Settings defaults;
    pose.translate += RE::NiPoint3{settings.xyz[0]-defaults.xyz[0],settings.xyz[1]-defaults.xyz[1],
                                   settings.xyz[2]-defaults.xyz[2]};
    pose.translate -= pose.rotate*point(grip.handle*pose.scale);
    // The original socket still supplies launch position and orientation.
    item.mesh->local = item.socket->local.Invert()*pose;
}

bool attach(RE::PlayerCharacter& player, const Settings& settings)
{
    auto* node = anchor(player);
    if (!node || !item.mesh) return false;
    if (!item.socket) {
        item.socket.reset(RE::NiNode::Create(1));
        if (!item.socket) return false;
        item.socket->name = "ThrowObjectSocket";
        item.socket->AttachChild(item.mesh.get());
        if (item.mesh->parent!=item.socket.get()) {
            item.socket.reset();
            return false;
        }
    }
    const auto* camera = RE::PlayerCamera::GetSingleton();
    const bool onCamera = camera && camera->cameraRoot.get()==node;
    const float reach = std::clamp(item.radius * 0.30f,0.f,9.f);
    item.socket->local.translate = onCamera ?
        RE::NiPoint3{19.f+settings.xyz[0],37.f+settings.xyz[1]+reach,-21.f+settings.xyz[2]} :
        RE::NiPoint3{settings.xyz[0],settings.xyz[1]+reach,settings.xyz[2]};
    constexpr float radians = std::numbers::pi_v<float>/180.f;
    item.socket->local.rotate.EulerAnglesToAxesZXY(settings.angles[0]*radians+item.gripPitch,
                                                  settings.angles[1]*radians,
                                                  settings.angles[2]*radians+item.gripYaw);
    item.socket->local.scale = 1.f;
    item.mesh->local = RE::NiTransform{};
    item.mesh->local.scale = hoverScale(settings);
    const RE::NiPoint3 up{0.f,0.f,1.f};
    const auto worldToHand = node->world.rotate.Transpose();
    const auto localUp = worldToHand*up;
    float fingerClearance = 0.f;
    std::array<Vec,7> handPoints{};
    std::size_t handPointCount = 0;
    std::optional<RE::NiPoint3> palmOpening;
    const bool inFirstPerson = firstPerson();
    if (!onCamera) {
        if (auto* knuckle = node->GetObjectByName("NPC R Finger20 [RF20]")) {
            const auto toKnuckle = knuckle->world.translate-node->world.translate;
            const float inverseScale = 1.f/std::max(node->world.scale,0.01f);
            const auto knuckleRelative = vec(toKnuckle)*inverseScale;
            auto palm = knuckleRelative*.55f;
            constexpr std::array tips{"NPC R Finger02 [RF02]","NPC R Finger12 [RF12]",
                                      "NPC R Finger22 [RF22]","NPC R Finger32 [RF32]","NPC R Finger42 [RF42]"};
            std::array<std::optional<Vec>,tips.size()> fingers;
            for (std::size_t i=0; i<tips.size(); ++i) {
                if (auto* tip = node->GetObjectByName(tips[i]))
                    fingers[i] = vec(tip->world.translate-node->world.translate)*inverseScale;
            }
            if (!inFirstPerson && fingers[0] && fingers[1] && fingers[2]) {
                palm = gripOpening(knuckleRelative,*fingers[0],*fingers[1],*fingers[2]);
                if (item.dagger || item.gripProfile!=GripProfile::standard)
                    palmOpening = worldToHand*point(palm);
            }
            const Settings defaults;
            item.socket->local.translate = worldToHand*point(palm)+RE::NiPoint3{settings.xyz[0]-defaults.xyz[0],
                settings.xyz[1]-defaults.xyz[1],settings.xyz[2]-defaults.xyz[2]};
            const auto base = vec(node->world.rotate*item.socket->local.translate);
            handPoints[handPointCount++] = base*-1.f;  // wrist
            handPoints[handPointCount++] = knuckleRelative-base;
            for (const auto& tip : fingers) {
                if (tip) {
                    fingerClearance = std::max(fingerClearance,tip->z-palm.z);
                    handPoints[handPointCount++] = *tip-base;
                }
            }
        }
    }
    const auto half = vec(item.boundsHalf)*item.mesh->local.scale;
    if (inFirstPerson) {
        const auto meshUp = item.socket->local.rotate.Transpose()*localUp;
        item.socket->local.translate += localUp*(projectedExtent(half,vec(meshUp))+fingerClearance+4.f);
    } else {
        const auto meshRotation = node->world.rotate * item.socket->local.rotate;
        const std::array meshAxes{vec(meshRotation*RE::NiPoint3{1,0,0}),
                                  vec(meshRotation*RE::NiPoint3{0,1,0}),vec(meshRotation*up)};
        const float yaw = player.GetAngleZ();
        const float c = std::cos(yaw), s = std::sin(yaw);
        // Skyrim faces +Y at zero yaw. Favor forward clearance, with a smaller
        // rightward bias to keep the held item outside the shoulder line.
        const Vec outward{.5f*c+.8660254f*s, .8660254f*c-.5f*s, 0.f};
        const auto shift = hoverOffset(half,meshAxes,outward,std::span{handPoints}.first(handPointCount));
        item.socket->local.translate += worldToHand*point(shift);
    }
    item.socket->local.translate -= item.socket->local.rotate*(item.boundsCenter*item.mesh->local.scale);
    if (!inFirstPerson && item.weaponGrip)
        placeWeaponVisual(*node,settings,*item.weaponGrip,palmOpening,player.GetAngleZ());
    if (item.socket->parent!=node) {
        if (item.parent && item.socket->parent==item.parent.get()) item.parent->DetachChild(item.socket.get());
        item.parent.reset();
        if (item.socket->parent) return false;
        SKSE::log::info("Attaching carry visual {:08X} to {}",item.id,onCamera ? "first-person camera" : "right hand");
        node->AttachChild(item.socket.get());
        if (item.socket->parent!=node) return false;
        item.parent.reset(node);
    }
    return true;
}

bool beginPickup(RE::PlayerCharacter& player, const Settings& settings)
{
    if (restore.id || pending.handle || player.IsInMidair()) {
        SKSE::log::info("Pickup rejected: recovery in progress or player airborne");
        return false;
    }
    auto* crosshair = RE::CrosshairPickData::GetSingleton();
    auto target = crosshair ? crosshair->target.get() : RE::NiPointer<RE::TESObjectREFR>{};
    if (!target && crosshair && !crosshair->targetActor)
        target = crosshair->grabPickRef.get();
    auto* reason = pickupBlock(player,target.get(),settings);
    if (reason && (!crosshair || !crosshair->targetActor)) {
        if (auto* hit = physicsPickupTarget(player,settings.pickupDistance);
            hit && hit!=target.get() && !pickupBlock(player,hit,settings)) {
            target.reset(hit);
            reason = nullptr;
            SKSE::log::info("Pickup used collision hit for reference {:08X}",id(hit));
        }
    }
    if (reason) {
        SKSE::log::info("Pickup rejected, crosshair {:08X} (base {:08X}): {}",
                        id(target.get()),id(target ? target->GetBaseObject() : nullptr),reason);
        return false;
    }
    item.target = target;
    item.id = id(target.get());
    item.base = id(target->GetBaseObject());
    item.cell = id(target->GetParentCell());
    item.world = id(target->GetWorldspace());
    phase = transition(phase,Event::requestPickup);
    elapsed = 0;
    actionStarted = false;
    markers.store(0);
    firstAtStart = firstPerson();
    SKSE::log::info("Pickup selected {:08X} (base {:08X}, {} person)",
                    item.id,item.base,firstAtStart ? "first" : "third");
    if (firstAtStart || graph(player,"TO_Pickup")) return true;
    reset(&player,false);
    return false;
}
void grab(RE::PlayerCharacter& player, const Settings& settings)
{
    if (phase!=Phase::pickup) return;
    const auto target = item.target;
    SKSE::log::info("Pickup grab marker: checking world reference {:08X} on gameplay thread",item.id);
    if (const auto* reason = pickupBlock(player,target.get(),settings)) {
        SKSE::log::warn("Grab canceled for {:08X}: {}",item.id,reason);
        return reset(&player,true);
    }
    auto* base = target->GetBaseObject();
    RE::NiPointer<RE::NiAVObject> root{target->Get3D()};
    if (!root || id(base)!=item.base || id(target->GetParentCell())!=item.cell || !anchor(player)) {
        SKSE::log::warn("Grab canceled for {:08X}: world 3D, reference identity, or right-hand anchor unavailable",item.id);
        return reset(&player,true);
    }

    SKSE::log::info("Building render-only carry visual for {:08X} (base {:08X}, root '{}')",
                    item.id,item.base,root->name.c_str());
    const auto model = isCrossbow(base) ? CarryModel::crossbow : isStaff(base) ? CarryModel::staff : CarryModel::ordinary;
    auto visual = buildCarryVisual(*root,model);
    if (!visual.root) {
        SKSE::log::warn("Grab canceled for {:08X}: {}",item.id,visual.reason);
        return reset(&player,true);
    }
    if (target->Get3D()!=root.get() || target->GetBaseObject()!=base || target->IsDisabled() ||
        target->IsDeleted() || id(target->GetParentCell())!=item.cell || player.IsGrabbing()) {
        SKSE::log::warn("Grab canceled for {:08X}: world reference changed while building the carry visual",item.id);
        return reset(&player,true);
    }
    item.mesh.reset(visual.root.get());
    item.scale = target->GetScale();
    item.radius = std::clamp(root->worldBound.radius,2.f,90.f);
    item.area = estimatedArea(*base,item.scale);
    const auto& min = base->boundData.boundMin;
    const auto& max = base->boundData.boundMax;
    item.boundsCenter = {0.5f*(min.x+max.x),0.5f*(min.y+max.y),0.5f*(min.z+max.z)};
    const auto x = max.x-min.x, y = max.y-min.y, z = max.z-min.z;
    item.boundsHalf = {0.5f*x,0.5f*y,0.5f*z};
    item.gripProfile = carryGripProfile(*base);
    if (isThrowableWeapon(base) || item.gripProfile==GripProfile::spear) {
        if (item.gripProfile==GripProfile::standard) {
            item.weaponGrip = weaponGrip(vec(item.boundsCenter),vec(item.boundsHalf)).value_or(WeaponGrip{});
        } else {
            const auto triangles = carryGripTriangles(*visual.root);
            item.weaponGrip = fittedWeaponGrip(vec(item.boundsCenter),vec(item.boundsHalf),item.gripProfile,triangles)
                .value_or(WeaponGrip{});
            const auto grip = item.weaponGrip->handle;
            SKSE::log::info("Fitted carry grip {:08X} at ({:.1f}, {:.1f}, {:.1f}), {} render triangles",
                            item.base,grip.x,grip.y,grip.z,triangles.size());
        }
        const auto scale = root->world.scale;
        item.weaponWorldScale = std::isfinite(scale) && scale>0.f ? scale : item.scale;
        if (const auto* weapon = base->As<RE::TESObjectWEAP>())
            item.dagger = weapon->GetWeaponType()==RE::WEAPON_TYPE::kOneHandDagger;
    }
    if (base->GetFormType()!=RE::FormType::AlchemyItem) {
        // Orient slender rods with the palm while keeping bottles upright.
        if (x>2*std::max(y,z)) item.gripYaw = std::numbers::pi_v<float>/2;
        if (z>2*std::max(x,y)) item.gripPitch = -std::numbers::pi_v<float>/2;
    }
    item.origin = target->GetPosition();
    if (!attach(player,settings)) {
        SKSE::log::warn("Grab canceled for {:08X}: could not attach carry visual",item.id);
        return reset(&player,true);
    }

    if (target->IsCrimeToActivate()) {
        auto* owner = target->GetOwner();
        auto* cell = target->GetParentCell();
        if (!owner && cell) owner = cell->GetActorOwner();
        if (!owner && cell) owner = cell->GetFactionOwner();
        if (owner) {
            const auto* countData = target->extraList.GetByType<RE::ExtraCount>();
            const auto count = countData ? std::max<int>(1,countData->count) : 1;
            const auto value = std::clamp(target->GetBaseObject()->GetGoldValue(),0,1000000);
            const auto total = std::min<std::int64_t>(std::int64_t(count)*value,INT32_MAX);
            player.StealAlarm(target.get(),base,count,static_cast<int>(total),owner,true);
        }
    }
    SKSE::log::info("Parking world reference {:08X} after attaching its carry visual",item.id);
    stopTracking(*target);
    target->Disable();
    if (!target->IsDisabled()) {
        SKSE::log::warn("Grab canceled for {:08X}: engine did not disable reference",item.id);
        return reset(&player,true);
    }
    item.parked = true;
    ++carryGeneration;
    phase = transition(phase,Event::grab);
    SKSE::log::info("Grab frame {} on reference {:08X}",grabFrame,item.id);
}

bool dropOrLaunch(RE::PlayerCharacter& player, const Settings& settings, bool thrown)
{
    auto target = item.target.get();
    const auto releaseCell = id(player.GetParentCell());
    const auto releaseWorld = id(player.GetWorldspace());
    if (!target || !item.socket || !item.parent || !target->IsDisabled() || !item.parked ||
        !sameCarryArea(item.cell,item.world,releaseCell,releaseWorld)) return false;
    RE::NiUpdateData update{};
    item.socket->UpdateWorldData(&update);
    const auto local = item.socket->local.translate;
    const auto centerOffset = vec(item.socket->world.rotate*(item.boundsCenter*item.scale));
    auto socket = vec(item.socket->world.translate);
    const auto center = socket+centerOffset;
    const bool first = firstPerson();
    const bool firstPersonThrowing = thrown && first;
    const bool firstPersonDropping = !thrown && first;
    RE::NiPointer<RE::NiNode> releaseNode;
    const char* releaseLabel = "hover socket";
    const auto bounds = [&](const RE::NiMatrix3& rotation) {
        return ReleaseBounds{vec(item.boundsHalf)*item.scale,
            {vec(rotation*RE::NiPoint3{1.f,0.f,0.f}), vec(rotation*RE::NiPoint3{0.f,1.f,0.f}),
             vec(rotation*RE::NiPoint3{0.f,0.f,1.f})}};
    };
    const auto setWorldRelease = [&](Vec newCenter, const RE::NiMatrix3& rotation) {
        releaseNode.reset(RE::NiNode::Create());
        if (!releaseNode) {
            SKSE::log::warn("Could not prepare world release transform for {:08X}",item.id);
            return false;
        }
        releaseNode->world = item.socket->world;
        releaseNode->world.rotate = rotation;
        socket = newCenter-vec(rotation*(item.boundsCenter*item.scale));
        releaseNode->world.translate = point(socket);
        return true;
    };
    const auto moveSocket = [&](Vec newCenter) {
        const auto delta = newCenter-centerOffset-socket;
        item.socket->local.translate += (item.parent->world.rotate.Transpose()*point(delta)) /
                                       std::max(item.parent->world.scale,.01f);
        item.socket->UpdateWorldData(&update);
        socket = vec(item.socket->world.translate);
    };
    Vec velocity{};
    bool safe{};
    const auto& socketRotation = item.socket->world.rotate;
    if (firstPersonDropping) {
        const auto box = bounds(socketRotation);
        auto drop = firstPersonDropCenter(player,box,item.radius);
        releaseLabel = "first-person drop in view";
        if (!drop) {
            drop = groundDropCenter(player,box,item.radius);
            releaseLabel = "first-person drop near player";
        }
        if (drop) {
            if (!setWorldRelease(*drop,socketRotation)) return false;
            safe = true;
        }
    } else if (firstPersonThrowing) {
        if (const auto launch = firstPersonThrow(player,*target,center,item.radius,item.area,settings)) {
            // MoveToNode needs a world transform. A detached node gives it
            // that transform without reprojecting through the animated hand.
            if (!setWorldRelease(launch->center,socketRotation)) return false;
            releaseLabel = launch->origin==FirstPersonOrigin::crosshair ?
                "centered first-person release" : "first-person clearance point";
            velocity = launch->velocity;
            safe = true;
        }
    } else if (thrown) {
        velocity = launchVelocity(player,*target,socket,item.area,settings);
        safe = velocity.length()>1.f && !clipsThrower(player,center,item.radius,velocity) &&
               clearRelease(player,center,item.radius,velocity.unit());
        if (!safe) {
            if (const auto fallback = thirdPersonThrow(player,*target,center,centerOffset,bounds(socketRotation),
                                                        item.radius,item.area,settings)) {
                moveSocket(fallback->center);
                velocity = fallback->velocity;
                releaseLabel = "third-person bounds clearance";
                safe = true;
            }
        }
    } else {
        const auto* base = target->GetBaseObject();
        const bool ammo = base && base->Is(RE::FormType::Ammo);
        const bool equipment = base && base->Is(RE::FormType::Weapon,RE::FormType::Armor,RE::FormType::Ammo);
        if (equipment && item.mesh) {
            // Try the displayed weapon pose first, then the usual hover
            // placement if the full-size reference cannot clear the player.
            auto heldCenter = vec(item.socket->world*(item.boundsCenter*hoverScale(settings)));
            auto dropRotation = socketRotation;
            std::optional<Vec> drop;
            if (item.weaponGrip) {
                const auto shown = item.socket->world*item.mesh->local;
                const auto shownCenter = vec(shown*item.boundsCenter);
                drop = thirdPersonEquipmentDropCenter(player,shownCenter,bounds(shown.rotate),item.radius);
                if (drop) {
                    heldCenter = shownCenter;
                    dropRotation = shown.rotate;
                }
            }
            if (!drop) drop = thirdPersonEquipmentDropCenter(player,heldCenter,bounds(dropRotation),item.radius);
            releaseLabel = "equipment drop at hand";
            if (!drop && (ammo || hasEquipmentGroundDrop(*base))) {
                drop = groundDropCenter(player,bounds(socketRotation),item.radius);
                dropRotation = socketRotation;
                releaseLabel = "equipment drop near player";
            }
            if (drop) {
                if (!setWorldRelease(*drop,dropRotation)) return false;
                SKSE::log::info("Equipment drop {:08X} adjusted {:.1f} game units from the visible hand position",
                                item.id,(*drop-heldCenter).length());
                safe = true;
            }
        } else {
            safe = clearRelease(player,center,item.radius,{});
        }
    }
    if (!safe) {
        item.socket->local.translate = local;
        SKSE::log::info("Release obstructed for {:08X} in {} person (center {:.1f}, {:.1f}, {:.1f}, radius {:.1f})",
                        item.id,first ? "first" : "third",center.x,center.y,center.z,item.radius);
        RE::DebugNotification(firstPersonDropping ? "Throw Object: no clear drop space nearby." :
                              firstPersonThrowing ? "Throw Object: the throw path is obstructed."
                                                  : "Throw Object: the hand position is obstructed.");
        return false;
    }
    SKSE::log::info("{} parked world reference {:08X} at {} ({:.2f}, {:.2f}, {:.2f})",
                    thrown ? "Throwing" : "Dropping",item.id,
                    releaseLabel,socket.x,socket.y,socket.z);
    // Move the same disabled reference to its release transform before re-enabling.
    // This retains all reference extras and avoids a temporary ground/carry duplicate.
    if (!target->MoveToNode(&player,releaseNode ? releaseNode.get() : item.socket.get())) {
        item.socket->local.translate = local;
        SKSE::log::warn("Release did not move world reference {:08X} to the carry socket",item.id);
        return false;
    }
    const float releaseDrift = (vec(target->GetPosition())-socket).length();
    if (!std::isfinite(releaseDrift) || releaseDrift>0.25f) {
        item.socket->local.translate = local;
        target->SetPosition(item.origin);
        SKSE::log::warn("Release canceled for {:08X}: world reference missed its release point by {:.2f} units",
                        item.id,releaseDrift);
        return false;
    }
    target->Enable(false);
    item.parked = target->IsDisabled();
    if (item.parked) {
        item.socket->local.translate = local;
        target->SetPosition(item.origin);
        SKSE::log::warn("Release did not enable world reference {:08X}; retaining the carry state",item.id);
        return false;
    }
    const bool sneakingThrow = thrown && item.sneakingThrow;
    const bool hiddenAtRelease = sneakingThrow && settings.reactions && hiddenSneaking(player);
    if (!initializeReleasedPhysics(player,*target,velocity,item.area,sneakingThrow,hiddenAtRelease,settings))
        pending = {target->GetHandle(),releaseCell,releaseWorld,velocity,item.area,0.f,sneakingThrow,hiddenAtRelease};
    detach();
    item = {};
    return true;
}

void onMarker(RE::PlayerCharacter& player, Event marker, const Settings& settings)
{
    if (marker==Event::grab) return grab(player,settings);
    if (marker==Event::pickupEnd) {
        if (phase==Phase::pickup) return reset(&player,true);
        phase = transition(phase,marker);
        carryPose(phase==Phase::held);
    }
    if (marker==Event::release && phase==Phase::throwing) {
        if (dropOrLaunch(player,settings,true)) {
            phase = transition(phase,marker);
            carryPose(false);
        }
        else {
            phase = Phase::held;
            carryPose(true);
            if (!firstAtStart) graph(player,"TO_Cancel");
        }
    }
    if (marker==Event::throwEnd && phase==Phase::released) reset(&player,false);
}

void update(RE::PlayerCharacter& player, float dt, const Settings& settings)
{
    if (restore.id) {
        auto* ui = RE::UI::GetSingleton();
        // Allow the load to finish before treating a missing reference as stale.
        if (ui && !ui->GameIsPaused() && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) && player.Is3DLoaded())
            recover(true);
    }
    if (!ready || (phase==Phase::idle && !pending.handle && !flightActive())) return;
    auto* ui = RE::UI::GetSingleton();
    if (!ui || ui->GameIsPaused()) return;
    updateFlights(player,dt,settings);
    if (pending.handle) {
        auto target = pending.handle.get();
        const auto nativeGrab = player.GetGrabbedRef();
        if (target && nativeGrab==target) {
            SKSE::log::info("Native grab took released reference {:08X}; canceled pending physics",id(target.get()));
            pending = {};
        }
        pending.elapsed += dt;
        if (!pending.handle || !target || target->IsDeleted() || target->IsDisabled() ||
            !sameCarryArea(pending.cell,pending.world,id(player.GetParentCell()),id(player.GetWorldspace())) ||
            !settings.enabled) pending = {};
        else if (initializeReleasedPhysics(player,*target,pending.velocity,pending.area,pending.sneakingThrow,
                                          pending.hiddenAtRelease,settings)) pending = {};
        else if (pending.elapsed>0.50f) {
            SKSE::log::warn("World physics initialization timed out for {:08X}; reference remains enabled",id(target.get()));
            pending = {};
        }
    }
    if (phase==Phase::idle) return;
    if (!settings.enabled || player.IsDead()) return reset(&player,true);
    const auto* cell = player.GetParentCell();
    const auto playerWorld = id(player.GetWorldspace());
    const bool areaReady = cell && (!cell->IsExteriorCell() || playerWorld!=0);
    if (item.target && areaReady && !sameCarryArea(item.cell,item.world,id(cell),playerWorld)) {
        SKSE::log::info("Ending carry for {:08X}: moved to a different playable area",item.id);
        return reset(&player,true);
    }
    if (phase!=Phase::released) {
        auto target = item.target.get();
        if (!target || target->IsDeleted() || id(target->GetBaseObject())!=item.base ||
            (!item.parked && id(target->GetParentCell())!=item.cell) ||
            (item.parked && !target->IsDisabled())) return reset(&player,true);
    }
    const auto* blocked = areaReady ? gameplayBlock(&player) : "player cell unavailable";
    if (blocked) {
        if (item.parked) return suspendCarry(player,blocked);
        return reset(&player,true);
    }
    if (item.mesh && !attach(player,settings)) return suspendCarry(player,"hand node unavailable");
    if (item.suspended) {
        item.suspended = false;
        carryPose(phase==Phase::held);
        SKSE::log::info("Resumed carry for {:08X}",item.id);
    }
    elapsed += dt;
    const auto events = markers.exchange(0);
    if (firstAtStart) {
        if (phase==Phase::pickup && elapsed>=float(grabFrame)/fps) onMarker(player,Event::grab,settings);
        if (phase==Phase::lifting && elapsed>=float(pickupEndFrame)/fps) onMarker(player,Event::pickupEnd,settings);
        if (phase==Phase::throwing && elapsed>=float(releaseFrame)/fps) onMarker(player,Event::release,settings);
        if (phase==Phase::released && elapsed>=float(throwEndFrame)/fps) onMarker(player,Event::throwEnd,settings);
    } else {
        if (events&16 && phase==Phase::pickup) actionStarted = true;
        if (events&32 && phase==Phase::throwing) actionStarted = true;
        if (events&1) onMarker(player,Event::grab,settings);
        if (events&2) onMarker(player,Event::pickupEnd,settings);
        if (events&4) onMarker(player,Event::release,settings);
        if (events&8) onMarker(player,Event::throwEnd,settings);
    }
    const auto confirmed = afterPickupTimeout(phase,elapsed,item.parked);
    if (confirmed!=phase) {
        if (confirmed==Phase::held) {
            SKSE::log::info("Keeping carried reference {:08X} after pickup end event timed out",item.id);
            onMarker(player,Event::pickupEnd,settings);
        } else {
            SKSE::log::warn("Pickup timed out: {}",actionStarted ? "grab marker missing" : "clip never reported start");
            reset(&player,true);
        }
    } else if (!firstAtStart && phase==Phase::throwing && elapsed>1.6f) {
        SKSE::log::warn("Throw timed out: {}",actionStarted ? "release marker missing" : "clip never reported start");
        phase = Phase::held;
        carryPose(true);
        graph(player,"TO_Cancel");
    } else if (phase==Phase::released && elapsed>1.6f) reset(&player,true);
}

class Markers final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event,
        RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
    {
        if (!event || !event->sender || event->sender!=RE::PlayerCharacter::GetSingleton())
            return RE::BSEventNotifyControl::kContinue;
        const std::string_view name(event->eventName.c_str());
        if (name=="TO_PickupStarted") markers.fetch_or(16);
        else if (name=="TO_ThrowStarted") markers.fetch_or(32);
        else if (name=="TO_MarkerGrab") markers.fetch_or(1);
        else if (name=="TO_MarkerPickupEnd") markers.fetch_or(2);
        else if (name=="TO_MarkerRelease") markers.fetch_or(4);
        else if (name=="TO_MarkerThrowEnd") markers.fetch_or(8);
        return RE::BSEventNotifyControl::kContinue;
    }
};
Markers callback;

std::array<int,2> nativeActivateBindings()
{
    const auto* map = RE::ControlMap::GetSingleton();
    const auto* events = RE::UserEvents::GetSingleton();
    if (!map || !events) return {activateKey,unbound};
    const auto keyboard = map->GetMappedKey(events->activate,RE::INPUT_DEVICE::kKeyboard);
    return {keyboard==RE::ControlMap::kInvalid ? unbound : bindingCode(Device::keyboard,keyboard),
            bindingCode(Device::mouse,map->GetMappedKey(events->activate,RE::INPUT_DEVICE::kMouse))};
}

std::uint64_t inputOwner() { return item.parked && item.parent && !item.suspended ? carryGeneration : 0; }

const char* actionName(Action action)
{
    switch (action) {
    case Action::pickUp: return "Pick Up";
    case Action::drop: return "Drop";
    case Action::throwItem: return "Throw";
    }
    return "Unknown action";
}

void executeInput(RE::PlayerCharacter& player, const InputRequest& request, const Settings& settings,
                  bool nativeActivateHeld)
{
    if (!currentInput(request,settings.keys,phase,inputOwner(),nativeActivateHeld)) {
        SKSE::log::info("Ignored queued {} because the request expired or the binding, carry state, or vanilla activation changed",
                        actionName(request.action));
        return;
    }
    if (!ready || !settings.enabled || menuBlocksInput()) return;
    if (const auto* reason = gameplayBlock(&player)) {
        SKSE::log::info("{} rejected in player update: {}",actionName(request.action),reason);
        return;
    }
    SKSE::log::info("Processing {} for {} in player update (thread {})",bindingName(request.key),
                    actionName(request.action),REX::W32::GetCurrentThreadId());

    switch (request.action) {
    case Action::pickUp:
        beginPickup(player,settings);
        break;
    case Action::drop:
        if (dropOrLaunch(player,settings,false)) reset(&player,true);
        break;
    case Action::throwItem:
        if (phase==Phase::held) {
            phase = transition(phase,Event::requestThrow);
            item.sneakingThrow = player.IsSneaking();
            elapsed = 0;
            actionStarted = false;
            markers.store(0);
            firstAtStart = firstPerson();
            if (!firstAtStart && !graph(player,"TO_Throw")) {
                phase = Phase::held;
                carryPose(true);
            }
        }
        break;
    }
}

template<class Pass>
auto filterInput(RE::InputEvent* const* events, Pass pass)
{
    if (!events || !*events) return pass(events);
    const auto settings = getSettings();
    const bool blocked = !settings.enabled || menuBlocksInput();
    return filterInputList(events,[&](RE::InputEvent& input) {
        auto* button = input.AsButtonEvent();
        if (!button) return false;
        const auto key = buttonCode(*button);
        const auto route = inputs.button(key,button->IsDown(),button->IsUp(),settings.keys,blocked);
        if (route.queued)
            SKSE::log::info("Queued {} for {} from input thread {}",bindingName(key),actionName(*route.queued),
                            REX::W32::GetCurrentThreadId());
        return route.consume;
    },pass);
}

struct DeviceDispatch {
    static void run(RE::BSTEventSource<RE::InputEvent*>* source, RE::InputEvent* const* events)
    {
        filterInput(events,[&](auto* list) { original(source,list); });
    }
    static inline REL::Relocation<decltype(run)> original;
};

struct PlayerControlsDispatch {
    using Sink = RE::BSTEventSink<RE::InputEvent*>;
    static RE::BSEventNotifyControl run(Sink* self, RE::InputEvent* const* events,
                                       RE::BSTEventSource<RE::InputEvent*>* source)
    {
        return filterInput(events,[&](auto* list) { return original(self,list,source); });
    }
    static inline REL::Relocation<decltype(run)> original;
};

void installInput()
{
    if (inputHookInstalled) return;
    // Same ordered input dispatch call used by Menu Framework 3 for Steam
    // 1.6.1170. SKSE's call trampoline chains the handler already installed there.
    const auto call = REL::RelocationID(67315,68617).address()+0x7B;
    if (*reinterpret_cast<const std::uint8_t*>(call)==0xE8) {
        SKSE::AllocTrampoline(32);
        DeviceDispatch::original = SKSE::GetTrampoline().write_call<5>(call,DeviceDispatch::run);
        SKSE::log::info("Input registered on the keyboard/mouse device queue");
    } else {
        REL::Relocation<std::uintptr_t> input{RE::VTABLE_PlayerControls[0]};
        PlayerControlsDispatch::original = input.write_vfunc(1,PlayerControlsDispatch::run);
        SKSE::log::warn("Input queue call unavailable; registered PlayerControls input fallback");
    }
    inputHookInstalled = true;
}
struct UpdateHook {
    static void run(RE::PlayerCharacter* player, float dt)
    {
        original(player,dt);
        if (!player || !std::isfinite(dt)) return;

        const auto settings = getSettings();
        const auto batch = inputs.take();
        const float step = std::clamp(dt,0.f,0.10f);
        update(*player,step,settings);
        for (std::size_t i=0; i<batch.count; ++i)
            executeInput(*player,batch.requests[i],settings,batch.nativeActivateHeld);

        const bool grabbing = player->IsGrabbing();
        const auto native = grabbing ? id(player->GetGrabbedRef().get()) : 0;
        if (native && native!=nativeGrabID)
            SKSE::log::info("Skyrim native grab {:08X} ({} person)",native,firstPerson() ? "first" : "third");
        nativeGrabID = native;
        if (grabbing) carryPose(false);
        auto* ui = RE::UI::GetSingleton();
        const bool showMeter = ready && settings.enabled && settings.toleranceMeter && toleranceMeterAvailable() &&
            player->Is3DLoaded() && ui && !ui->GameIsPaused() &&
            !ui->IsMenuOpen(RE::Console::MENU_NAME) && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
        updateToleranceMeter(*player,settings,showMeter);
        inputs.publish(phase,inputOwner(),ready && settings.enabled && !grabbing && ui && !ui->GameIsPaused(),
                       nativeActivateBindings());
    }
    static inline REL::Relocation<decltype(run)> original;
};

bool checkAsset(const std::filesystem::path& path)
{
    std::error_code error;
    if (std::filesystem::is_regular_file(path,error)) return true;
    SKSE::log::error("Missing asset {}",path.generic_string());
    return false;
}

bool assetsReady()
{
    bool valid = true;
    const std::filesystem::path base("Data/meshes/actors/character/animations");
    for (auto clip : clips)
        valid &= checkAsset(base/"ThrowObject"/("ThrowObject_"+std::string(clip)+".hkx"));
    valid &= checkAsset("Data/SKSE/Plugins/AnimationInjector/ThrowObject/ThrowObject.xml");
    valid &= checkAsset(base/"OpenAnimationReplacer/ThrowObject/Carry/config.json");
    return valid;
}

void message(SKSE::MessagingInterface::Message* msg)
{
    switch (msg->type) {
    case SKSE::MessagingInterface::kInputLoaded:
        installInput();
        break;
    case SKSE::MessagingInterface::kDataLoaded: {
        SKSE::log::info("SKSE DataLoaded: checking gameplay dependencies");
        installInput();
        auto* data = RE::TESDataHandler::GetSingleton();
        carry = data ? data->LookupForm<RE::TESGlobal>(carryGlobal,"ThrowObject.esm") : nullptr;
        carryPose(false);
        const auto validAssets = assetsReady();
        const bool injector = REX::W32::GetModuleHandleW(L"AnimationInjector.dll")!=nullptr;
        const bool oar = REX::W32::GetModuleHandleW(L"OpenAnimationReplacer.dll")!=nullptr;
        auto* source = SKSE::GetModCallbackEventSource();
        if (!carry) SKSE::log::error("ThrowObject.esm did not resolve its carry global (local FormID 000800)");
        if (!injector) SKSE::log::error("AnimationInjector.dll is not loaded by SKSE");
        if (!oar) SKSE::log::error("OpenAnimationReplacer.dll is not loaded by SKSE");
        if (!source) SKSE::log::error("SKSE mod callback event source unavailable");
        ready = carry && validAssets && injector && oar && source;
        if (ready) {
            source->AddEventSink(&callback);
            loadReactionTopic();
            REL::Relocation<std::uintptr_t> player{RE::VTABLE_PlayerCharacter[0]};
            UpdateHook::original = player.write_vfunc(0xAD,UpdateHook::run);
            SKSE::log::info("Throw Object ready for Skyrim Steam 1.6.1170; actions run in player update; carry priority 2147483647");
        } else {
            SKSE::log::error("Throw Object gameplay disabled. Input diagnostics remain active.");
        }
        registerMenu();
        break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
    case SKSE::MessagingInterface::kNewGame:
        reset(RE::PlayerCharacter::GetSingleton(),false);
        clearFlights();
        pending = {};
        inputs.clear();
        nativeGrabID = 0;
        restore = {};
        resetTolerance();
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        recover(false);
        carryPose(false);
        if (msg->data) recoverInvestigationPackages();
        break;
    case SKSE::MessagingInterface::kSaveGame:
        cancelDistractions();
        break;
    default: break;
    }
}
void save(SKSE::SerializationInterface* archive)
{
    const auto record = item.parked ? Recovery{item.id,item.base} : restore;
    if (record.id && archive->OpenRecord(heldRecord,heldRecordVersion)) archive->WriteRecordData(record);
    saveTolerance(*archive);
}
void load(SKSE::SerializationInterface* archive)
{
    std::uint32_t type{}, version{}, length{};
    while (archive->GetNextRecordInfo(type,version,length)) {
        if (type==toleranceRecord) {
            loadTolerance(*archive,version,length);
            continue;
        }
        Recovery previous, next;
        if (type==heldRecord && version==heldRecordVersion && length==sizeof(previous) && archive->ReadRecordData(previous)==length &&
            archive->ResolveFormID(previous.id,next.id) && archive->ResolveFormID(previous.base,next.base))
            restore = next;
    }
}
void revert(SKSE::SerializationInterface*)
{
    reset(nullptr,false,false);
    clearFlights();
    pending = {};
    inputs.clear();
    nativeGrabID = 0;
    restore = {};
    resetTolerance();
}
}

class LogFile final : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit LogFile(const std::filesystem::path& filename) : stream(filename,std::ios::binary | std::ios::trunc)
    {
        if (!stream) throw spdlog::spdlog_ex("ThrowObject log could not be opened");
    }
private:
    void sink_it_(const spdlog::details::log_msg& message) override
    {
        spdlog::memory_buf_t line;
        formatter_->format(message,line);
        stream.write(line.data(),static_cast<std::streamsize>(line.size()));
    }
    void flush_() override { stream.flush(); }
    std::ofstream stream;
};

void startLogging()
{
    const auto open = [](const std::filesystem::path& directory) {
        auto logger = std::make_shared<spdlog::logger>("ThrowObject",
                                                       std::make_shared<LogFile>(directory/"ThrowObject.log"));
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
    };
    try {
        if (const auto desktop = SKSE::log::log_directory()) {
            std::error_code error;
            std::filesystem::create_directories(*desktop,error);
            if (!error) {
                open(*desktop);
                return;
            }
        }
    } catch (const std::exception&) {}
    try {
        open("Data/SKSE/Plugins");
        SKSE::log::warn("Primary log folder unavailable; logging in Data/SKSE/Plugins");
    } catch (const std::exception&) {
        REX::W32::OutputDebugStringW(L"Throw Object could not open either log directory.");
    }
}

}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData metadata{};
    metadata.PluginVersion({0,3,9,3});
    metadata.PluginName("ThrowObject");
    metadata.AuthorName("Tsoxer");
    metadata.UsesAddressLibrary();
    metadata.UsesStructsPost629();
    metadata.MinimumRequiredXSEVersion({2,2,6,0});
    metadata.CompatibleVersions({to::runtime});
    return metadata;
}();

extern "C" __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    if (skse->RuntimeVersion()!=to::runtime || skse->IsEditor()) return false;
    SKSE::Init(skse);
    to::startLogging();
    SKSE::log::info("Throw Object v0.3.9 RC3 DLL loaded for Skyrim Steam 1.6.1170");
    to::loadSettings();
    auto* serialization = SKSE::GetSerializationInterface();
    auto* messaging = SKSE::GetMessagingInterface();
    if (!serialization || !messaging || !messaging->RegisterListener(to::message)) {
        SKSE::log::error("Unable to register SKSE messaging or serialization");
        return false;
    }
    serialization->SetUniqueID(to::serializationID);
    serialization->SetSaveCallback(to::save);
    serialization->SetLoadCallback(to::load);
    serialization->SetRevertCallback(to::revert);
    SKSE::log::info("SKSE messaging and serialization registered");
    return true;
}
