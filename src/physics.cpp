#include "physics.hpp"
#include "equipment_game.hpp"
#include "distraction.hpp"
#include "distraction_rules.hpp"
#include "suspicion.hpp"
#include "weapon_spin.hpp"

#include <atomic>
#include <mutex>
#include <numbers>
#include <vector>

namespace to
{
Vec vec(const RE::NiPoint3& p) { return {p.x,p.y,p.z}; }
RE::NiPoint3 point(Vec v) { return {v.x,v.y,v.z}; }

namespace
{
Vec vector(const RE::hkVector4& v)
{
    float xyz[4];
    _mm_storeu_ps(xyz,v.quad);
    return {xyz[0],xyz[1],xyz[2]};
}
RE::hkVector4 havok(Vec v) { return {v.x,v.y,v.z,0}; }
RE::hkpRigidBody& native(RE::bhkRigidBody& body)
{
    return *static_cast<RE::hkpRigidBody*>(body.referencedObject.get());
}

Vec bodyPosition(RE::bhkRigidBody& body)
{
    return vector(native(body).motion.motionState.transform.translation)*RE::bhkWorld::GetWorldScaleInverse();
}

struct Impact {
    std::uint64_t launch{};
    RE::FormID actor{};
    float speed{};
    RE::bhkCharacterController* controller{};
};
struct GroundTouch { std::uint64_t launch{}; Vec point; };
struct ContactBatch {
    std::vector<Impact> impacts;
    std::array<GroundTouch,4> touches;
};
class Contacts final : public RE::hkpContactListener {
public:
    void ContactPointCallback(const RE::hkpContactPointEvent& event) override
    {
        if (!hasTrackedBodies.load(std::memory_order_relaxed) || !event.bodies[0] || !event.bodies[1]) return;
        const std::scoped_lock guard(lock);
        for (int i=0; i<2; ++i) {
            const auto id = std::find(bodies.begin(),bodies.end(),event.bodies[i]);
            if (id == bodies.end() || !*id) continue;
            const auto slot = static_cast<std::size_t>(id-bodies.begin());
            auto* ref = event.bodies[1-i]->GetUserData();
            if (!ref || !ref->As<RE::Actor>()) {
                if (listenForGround[slot] && !touches[slot].launch && event.contactPoint) {
                    const auto at = vector(event.contactPoint->position);
                    const auto normal = vector(event.contactPoint->separatingNormal);
                    const auto origin = vector(event.bodies[i]->motion.motionState.transform.translation);
                    if (std::abs(normal.z)>=.55f && origin.z>=at.z-.035f && std::isfinite(at.length()))
                        touches[slot] = {launches[slot],at};
                }
                return;
            }
            if (count==impacts.size()) return;
            const auto relative = (event.bodies[i]->motion.linearVelocity-event.bodies[1-i]->motion.linearVelocity).Length3();
            const float speed = event.separatingVelocity ? std::max(0.f,-*event.separatingVelocity) : relative;
            impacts[count++] = {launches[id-bodies.begin()],ref->GetFormID(),std::isfinite(speed) ? speed : 0.f};
            return;
        }
    }

    void ProxyContact(RE::bhkCharProxyController* controller, RE::hkpCharacterProxy* proxy,
                      const RE::hkpCharacterObjectInteractionEvent& event)
    {
        if (!hasTrackedBodies.load(std::memory_order_relaxed) || !proxy || !proxy->shapePhantom || !event.body) return;
        const std::scoped_lock guard(lock);
        if (count == impacts.size()) return;
        const auto body = std::find(bodies.begin(),bodies.end(),event.body);
        if (body==bodies.end()) return;
        auto* ref = proxy->shapePhantom->GetUserData();
        const auto relative = event.body->motion.linearVelocity-proxy->velocity;
        const float speed = std::max(std::abs(event.projectedVelocity),std::abs(relative.Dot3(event.normal)));
        const auto actor = ref && ref->As<RE::Actor>() ? ref->GetFormID() : 0;
        impacts[count++] = {launches[body-bodies.begin()],actor,std::isfinite(speed) ? speed : 0.f,controller};
    }

    void track(std::size_t slot, RE::hkpRigidBody* body, std::uint64_t launch, bool hearLanding = false)
    {
        const std::scoped_lock guard(lock);
        bodies[slot] = body;
        launches[slot] = launch;
        listenForGround[slot] = hearLanding;
        touches[slot] = {};
        hasTrackedBodies.store(std::any_of(bodies.begin(),bodies.end(),[](auto* body) { return body!=nullptr; }),
                               std::memory_order_relaxed);
    }

    ContactBatch drain()
    {
        const std::scoped_lock guard(lock);
        ContactBatch result{{impacts.begin(),impacts.begin()+count},touches};
        count = 0;
        touches = {};
        return result;
    }
private:
    std::mutex lock;
    std::atomic_bool hasTrackedBodies{};
    std::array<RE::hkpRigidBody*,4> bodies{};
    std::array<std::uint64_t,4> launches{};
    std::array<bool,4> listenForGround{};
    std::array<GroundTouch,4> touches{};
    std::array<Impact,64> impacts{};
    std::size_t count{};
};

struct DistractionNotice {
    std::vector<RE::ActorHandle> witnesses;
    Vec scanFrom, ground, quietAnchor;
    float nextScan{}, groundAt{}, quietTime{};
    bool enabled{}, grounded{};
};

struct Flight {
    RE::ObjectRefHandle object;
    RE::NiPointer<RE::bhkRigidBody> body;
    RE::FormID id{};
    std::uint64_t launch{};
    float elapsed{}, stillTime{}, appliedDrag{}, launchSpeed{}, weight{};
    Vec previousPosition, previousVelocity;
    float sweepRadius{};
    bool reacted{}, metal{}, clearedThrower{}, sneakingThrow{}, weaponHitSpent{};
    std::vector<RE::FormID> targets;
    RE::FormID suspicionSpeaker{};
    DistractionNotice notice;
    bool distractionThrow{};
    bool inspectWeaponMiss{};
};
std::array<Flight,4> flights;
Contacts listener;
struct ProxyHook {
    static void contact(RE::bhkCharProxyController* controller, RE::hkpCharacterProxy* proxy,
                        const RE::hkpCharacterObjectInteractionEvent& event,
                        RE::hkpCharacterObjectInteractionResult& result)
    {
        listener.ProxyContact(controller,proxy,event);
        original.load(std::memory_order_acquire)(controller,proxy,event,result);
    }
    static inline std::atomic<decltype(&contact)> original{};
};

void installContactHook()
{
    if (ProxyHook::original.load(std::memory_order_acquire)) return;
    // CommonLib documents ObjectInteractionCallback at slot 05 of the first base.
    constexpr std::size_t slot = 5;
    REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_bhkCharProxyController[0]};
    const auto* entries = reinterpret_cast<const std::uintptr_t*>(vtable.address());
    // A Havok worker can enter the replacement as soon as the slot is patched.
    ProxyHook::original.store(reinterpret_cast<decltype(&ProxyHook::contact)>(entries[slot]),
                              std::memory_order_release);
    vtable.write_vfunc(slot,ProxyHook::contact);
}
RE::NiPointer<RE::bhkWorld> currentWorld;
std::uint64_t nextLaunch{};
float reactionCooldown{};
float suspicionCooldown{};
RE::TESTopic* vanillaReaction{};
void removeListener()
{
    if (!currentWorld) return;
    {
        const RE::BSWriteLockGuard lock(currentWorld->worldLock);
        auto& listeners = currentWorld->GetWorld1()->contactListeners;
        if (const auto it = std::find(listeners.begin(),listeners.end(),&listener); it != listeners.end()) {
            *it = listeners.back();
            listeners.resize(listeners.size()-1);
        }
    }
    currentWorld.reset();
}

void forget(std::size_t slot)
{
    listener.track(slot,nullptr,0);
    flights[slot] = {};
}

bool observe(RE::bhkWorld& world)
{
    if (currentWorld.get() == &world) return true;
    if (!world.GetWorld1()) return false;
    // A settled flight can release its contact listener while NPCs continue
    // investigating. Connecting a new throw must not cancel those NPCs.
    if (currentWorld || !investigationsActive()) clearFlights();
    const RE::BSWriteLockGuard lock(world.worldLock);
    world.GetWorld1()->contactListeners.push_back(&listener);
    currentWorld.reset(&world);
    return true;
}

struct CameraAim { Vec position, forward; };

CameraAim cameraAim()
{
    auto* camera = RE::PlayerCamera::GetSingleton();
    if (!camera || !camera->currentState) return {};
    RE::NiQuaternion rotation{};
    RE::NiPoint3 position{};
    camera->currentState->GetRotation(rotation);
    camera->currentState->GetTranslation(position);
    const auto& [w,x,y,z] = rotation;
    // Skyrim's camera-state forward axis is +Y.
    return {vec(position),Vec{2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)}.unit()};
}

struct FirstPersonView { CameraAim aim; float nearPlane; };

FirstPersonView firstPersonView()
{
    auto* camera = RE::Main::WorldRootCamera();
    if (camera) {
        const auto& frustum = camera->GetRuntimeData2().viewFrustum;
        const auto& projection = camera->GetRuntimeData().worldToCam;
        const auto row = [&](int index) { return Vec{projection[index][0],projection[index][1],projection[index][2]}; };
        const auto forward = screenCenterDirection(row(0),row(1),row(3));
        if (!frustum.bOrtho && std::isfinite(frustum.fNear) && frustum.fNear>0.f && forward.length()>.5f) {
            const auto eye = vec(camera->world.translate);
            const RE::NiRect<float> viewport{0.f,1.f,1.f,0.f};
            float x{},y{},z{};
            // Confirm the matrix and eye belong to the same rendered view.
            if (RE::NiCamera::WorldPtToScreenPt3(projection,viewport,point(eye+forward*128.f),x,y,z,1e-5f) &&
                std::abs(x-.5f)<.005f && std::abs(y-.5f)<.005f) {
                const auto depthFactor = forward.dot(row(3).unit());
                return {{eye,forward},frustum.fNear/depthFactor};
            }
        }
    }
    return {cameraAim(),15.f};
}

struct Ray {
    float fraction{1.0f};
    RE::TESObjectREFR* hit{};
};

Ray ray(RE::PlayerCharacter& player, Vec start, Vec end, RE::COL_LAYER layer,
        const RE::TESObjectREFR* ignored = nullptr, const RE::hkpCollidable* ignoredBody = nullptr)
{
    auto* cell = player.GetParentCell();
    auto* world = cell ? cell->GetbhkWorld() : nullptr;
    if (!world || !world->GetWorld1()) return {0,nullptr};
    const auto scale = RE::bhkWorld::GetWorldScale();
    const RE::BSReadLockGuard lock(world->worldLock);
    const auto cast = [&](Vec from) {
        RE::hkpWorldRayCastInput input{};
        input.from = havok(from*scale);
        input.to = havok(end*scale);
        input.enableShapeCollectionFilter = true;
        input.filterInfo = static_cast<std::uint32_t>(layer);
        RE::hkpWorldRayCastOutput output{};
        world->GetWorld1()->CastRay(input,output);
        return output;
    };
    // A third-person ray can meet the player's capsule before reaching the crosshair target.
    Vec from = start;
    for (int i=0;i<8;++i) {
        auto output = cast(from);
        if (!output.HasHit()) return {};
        if (!output.rootCollidable) return {0,nullptr};
        const auto site = from+(end-from)*std::clamp(output.hitFraction,0.0f,1.0f);
        const bool ownBody = output.rootCollidable==ignoredBody;
        auto* hit = ownBody ? nullptr : RE::TESHavokUtilities::FindCollidableRef(*output.rootCollidable);
        if (!ownBody && (!hit || (!hit->IsPlayerRef() && hit!=ignored))) {
            return {std::clamp((site-start).length()/std::max((end-start).length(),1.0f),0.0f,1.0f),hit};
        }
        from = site+(end-from).unit()*((ownBody || hit==ignored) ? 2.f : 9.f);
        if ((end-from).dot(end-start)<=0) return {};
    }
    return {0,nullptr};
}

const RE::hkpCollidable* playerProxyCollider(RE::PlayerCharacter& player)
{
    auto* controller = player.GetCharController();
    const auto* proxyController = controller ? skyrim_cast<RE::bhkCharProxyController*>(controller) : nullptr;
    const auto* proxy = proxyController ? proxyController->GetCharacterProxy() : nullptr;
    return proxy && proxy->shapePhantom ? proxy->shapePhantom->GetCollidable() : nullptr;
}

bool clearReleaseMargins(RE::PlayerCharacter& player, Vec center, Vec end, float radius,
                         const RE::hkpCollidable* playerBody)
{
    const auto margin = std::clamp(radius*.70f,3.f,28.f);
    for (const auto offset : {Vec{margin,0,0},Vec{-margin,0,0},Vec{0,margin,0},
                               Vec{0,-margin,0},Vec{0,0,margin},Vec{0,0,-margin}})
        if (ray(player,center,end+offset,RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction<.99f) return false;
    return true;
}

template<class... Args>
bool actorFunction(RE::Actor& actor, const char* script, const char* method, Args... args)
{
    auto* vm = RE::SkyrimVM::GetSingleton();
    if (!vm || !vm->impl) return false;
    auto* policy = vm->impl->GetObjectHandlePolicy();
    if (!policy) return false;
    const auto handle = policy->GetHandleForObject(RE::FormType::ActorCharacter, &actor);
    if (handle == policy->EmptyHandle()) return false;
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
    return vm->impl->DispatchMethodCall(handle,script,method,RE::MakeFunctionArguments(std::move(args)...),result);
}

struct ActorCapsule { Vec bottom, top; float radius; };

ActorCapsule actorCapsule(RE::Actor& actor)
{
    const float height = std::clamp(actor.GetHeight(),24.f,210.f);
    const float radius = std::clamp(height*0.12f,6.f,24.f);
    const auto feet = vec(actor.GetPosition());
    return {feet+Vec{0,0,radius},feet+Vec{0,0,height-radius},radius};
}

bool inFlightWorld(RE::Actor& actor)
{
    const auto* cell = actor.GetParentCell();
    return cell && cell->GetbhkWorld()==currentWorld.get();
}

void stagger(RE::Actor& target, Vec source, float magnitude)
{
    if (target.IsInRagdollState() || target.IsInKillMove() || target.GetOccupiedFurniture()) return;
    const auto delta = source-vec(target.GetPosition());
    constexpr float tau = 2.f*std::numbers::pi_v<float>;
    const float angle = std::fmod(std::atan2(delta.x,delta.y)-target.GetAngleZ()+tau,tau)/tau;
    target.SetGraphVariableFloat("staggerDirection",angle);
    target.SetGraphVariableFloat("staggerMagnitude",magnitude);
    target.NotifyAnimationGraph("staggerStart");
}

bool unawareOfPlayer(RE::PlayerCharacter& player, RE::Actor& target)
{
    return player.GetActorRuntimeData().currentProcess && target.GetActorRuntimeData().currentProcess &&
        target.RequestDetectionLevel(&player)<=0;
}

std::optional<std::int16_t> highestPlayerDetection(RE::PlayerCharacter& player)
{
    auto* processes = RE::ProcessLists::GetSingleton();
    if (!processes || !player.GetActorRuntimeData().currentProcess) return std::nullopt;
    // Ask the engine across observers. Someone other than the reacting actor
    // can open the sneak eye, and the HUD artwork can be replaced or hidden.
    std::uint32_t losCount{};
    return processes->RequestHighestDetectionLevelAgainstActor(&player,losCount);
}

void noteWitness(Flight& flight, RE::Actor& actor);

void flightReaction(RE::PlayerCharacter& player, Flight& flight, RE::Actor& target, bool actorUnaware,
                    const Settings& settings)
{
    if (!settings.reactions || flight.reacted || isInvestigating(target) ||
        target.GetCurrentScene() || target.IsInCombat() || target.AsActorState()->IsUnconscious()) return;

    const bool stillSneaking = player.IsSneaking();
    const auto detection = flight.sneakingThrow && stillSneaking && actorUnaware ?
        highestPlayerDetection(player) : std::nullopt;
    if (detection && (settings.log || settings.trajectoryDebug))
        SKSE::log::info("Flight {:08X} highest player detection: {}",flight.id,*detection);
    if (useSuspicionVoice(flight.sneakingThrow,stillSneaking,actorUnaware,detection)) {
        if (flight.notice.enabled) noteWitness(flight,target);
        if (suspicionCooldown>0.f) return;
        flight.reacted = true;
        if (playSuspicion(target)) {
            suspicionCooldown = 4.f;
            flight.suspicionSpeaker = target.GetFormID();
        }
        return;
    }

    if (reactionCooldown>0.f) return;
    flight.reacted = true;
    reactionCooldown = settings.reactCooldown;
    auto* topic = settings.reactionTopic ?
        RE::TESForm::LookupByID<RE::TESTopic>(settings.reactionTopic) : vanillaReaction;
    const bool voiced = topic && actorFunction(target,"ObjectReference","Say",topic,
                                               static_cast<RE::Actor*>(nullptr),false);
    SKSE::log::info("Flight {:08X} regular reaction at {:08X}: speech {}",
                    flight.id,target.GetFormID(),voiced ? "queued" : "unavailable");
}

void noteWitness(Flight& flight, RE::Actor& actor)
{
    auto& witnesses = flight.notice.witnesses;
    if (!eligibleThrowWitness(flight.inspectWeaponMiss,flight.targets,actor.GetFormID()) ||
        witnesses.size()>=32 || isInvestigating(actor) || !canInvestigate(actor)) return;
    const auto handle = actor.GetHandle();
    if (std::find(witnesses.begin(),witnesses.end(),handle)==witnesses.end()) {
        witnesses.push_back(handle);
        SKSE::log::info("Distraction {:08X}: witness {:08X}",flight.id,actor.GetFormID());
    }
}

void queueReactionInspection(Flight& flight, RE::Actor& actor, const Settings& settings)
{
    if (!settings.enabled || !settings.reactions || !flight.distractionThrow || isInvestigating(actor)) return;
    if (flight.notice.enabled) noteWitness(flight,actor);
    // Preserve the pass witness. The first landing scan supplies their
    // destination after the impact checks have run.
    if (flight.inspectWeaponMiss) return;
    if (auto object = flight.object.get())
        queueNearbyInspection(actor,*object,actor.GetFormID()==flight.suspicionSpeaker);
}

void noticeDistraction(RE::PlayerCharacter& player, RE::TESObjectREFR& object, Flight& flight,
                       Vec position, GroundTouch touch, float dt, bool finishing, const Settings& settings)
{
    auto& notice = flight.notice;
    if (!notice.enabled) return;
    if (!settings.enabled || !settings.reactions) {
        notice = {};
        return;
    }
    if ((position-notice.quietAnchor).length()<=3.f) notice.quietTime += dt;
    else {
        notice.quietAnchor = position;
        notice.quietTime = 0.f;
    }
    const bool landed = touch.launch==flight.launch && !notice.grounded;
    if (touch.launch==flight.launch) {
        notice.grounded = true;
        if (landed) notice.groundAt = flight.elapsed;
        notice.ground = touch.point*RE::bhkWorld::GetWorldScaleInverse();
    }
    const bool settled = readyToInvestigate(notice.grounded,notice.quietTime,flight.elapsed-notice.groundAt,finishing);
    if (!landed && !settled && notice.nextScan>flight.elapsed) return;
    const auto landing = landed ? notice.ground : position;
    auto* processes = RE::ProcessLists::GetSingleton();
    if (processes) {
        processes->ForEachHighActor([&](RE::Actor* actor) {
            using Result = RE::BSContainer::ForEachResult;
            if (!actor || actor==&player || !inFlightWorld(*actor)) return Result::kContinue;
            const auto capsule = actorCapsule(*actor);
            const auto radius = settings.reactDistance+capsule.radius;
            if (hearsThrow(notice.scanFrom,position,capsule.bottom,capsule.top,radius) ||
                ((landed || settled) && hearsThrow(landing,landing,capsule.bottom,capsule.top,radius)))
                noteWitness(flight,*actor);
            return Result::kContinue;
        });
    }
    notice.scanFrom = position;
    notice.nextScan = flight.elapsed+.09f;
    if (!settled) return;

    SKSE::log::info("Distraction {:08X}: landing via {}, {} witness(es)",flight.id,
                    notice.grounded ? "contact and motion" : "resting position",notice.witnesses.size());
    if (!notice.witnesses.empty()) {
        // Project the resting position toward walkable floor. Use a nearby
        // Havok support point if the ray starts inside a collision shape.
        const auto from = position+Vec{0,0,4.f};
        const auto below = position-Vec{0,0,72.f};
        const auto floor = ray(player,from,below,RE::COL_LAYER::kDroppingPick,&object,native(*flight.body).GetCollidable());
        if (floor.fraction<.99f && (!floor.hit || !floor.hit->As<RE::Actor>()))
            position.z = from.z+(below.z-from.z)*floor.fraction;
        else if (notice.grounded && (position-notice.ground).length()<56.f)
            position.z = std::min(position.z,notice.ground.z);
        investigateLanding(object,position,notice.witnesses,flight.suspicionSpeaker);
    }
    notice = {};
}

void assault(RE::PlayerCharacter& player, RE::Actor& victim, const Settings& settings)
{
    auto* faction = victim.GetCrimeFaction();
    if (!faction || !faction->TracksCrimes() || faction->IgnoresAssault() ||
        !settings.crime || settings.bounty==0 ||
        victim.IsHostileToActor(&player) || victim.IsPlayerTeammate()) return;
    faction->ModCrimeGold(settings.bounty,true);
    // Leave arrest and surrender to the crime faction's ordinary guard packages.
    SKSE::log::info("Assault victim {:08X} faction {:08X} custom violent bounty +{}",
                   victim.GetFormID(),faction->GetFormID(),settings.bounty);
}

void applyImpact(RE::PlayerCharacter& player, Flight& flight, RE::Actor& target, float speed,
                 const Settings& settings)
{
    const auto targetID = target.GetFormID();
    if (target.IsDead() || target.IsGhost() || !inFlightWorld(target) || !std::isfinite(speed) ||
        speed<1.f || flight.targets.size()>=24 ||
        std::find(flight.targets.begin(),flight.targets.end(),targetID)!=flight.targets.end()) return;

    flight.targets.push_back(targetID);
    const bool self = &target==&player;
    const bool sneakHit = flight.sneakingThrow && !self;
    auto weapon = !self ? flight.object.get() : RE::NiPointer<RE::TESObjectREFR>{};
    const auto* hitBase = weapon ? weapon->GetBaseObject() : nullptr;
    const bool weaponHit = isThrowableWeapon(hitBase);
    if (weapon && flight.inspectWeaponMiss && flight.distractionThrow) {
        std::erase(flight.notice.witnesses,target.GetHandle());
        cancelInspectionForHit(target,*weapon);
    }
    const bool meleeHit = isOneHandedWeapon(hitBase) && settings.damageEnabled &&
        settings.weaponMeleeDamage && !flight.weaponHitSpent;
    const auto weaponDamage = meleeHit ? ordinaryWeaponDamage(player,target,*weapon) : std::nullopt;
    if (meleeHit) flight.weaponHitSpent = true;
    const auto reaction = self ? ToleranceReaction::ordinary : recordToleratedHit(
        player,target,settings,weaponHit ? ToleranceHit::exhaust : ToleranceHit::ordinary);
    const bool unawareOfThrower = sneakHit && unawareOfPlayer(player,target);
    const bool investigating = !self && isInvestigating(target);
    if (sneakHit && !investigating && (settings.damageEnabled || settings.reactions)) stagger(target,flight.previousPosition,.35f);
    if (sneakHit) flightReaction(player,flight,target,unawareOfThrower,settings);
    if (sneakHit) queueReactionInspection(flight,target,settings);
    const bool tolerated = reaction==ToleranceReaction::tolerated;
    if (!self && !tolerated) assault(player,target,settings);
    const auto startCombat = [&] {
        if (target.IsDead() || target.IsPlayerTeammate() || target.IsInCombat()) return;
        if (unawareOfThrower && !weaponHit && reaction!=ToleranceReaction::limitReached) return;
        const bool started = actorFunction(target,"Actor","StartCombat",static_cast<RE::Actor*>(&player));
        SKSE::log::info("Flight {:08X} combat request for {:08X}: {}",flight.id,targetID,
                        started ? "queued" : "unavailable");
    };
    if (!settings.damageEnabled) {
        if (weaponHit) startCombat();
        return;
    }
    auto* values = target.AsActorValueOwner();
    if (!values) return;
    const float health = values->GetActorValue(RE::ActorValue::kHealth);
    if (!std::isfinite(health) || health<=0.f) return;
    const bool protectedHit = tolerated || target.IsEssential() || target.IsProtected();
    const bool armedHit = weaponDamage.has_value() && !protectedHit;
    float loss = weaponDamage ? *weaponDamage*thrownSwingDamageScale :
        damage(flight.weight,speed,flight.launchSpeed,settings.damageScale,settings.maxDamage);
    if (protectedHit)
        loss = std::min(loss,std::max(0.0f,health-1.0f));
    if (!(loss>0.f) && !armedHit) {
        if (weaponHit || reaction==ToleranceReaction::limitReached) startCombat();
        return;
    }

    // CommonLibSSE-NG Actor::DoDamage, with the pinned library's AE Address
    // Library ID. The native damage path takes credit for a real actor hit.
    using Damage = bool(RE::Actor*,float,RE::Actor*,bool);
    static const REL::Relocation<Damage> damageActor{REL::ID(37335)};
    // Early hits have no attacker credit that would start combat.
    if (loss>0.f) damageActor(&target,loss,self || tolerated ? nullptr : &player,!armedHit);
    if (armedHit && !target.IsDead()) applyWeaponEnchantment(player,target,*weapon);
    const float after = values->GetActorValue(RE::ActorValue::kHealth);
    SKSE::log::info("Flight {:08X} {} {:08X}: {:.2f} m/s, requested {:.2f} HP, health {:.2f} -> {:.2f}",
                   flight.id,self ? "return hit" : "NPC hit",targetID,speed,loss,health,after);
    if (!std::isfinite(after) || after>=health || target.IsDead()) {
        if (weaponHit || reaction==ToleranceReaction::limitReached) startCombat();
        return;
    }

    if (self) {
        const auto force = returnKnockdown(flight.weight,flight.metal,speed);
        auto* process = player.GetActorRuntimeData().currentProcess;
        if (force>0.f && process && !player.IsInRagdollState() && !player.IsInKillMove() &&
            !player.IsOnMount() && !player.GetOccupiedFurniture()) {
            auto origin = player.GetPosition();
            origin.z += player.GetHeight()*.55f;
            origin -= point(flight.previousVelocity.unit()*100.f);
            process->KnockExplosion(&player,origin,force);
            SKSE::log::info("Flight {:08X} return impact knocked the player down (weight {:.2f}, force {:.2f})",
                            flight.id,flight.weight,force);
        }
    } else {
        if (!sneakHit && !investigating) stagger(target,flight.previousPosition,.35f);
        if (!tolerated) startCombat();
    }
}

RE::Actor* impactActor(RE::PlayerCharacter& player, const Impact& impact)
{
    if (impact.actor) return RE::TESForm::LookupByID<RE::Actor>(impact.actor);
    if (!impact.controller) return nullptr;
    if (player.GetCharController()==impact.controller) return &player;
    RE::Actor* target = nullptr;
    if (auto* actors = RE::ProcessLists::GetSingleton()) {
        actors->ForEachHighActor([&](RE::Actor* actor) {
            if (actor && inFlightWorld(*actor) && actor->GetCharController()==impact.controller) {
                target = actor;
                return RE::BSContainer::ForEachResult::kStop;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }
    return target;
}

void checkReturn(RE::PlayerCharacter& player, RE::TESObjectREFR& object, Flight& flight, Vec position,
                 const Settings& settings, bool nativeContact, float speed)
{
    if (!settings.damageEnabled) return;
    const auto capsule = actorCapsule(player);
    const auto from = flight.previousPosition;
    const auto previous = approachCapsule(from,from,capsule.bottom,capsule.top);
    const auto current = approachCapsule(position,position,capsule.bottom,capsule.top);
    const auto path = approachCapsule(from,position,capsule.bottom,capsule.top);
    const auto clearance = capsule.radius+flight.sweepRadius;
    if (current.distance>clearance+12.f && flight.elapsed>.12f) flight.clearedThrower = true;
    const auto distance = nativeContact ? std::min(path.distance,clearance) : path.distance;
    const auto speedToward = flight.previousVelocity.dot((previous.axisPoint-from).unit());
    if (!returningImpact(flight.clearedThrower,flight.elapsed,speedToward,previous.distance,distance,clearance))
        return;
    if (!nativeContact) {
        const auto* body = native(*flight.body).GetCollidable();
        const auto line = ray(player,from,path.axisPoint,RE::COL_LAYER::kLOS,&object,body);
        if (line.fraction<.99f) return;
    }
    applyImpact(player,flight,player,std::max(speed,flight.previousVelocity.length()),settings);
}

void sweepFlight(RE::PlayerCharacter& player, RE::TESObjectREFR& object, Flight& flight,
                 Vec position, const Settings& settings)
{
    const auto from = flight.previousPosition;
    const float speed = flight.previousVelocity.length();
    if (speed<1.f || (position-from).length()<0.2f) return;
    const auto* body = native(*flight.body).GetCollidable();
    const auto path = ray(player,from,position,RE::COL_LAYER::kProjectile,&object,body);
    if (auto* actor = path.hit ? path.hit->As<RE::Actor>() : nullptr) {
        applyImpact(player,flight,*actor,speed,settings);
        return;
    }

    auto* actors = RE::ProcessLists::GetSingleton();
    if (!actors) return;
    RE::Actor* hit = nullptr;
    RE::Actor* nearby = nullptr;
    float nearestHit = path.fraction;
    float nearestMiss = settings.reactDistance;
    const bool mayReact = settings.reactions && !flight.reacted && speed>=3.f &&
                          (flight.sneakingThrow || reactionCooldown<=0.f);
    actors->ForEachHighActor([&](RE::Actor* actor) {
        using Result = RE::BSContainer::ForEachResult;
        if (!actor || actor==&player || !inFlightWorld(*actor) || !actor->Is3DLoaded() ||
            actor->IsDead() || actor->IsGhost()) return Result::kContinue;
        if (std::find(flight.targets.begin(),flight.targets.end(),actor->GetFormID())!=flight.targets.end())
            return Result::kContinue;

        const auto capsule = actorCapsule(*actor);
        const auto approach = approachCapsule(from,position,capsule.bottom,capsule.top);
        const auto surface = approach.distance-capsule.radius-flight.sweepRadius;
        const auto passed = from+(position-from)*approach.fraction;
        if (approach.fraction>path.fraction) return Result::kContinue;
        if (surface<=0.f && approach.fraction<=nearestHit) {
            const auto line = ray(player,from,approach.axisPoint,RE::COL_LAYER::kLOS,&object,body);
            if (line.fraction>=.99f || line.hit==actor) {
                nearestHit = approach.fraction;
                hit = actor;
            }
        } else if (mayReact && surface>0.f && surface<nearestMiss && !isInvestigating(*actor) &&
                   !actor->IsInCombat() && !actor->AsActorState()->IsUnconscious() && !actor->GetCurrentScene()) {
            const auto line = ray(player,passed,approach.axisPoint,RE::COL_LAYER::kLOS,&object,body);
            if (line.fraction>=.99f || line.hit==actor) {
                nearestMiss = surface;
                nearby = actor;
            }
        }
        return Result::kContinue;
    });
    if (hit) {
        applyImpact(player,flight,*hit,speed,settings);
    } else if (nearby) {
        stagger(*nearby,from,.12f);
        const bool actorUnaware = flight.sneakingThrow && player.IsSneaking() && unawareOfPlayer(player,*nearby);
        flightReaction(player,flight,*nearby,actorUnaware,settings);
        queueReactionInspection(flight,*nearby,settings);
        flight.reacted = true;
    }
}
}

bool hiddenSneaking(RE::PlayerCharacter& player)
{
    return player.IsSneaking() && useSuspicionVoice(true,true,true,highestPlayerDetection(player));
}

void loadReactionTopic()
{
    loadSuspicionVoices();
    loadInvestigationIdle();
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return;
    for (auto* topic : data->GetFormArray<RE::TESTopic>()) {
        if (!topic || topic->IsDeleted() || !topic->numTopicInfos || !topic->topicInfos ||
            !topic->ownerQuest || !topic->GetFile(0)) continue;
        const auto file = topic->GetFile(0)->GetFilename();
        const auto kind = topic->data.subtype.get();
        if (file!="Skyrim.esm" || std::string_view(topic->ownerQuest->GetFormEditorID())!="DialogueGeneric") continue;
        if (kind==RE::DIALOGUE_DATA::Subtype::kActorCollidewithActor && !vanillaReaction) vanillaReaction=topic;
        if (kind==RE::DIALOGUE_DATA::Subtype::kKnockOverObject &&
            (!vanillaReaction || vanillaReaction->data.subtype.get()!=kind))
            vanillaReaction = topic;
    }
    if (vanillaReaction)
        SKSE::log::info("Vanilla disturbance topic: {:08X} ({})",vanillaReaction->GetFormID(),
                        vanillaReaction->GetFormEditorID());
    else SKSE::log::warn("No Skyrim.esm DialogueGeneric disturbance topic resolved.");
}

RE::bhkRigidBody* uniqueBody(RE::NiAVObject* root)
{
    if (!root) return nullptr;
    RE::bhkRigidBody* result = nullptr;
    RE::BSVisit::TraverseScenegraphCollision(root,[&](RE::bhkNiCollisionObject* object) {
        auto* collision = netimmerse_cast<RE::bhkCollisionObject*>(object);
        auto* body = collision ? collision->GetRigidBody() : nullptr;
        if (!body || !body->referencedObject) return RE::BSVisit::BSVisitControl::kContinue;
        if (result && result!=body) {
            result = nullptr;
            return RE::BSVisit::BSVisitControl::kStop;
        }
        result = body;
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    return result;
}

std::optional<Vec> liveBodyPosition(RE::TESObjectREFR& ref)
{
    const auto* cell = ref.GetParentCell();
    auto* world = cell ? cell->GetbhkWorld() : nullptr;
    auto* body = uniqueBody(ref.Get3D());
    if (!world || !body) return std::nullopt;
    const RE::BSReadLockGuard lock(world->worldLock);
    const auto* activeWorld = world->GetWorld1();
    if (!activeWorld || native(*body).world!=activeWorld) return std::nullopt;
    const auto position = bodyPosition(*body);
    return std::isfinite(position.dot(position)) ? std::optional{position} : std::nullopt;
}

bool movable(RE::bhkRigidBody& wrapper)
{
    const auto& body = native(wrapper);
    const auto type = body.motion.type.get();
    return body.world && body.GetShape() && (type==RE::hkpMotion::MotionType::kDynamic ||
        type==RE::hkpMotion::MotionType::kBoxInertia || type==RE::hkpMotion::MotionType::kSphereInertia ||
        type==RE::hkpMotion::MotionType::kThinBoxInertia);
}

bool canHandleBody(RE::bhkRigidBody& wrapper, bool looseInstance, const RE::TESBoundObject* base)
{
    if (movable(wrapper)) return true;
    const auto& body = native(wrapper);
    const auto type = body.motion.type.get();
    if (!body.world || !body.GetShape() ||
        (type!=RE::hkpMotion::MotionType::kFixed && type!=RE::hkpMotion::MotionType::kKeyframed)) return false;
    if (body.motion.savedMotion) return true;
    const auto* collidable = body.GetCollidable();
    if (!collidable) return false;
    const auto layer = collidable->GetCollisionLayer();
    const auto restingLayer = layer==RE::COL_LAYER::kWeapon ? RestingPickupLayer::weapon :
        layer==RE::COL_LAYER::kClutter || layer==RE::COL_LAYER::kClutterLarge ?
            RestingPickupLayer::clutter : RestingPickupLayer::other;
    const bool supportedWeapon = isThrowableWeapon(base) ||
        (looseInstance && base && base->Is(RE::FormType::Ammo));
    return canRestoreRestingPickup(restingLayer,looseInstance,type==RE::hkpMotion::MotionType::kFixed,supportedWeapon);
}

std::optional<float> livePickupDistance(const RE::TESObjectREFR& ref, RE::bhkRigidBody& body,
                                        const RE::NiAVObject& root, Vec player)
{
    const auto* cell = ref.GetParentCell();
    auto* world = cell ? cell->GetbhkWorld() : nullptr;
    if (!world) return std::nullopt;
    const RE::BSReadLockGuard lock(world->worldLock);
    const auto* activeWorld = world->GetWorld1();
    if (!activeWorld || native(body).world!=activeWorld) return std::nullopt;
    // GetPosition() is the reference's stored location. A thrown item can
    // roll away from that point while its collision body still tracks the mesh.
    const auto reach = pickupReach(player,bodyPosition(body),root.worldBound.radius);
    if (!std::isfinite(reach)) return std::nullopt;
    return reach;
}

float estimatedArea(RE::TESBoundObject& item, float scale)
{
    const auto& bounds = item.boundData;
    const float mult = scale * RE::bhkWorld::GetWorldScale();
    const float x = std::abs(bounds.boundMax.x-bounds.boundMin.x)*mult;
    const float y = std::abs(bounds.boundMax.y-bounds.boundMin.y)*mult;
    const float z = std::abs(bounds.boundMax.z-bounds.boundMin.z)*mult;
    return (x*y+x*z+y*z)/3.0f;
}

ItemKind itemKind(RE::FormType type)
{
    switch (type) {
    case RE::FormType::AlchemyItem: return ItemKind::bottle;
    case RE::FormType::Book: return ItemKind::book;
    case RE::FormType::Scroll: return ItemKind::scroll;
    case RE::FormType::Ingredient: return ItemKind::ingredient;
    case RE::FormType::SoulGem: return ItemKind::gem;
    case RE::FormType::KeyMaster: return ItemKind::key;
    default: return ItemKind::miscellaneous;
    }
}

float itemDrag(RE::TESObjectREFR& ref, float area, const Settings& settings)
{
    auto* base = ref.GetBaseObject();
    auto kind = itemKind(base->GetFormType());
    if (auto* alchemy = base->As<RE::AlchemyItem>(); alchemy && alchemy->IsFood()) kind=ItemKind::food;
    return drag(ref.GetWeight(),area,kind,settings.dragScale)*throwDragMultiplier*equipmentFlight(*base).drag;
}

Vec launchVelocity(RE::PlayerCharacter& player, RE::TESObjectREFR& item, Vec release,
                   float area, const Settings& settings)
{
    const auto [from,forward] = cameraAim();
    if (forward.length()<.5f) return {};
    const auto distance = settings.aimDistance;
    const auto hit = ray(player,from,from+forward*distance,RE::COL_LAYER::kLOS);
    const auto target = from+forward*(distance*hit.fraction);
    const float speed = settings.metresPerSecond*settings.force*throwSpeedMultiplier*equipmentFlight(*item.GetBaseObject()).speed;
    auto motion = doubleThrowRange(std::abs(forward.z)>.995f && hit.fraction>=.99f ?
                                   forward*speed : aim(release,target,forward,speed));
    if (hit.fraction>0.f && hit.fraction<1.f && hit.hit && hit.hit->As<RE::Actor>()) {
        auto* cell = player.GetParentCell();
        auto* world = cell ? cell->GetbhkWorld() : nullptr;
        if (world && world->GetWorld1()) {
            const float scale = RE::bhkWorld::GetWorldScale();
            const float gravity = std::clamp(-vector(world->GetWorld1()->gravity).z,0.f,40.f);
            motion = ballisticActorThrow((target-release)*scale,forward,speed,gravity,itemDrag(item,area,settings));
        }
    }
    if (settings.trajectoryDebug)
        SKSE::log::info("Aim target ({:.2f},{:.2f},{:.2f}) launch ({:.2f},{:.2f},{:.2f}) m/s",
                       target.x,target.y,target.z,motion.x,motion.y,motion.z);
    return motion;
}

RE::TESObjectREFR* physicsPickupTarget(RE::PlayerCharacter& player, float distance)
{
    const auto [start,forward] = cameraAim();
    if (forward.length()<.5f) return nullptr;
    const auto extent = (start-vec(player.GetPosition())).length()+distance;
    const auto end = start+forward*extent;
    const auto pick = ray(player,start,end,RE::COL_LAYER::kItemPicker);
    if (pick.hit) return pick.hit;
    // A missed activation pick can still have a real rigid-body collision.
    // The projectile ray also keeps scenery between camera and item opaque.
    return ray(player,start,end,RE::COL_LAYER::kProjectile).hit;
}

RE::NiPointer<RE::Actor> toleranceAimTarget(RE::PlayerCharacter& player, float distance)
{
    if (auto* crosshair = RE::CrosshairPickData::GetSingleton()) {
        const auto selected = crosshair->target.get();
        if (auto* actor = selected ? selected->As<RE::Actor>() : nullptr) return RE::NiPointer<RE::Actor>(actor);
        const auto aimed = crosshair->targetActor.get();
        if (auto* actor = aimed ? aimed->As<RE::Actor>() : nullptr) return RE::NiPointer<RE::Actor>(actor);
    }
    const auto view = firstPersonView().aim;
    if (!(view.forward.length()>.5f) || !std::isfinite(view.position.x) ||
        !std::isfinite(view.position.y) || !std::isfinite(view.position.z)) return {};
    const auto end = view.position+view.forward*distance;
    const auto hit = ray(player,view.position,end,RE::COL_LAYER::kProjectile,nullptr,playerProxyCollider(player));
    auto* actor = hit.hit ? hit.hit->As<RE::Actor>() : nullptr;
    return RE::NiPointer<RE::Actor>(actor);
}

Vec firstPersonReleaseCenter(RE::PlayerCharacter& player, Vec center, float radius, int attempt)
{
    const auto capsule = actorCapsule(player);
    const auto feet = vec(player.GetPosition());
    const auto yaw = player.GetAngleZ();
    const auto view = cameraAim().forward;
    auto forward = Vec{view.x,view.y,0.f}.unit();
    if (std::abs(view.z)>.96f || forward.length()<.5f) forward = {std::sin(yaw),std::cos(yaw),0.f};
    const Vec right{forward.y,-forward.x,0.f};
    const float minimum = capsule.radius+radius+8.f;
    const float reach = std::clamp((center-feet).dot(forward),minimum,minimum+9.f);
    const float height = std::clamp(center.z,capsule.bottom.z+18.f,capsule.top.z+21.f);
    const auto base = Vec{feet.x,feet.y,height};
    if (attempt==0) return base+forward*reach+right*std::clamp((center-feet).dot(right),9.f,24.f);
    if (attempt==1) return base+forward*(minimum+6.f);
    return base+forward*(minimum*.82f)+right*(minimum*.65f);
}

namespace
{
bool prepareDropBounds(ReleaseBounds& box, float radius)
{
    if (!box.valid()) {
        if (!std::isfinite(radius) || radius<=1.f) return false;
        box = {{radius,radius,radius},{Vec{1,0,0},Vec{0,1,0},Vec{0,0,1}}};
    }
    box.half.x = std::max(1.f,box.half.x);
    box.half.y = std::max(1.f,box.half.y);
    box.half.z = std::max(1.f,box.half.z);
    return true;
}

bool clearDropVolume(RE::PlayerCharacter& player, const ReleaseBounds& box, Vec center,
                     Vec fall, const RE::hkpCollidable* playerBody)
{
    const auto margins = std::array{box.axes[0]*box.half.x,box.axes[1]*box.half.y,box.axes[2]*box.half.z};
    const bool checkFall = fall.z!=0.f;
    const auto clear = [&](Vec from, Vec to) {
        return ray(player,from,to,RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction>=.99f;
    };
    for (int x=-1; x<=1; ++x)
        for (int y=-1; y<=1; ++y)
            for (int z=-1; z<=1; ++z) {
                const auto offset = margins[0]*static_cast<float>(x)+
                                    margins[1]*static_cast<float>(y)+margins[2]*static_cast<float>(z);
                if ((x || y || z) && !clear(center,center+offset)) return false;
                if (checkFall && !clear(center+offset,center+offset+fall)) return false;
            }
    return true;
}
}

std::optional<Vec> thirdPersonEquipmentDropCenter(RE::PlayerCharacter& player, Vec heldCenter,
                                                  ReleaseBounds box, float radius)
{
    if (!prepareDropBounds(box,radius)) return std::nullopt;
    const auto capsule = actorCapsule(player);
    const auto torso = (capsule.bottom+capsule.top)*.5f;
    if (!((heldCenter-torso).length()<=150.f) || !std::isfinite(capsule.radius)) return std::nullopt;

    const auto fromBody = heldCenter-capsule.bottom;
    auto outward = Vec{fromBody.x,fromBody.y,0.f}.unit();
    if (outward.length()<.5f) {
        const auto yaw = player.GetAngleZ();
        if (!std::isfinite(yaw)) return std::nullopt;
        outward = {std::cos(yaw),-std::sin(yaw),0.f};
    }
    constexpr float maxAdjustment = 32.f;
    const float playerPadding = capsule.radius+2.f;
    const float belowFeet = capsule.bottom.z-capsule.radius-box.extent({0,0,1})-2.f;
    const auto* playerBody = playerProxyCollider(player);
    const auto points = equipmentHandDropPoints(box,heldCenter,capsule.bottom,outward,playerPadding);
    unsigned tooFar{}, playerBlocked{}, sceneryBlocked{};
    for (const auto center : points) {
        const float adjustment = (center-heldCenter).length();
        if (!(adjustment<=maxAdjustment) || !((center-torso).length()<=150.f)) {
            ++tooFar;
            continue;
        }
        const Vec fall{0,0,std::min(-2.f,belowFeet-center.z)};
        if (!sweptBoundsClearCapsule(box,center,fall,capsule.bottom,capsule.top,playerPadding)) {
            ++playerBlocked;
            continue;
        }
        // Start at the visible equipment. A ray from the torso may hit
        // something behind the hand that the released object never crosses.
        if ((adjustment>.05f &&
             ray(player,heldCenter,center,RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction<.99f) ||
            !clearDropVolume(player,box,center,{},playerBody)) {
            ++sceneryBlocked;
            continue;
        }
        return center;
    }
    SKSE::log::info("Equipment hand drop checked {} positions: {} beyond reach, {} player overlaps, {} scenery obstructions",
                    points.size(),tooFar,playerBlocked,sceneryBlocked);
    return std::nullopt;
}

std::optional<Vec> firstPersonDropCenter(RE::PlayerCharacter& player, ReleaseBounds box, float radius)
{
    if (!prepareDropBounds(box,radius)) return std::nullopt;
    const auto [view,nearPlane] = firstPersonView();
    const auto capsule = actorCapsule(player);
    const auto torso = (capsule.bottom+capsule.top)*.5f;
    const auto* playerBody = playerProxyCollider(player);
    constexpr Vec fall{0,0,-2};
    const float belowFeet = capsule.bottom.z-capsule.radius-box.extent({0,0,1})-2.f;
    for (const float extraDepth : {0.f,6.f,12.f,18.f}) {
        const auto center = viewDropPoint(box,view.position,view.forward,nearPlane,extraDepth);
        if (!center) break;
        const Vec pastPlayer{0,0,std::min(fall.z,belowFeet-center->z)};
        if (!((*center-torso).length()<=145.f) ||
            !sweptBoundsClearCapsule(box,*center,pastPlayer,capsule.bottom,capsule.top,capsule.radius+2.f)) continue;
        if (ray(player,view.position,*center,RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction>=.99f &&
            clearDropVolume(player,box,*center,fall,playerBody)) return center;
    }
    return std::nullopt;
}

std::optional<Vec> groundDropCenter(RE::PlayerCharacter& player, ReleaseBounds box, float radius)
{
    if (!prepareDropBounds(box,radius)) return std::nullopt;
    const auto feet = vec(player.GetPosition());
    const auto capsule = actorCapsule(player);
    const auto torso = (capsule.bottom+capsule.top)*.5f;
    const auto yaw = player.GetAngleZ();
    if (!std::isfinite(yaw) || !std::isfinite(feet.length()) || !std::isfinite(capsule.radius))
        return std::nullopt;
    const auto* playerBody = playerProxyCollider(player);
    constexpr Vec up{0,0,1};
    constexpr Vec fall{0,0,-2};
    const auto footprints = groundDropFootprints(box,feet,{std::sin(yaw),std::cos(yaw),0.f},capsule.radius);
    for (const auto footprint : footprints) {
        if ((footprint-feet).length()>110.f) continue;
        const auto above = footprint+up*std::clamp(capsule.top.z-feet.z,24.f,76.f);
        const auto below = footprint-up*42.f;
        const auto floor = ray(player,above,below,RE::COL_LAYER::kDroppingPick,nullptr,playerBody);
        if (!(floor.fraction>0.f && floor.fraction<1.f) || (floor.hit && floor.hit->As<RE::Actor>())) continue;
        const auto surface = above+(below-above)*floor.fraction;
        for (const float gap : {5.f,12.f,22.f}) {
            const auto center = surface+up*(box.extent(up)+gap);
            if ((center-torso).length()<=145.f &&
                sweptBoundsClearCapsule(box,center,fall,capsule.bottom,capsule.top,capsule.radius+2.f) &&
                ray(player,torso,center,RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction>=.99f &&
                clearDropVolume(player,box,center,fall,playerBody)) return center;
        }
    }
    return std::nullopt;
}

std::optional<ThrowRelease> firstPersonThrow(RE::PlayerCharacter& player, RE::TESObjectREFR& item,
                                            Vec heldCenter, float radius, float area, const Settings& settings)
{
    const auto [view,nearPlane] = firstPersonView();
    const auto [eye,forward] = view;
    auto* cell = player.GetParentCell();
    auto* world = cell ? cell->GetbhkWorld() : nullptr;
    if (!world || !world->GetWorld1() || !(forward.length()>.5f) ||
        !std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z)) return std::nullopt;

    const auto* playerBody = playerProxyCollider(player);
    const auto hit = ray(player,eye,eye+forward*settings.aimDistance,RE::COL_LAYER::kLOS,nullptr,playerBody);
    const bool hasTarget = hit.fraction>0.f && hit.fraction<1.f;
    const float hitDepth = settings.aimDistance*(hasTarget ? hit.fraction : 1.f);
    const auto target = eye+forward*hitDepth;
    const float speed = settings.metresPerSecond*settings.force*throwSpeedMultiplier*equipmentFlight(*item.GetBaseObject()).speed;
    if (speed<=1.f) return std::nullopt;
    const float gravity = std::clamp(-vector(world->GetWorld1()->gravity).z,0.f,40.f);
    const float scale = RE::bhkWorld::GetWorldScale();
    const auto drag = itemDrag(item,area,settings);
    const auto aimInThrowSpace = referenceThrowSpace(forward).unit();
    const auto directVelocity = [&](Vec center) { return cursorVelocity(aim(center,target,forward,1.f),speed); };
    const auto velocityFor = [&](Vec center) {
        if (hasTarget) {
            const auto correction = ballisticCorrection(referenceThrowSpace((target-center)*scale),
                                                        aimInThrowSpace,speed,gravity,drag);
            if (correction) return doubleThrowRange(*correction);
        }
        return directVelocity(center);
    };

    const auto launch = selectFirstPersonRelease([&](FirstPersonOrigin origin) -> std::optional<ThrowRelease> {
        if (origin==FirstPersonOrigin::crosshair) {
            const float lastDepth = std::min(150.f,hitDepth-radius-2.f);
            for (float depth=nearPlane+radius+4.f; depth<=lastDepth; depth+=6.f) {
                const auto center = eye+forward*depth;
                const auto velocity = velocityFor(center);
                if (clipsThrower(player,center,radius,velocity)) continue;
                const auto end = center+velocity.unit()*std::clamp(radius,5.f,25.f);
                if (ray(player,eye,center+forward*(radius+2.f),RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction<.99f)
                    return std::nullopt;
                if (clearReleaseMargins(player,center,end,radius,playerBody)) return ThrowRelease{center,velocity};
            }
            return std::nullopt;
        }

        // Use the proven first-person release positions when the view ray
        // cannot fit the object. Every candidate still needs clear space.
        auto center = heldCenter;
        switch (origin) {
        case FirstPersonOrigin::forward: center = firstPersonReleaseCenter(player,heldCenter,radius,1); break;
        case FirstPersonOrigin::right: center = firstPersonReleaseCenter(player,heldCenter,radius,0); break;
        case FirstPersonOrigin::diagonal: center = firstPersonReleaseCenter(player,heldCenter,radius,2); break;
        default: break;
        }
        const auto safeVelocity = [&](Vec velocity) {
            return !clipsThrower(player,center,radius,velocity) &&
                   clearRelease(player,center,radius,velocity.unit(),playerBody);
        };
        const auto velocity = velocityFor(center);
        if (safeVelocity(velocity)) return ThrowRelease{center,velocity};
        const auto direct = directVelocity(center);
        if ((direct-velocity).length()>.01f && safeVelocity(direct)) return ThrowRelease{center,direct};
        return std::nullopt;
    });
    if (settings.trajectoryDebug)
        SKSE::log::info("First-person aim target ({:.2f},{:.2f},{:.2f}), depth {:.2f}, geometry hit {}, launch candidate {}",
                        target.x,target.y,target.z,hitDepth,hasTarget,launch ? static_cast<int>(launch->origin) : -1);
    return launch;
}

std::optional<ThrowRelease> thirdPersonThrow(RE::PlayerCharacter& player, RE::TESObjectREFR& item,
                                            Vec heldCenter, Vec centerOffset, ReleaseBounds box,
                                            float radius, float area, const Settings& settings)
{
    const auto* playerBody = playerProxyCollider(player);
    const auto velocityAt = [&](Vec center) {
        return launchVelocity(player,item,center-centerOffset,area,settings);
    };
    if (!box.valid()) {
        if (!std::isfinite(radius) || radius<=1.f) return std::nullopt;
        box = {{radius,radius,radius},{Vec{1,0,0},Vec{0,1,0},Vec{0,0,1}}};
    }

    box.half.x = std::max(1.f,box.half.x);
    box.half.y = std::max(1.f,box.half.y);
    box.half.z = std::max(1.f,box.half.z);
    const auto capsule = actorCapsule(player);
    const auto torso = (capsule.bottom+capsule.top)*.5f;
    const auto view = cameraAim().forward;
    auto forward = Vec{view.x,view.y,0.f}.unit();
    if (forward.length()<.5f || std::abs(view.z)>.96f) {
        const float yaw = player.GetAngleZ();
        forward = {std::sin(yaw),std::cos(yaw),0.f};
    }
    const Vec right{forward.y,-forward.x,0.f};
    const auto candidates = thirdPersonReleaseCandidates(box,heldCenter,torso,forward,right,
                                                         capsule.bottom,capsule.top,capsule.radius);
    const auto axisX = box.axes[0]*box.half.x;
    const auto axisY = box.axes[1]*box.half.y;
    const auto axisZ = box.axes[2]*box.half.z;
    const std::array supports{
        axisX,axisX*-1.f,axisY,axisY*-1.f,axisZ,axisZ*-1.f,
        axisX+axisY+axisZ,axisX+axisY-axisZ,axisX-axisY+axisZ,axisX-axisY-axisZ,
        axisY+axisZ-axisX,axisY-axisX-axisZ,axisZ-axisX-axisY,(axisX+axisY+axisZ)*-1.f
    };
    for (const auto center : candidates) {
        const auto reach = (center-torso).length();
        if (!std::isfinite(reach) || reach>150.f ||
            ray(player,torso,center,RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction<.99f) continue;
        const auto velocity = velocityAt(center);
        const float speed = velocity.length();
        if (!std::isfinite(speed) || speed<=1.f) continue;
        const auto playerSweep = velocity*(.035f*RE::bhkWorld::GetWorldScaleInverse());
        if (!sweptBoundsClearCapsule(box,center,playerSweep,capsule.bottom,capsule.top,capsule.radius+2.f))
            continue;
        // The rigid body handles impacts once it leaves the release point.
        // Check immediate clearance here so a nearby stair remains a valid target.
        const auto travel = velocity.unit()*3.f;
        const auto clear = [&](Vec start, Vec end) {
            return ray(player,start,end,RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction>=.99f;
        };
        const auto clearPoint = [&](Vec offset) {
            return clear(center,center+offset) && clear(center+offset,center+offset+travel);
        };
        if (!clear(center,center+travel)) continue;
        if (std::all_of(supports.begin(),supports.end(),clearPoint)) return ThrowRelease{center,velocity};
    }
    return std::nullopt;
}

bool clearRelease(RE::PlayerCharacter& player, Vec release, float radius, Vec direction,
                  const RE::hkpCollidable* playerBody)
{
    const auto torso = vec(player.GetPosition())+Vec{0,0,56};
    const auto reach = (release-torso).length();
    if (!std::isfinite(reach) || reach<radius+9 || reach>150) return false;
    const auto end = release+direction*std::clamp(radius,5.0f,25.0f);
    if (ray(player,torso,end,RE::COL_LAYER::kDroppingPick,nullptr,playerBody).fraction<.99f) return false;
    return clearReleaseMargins(player,release,end,radius,playerBody);
}

bool clipsThrower(RE::PlayerCharacter& player, Vec center, float radius, Vec velocity)
{
    const auto capsule = actorCapsule(player);
    const auto end = center+velocity*(0.035f*RE::bhkWorld::GetWorldScaleInverse());
    const auto path = approachCapsule(center,end,capsule.bottom,capsule.top);
    return path.distance<capsule.radius+radius;
}

static bool activatePhysics(RE::TESObjectREFR& ref, RE::bhkRigidBody& body, Vec velocity,
                     float area, bool sneakingThrow, bool hiddenAtRelease, const Settings& settings)
{
    auto* cell = ref.GetParentCell();
    auto* world = cell ? cell->GetbhkWorld() : nullptr;
    if (!world || !movable(body) || native(body).world!=world->GetWorld1()) return false;
    const bool thrown = velocity.length()>1.f;
    if (thrown && !observe(*world)) return false;
    const float requestedSpeed = velocity.length();
    float actualSpeed{};
    Vec position;
    Vec measured;
    const auto* base = thrown ? ref.GetBaseObject() : nullptr;
    const bool spinWeapon = isThrowableWeapon(base);
    float spinRate{};
    bool metal{};
    {
        const RE::BSWriteLockGuard lock(world->worldLock);
        body.SetAngularVelocity(havok({}));
        body.SetLinearVelocity(havok(velocity));
        measured = vector(native(body).motion.linearVelocity);
        actualSpeed = measured.length();
        if (thrown && (!std::isfinite(actualSpeed) || actualSpeed<=1.f || measured.unit().dot(velocity.unit())<.99f)) {
            body.SetLinearVelocity(havok({}));
            return false;
        }
        if (spinWeapon) {
            Vec lengthAxis{};
            if (const auto* root = ref.Get3D()) {
                const auto& min = base->boundData.boundMin;
                const auto& max = base->boundData.boundMax;
                const Vec dimensions{float(max.x-min.x),float(max.y-min.y),float(max.z-min.z)};
                lengthAxis = vec(root->world.rotate*point(weaponLengthAxis(dimensions)));
            }
            body.SetAngularVelocity(havok(weaponAngularVelocity(measured,lengthAxis)));
            spinRate = vector(native(body).motion.angularVelocity).length();
        }
        position = bodyPosition(body);
        const auto* shape = native(body).GetShape();
        using enum RE::MATERIAL_ID;
        const auto material = shape && shape->userData ? shape->userData->materialID : kNone;
        metal = material==kMetalLight || material==kMetalSolid || material==kMetalHeavy || material==kChainMetal;
    }
    if (thrown) {
        if (std::abs(actualSpeed-requestedSpeed)>.03f)
            SKSE::log::warn("Launch for {:08X} used measured speed {:.2f} m/s (requested {:.2f})",
                            ref.GetFormID(),actualSpeed,requestedSpeed);
        installContactHook();
        auto slot = std::find_if(flights.begin(),flights.end(),[](const auto& flight) { return !flight.body; });
        if (slot==flights.end()) slot=std::max_element(flights.begin(),flights.end(),
            [](const auto& a,const auto& b) { return a.elapsed<b.elapsed; });
        const auto index = static_cast<std::size_t>(slot-flights.begin());
        forget(index);
        const auto weight = ref.GetWeight();
        const auto launch = ++nextLaunch;
        *slot = {ref.GetHandle(),RE::NiPointer<RE::bhkRigidBody>(&body),ref.GetFormID(),launch,0.f,0.f,
                 itemDrag(ref,area,settings),actualSpeed,weight,position,measured,
                 std::clamp(std::sqrt(std::max(0.f,area))*RE::bhkWorld::GetWorldScaleInverse()*0.35f,0.f,12.f),
                 false,metal,false,sneakingThrow,false,{}};
        slot->inspectWeaponMiss = isOneHandedWeapon(base);
        const bool canDistract = !spinWeapon || slot->inspectWeaponMiss;
        slot->distractionThrow = settings.reactions && sneakingThrow && hiddenAtRelease && canDistract;
        slot->notice.enabled = slot->distractionThrow;
        slot->notice.scanFrom = position;
        slot->notice.quietAnchor = position;
        if (sneakingThrow && settings.reactions && canDistract)
            SKSE::log::info("Distraction {:08X}: {}",ref.GetFormID(),
                            slot->notice.enabled ? "tracking a hidden throw" : "no hidden release");
        listener.track(index,&native(body),launch,slot->notice.enabled);
        if (settings.log || settings.trajectoryDebug)
            SKSE::log::info("Launched {:08X}: requested={:.3f} m/s measured={:.3f} m/s drag={:.4f}/m weight={:.3f} area={:.4f} m2",
                           ref.GetFormID(),requestedSpeed,actualSpeed,slot->appliedDrag,weight,area);
        if (spinWeapon && (settings.log || settings.trajectoryDebug))
            SKSE::log::info("Weapon spin {:08X}: {:.2f} rotations/s at release",
                            ref.GetFormID(),spinRate/(2.f*std::numbers::pi_v<float>));
    }
    return true;
}

bool initializeReleasedPhysics(RE::PlayerCharacter& player, RE::TESObjectREFR& ref, Vec velocity,
                               float area, bool sneakingThrow, bool hiddenAtRelease, const Settings& settings)
{
    const auto grabbed = player.GetGrabbedRef();
    if (grabbed.get()==&ref) return false;
    auto* body = uniqueBody(ref.Get3D());
    if (!body || !canHandleBody(*body,true,ref.GetBaseObject())) return false;
    if (!movable(*body)) {
        // This is the specific reference already released by Throw Object.
        // Do not change the physics of scenery or a Skyrim-grabbed reference.
        ref.SetMotionType(RE::TESObjectREFR::MotionType::kDynamic,true);
        body = uniqueBody(ref.Get3D());
    }
    return body && activatePhysics(ref,*body,velocity,area,sneakingThrow,hiddenAtRelease,settings);
}

void updateFlights(RE::PlayerCharacter& player, float dt, const Settings& settings)
{
    updateSuspicion(dt);
    if (settings.enabled && settings.reactions) updateInvestigations(player,dt,settings.investigationKneel);
    else clearInvestigations();
    if (!currentWorld) return;
    auto* cell = player.GetParentCell();
    if (!cell || cell->GetbhkWorld()!=currentWorld.get()) return clearFlights();
    reactionCooldown = std::max(0.0f,reactionCooldown-dt);
    suspicionCooldown = std::max(0.0f,suspicionCooldown-dt);
    if (auto grabbed = player.GetGrabbedRef()) stopTracking(*grabbed);
    const auto contacts = listener.drain();
    for (std::size_t i=0; i<flights.size(); ++i) {
        auto& flight = flights[i];
        if (!flight.body) continue;
        auto ref = flight.object.get();
        flight.elapsed += dt;
        if (!ref || ref->IsDisabled() || ref->IsDeleted() || flight.elapsed>8.0f ||
            uniqueBody(ref->Get3D())!=flight.body.get() || !movable(*flight.body)) {
            if (flight.notice.enabled)
                SKSE::log::info("Distraction {:08X}: tracking ended before landing dispatch at {:.2f} s, {} witness(es)",
                                flight.id,flight.elapsed,flight.notice.witnesses.size());
            forget(i);
            continue;
        }
        auto& body = *flight.body;
        float referenceSpeed;
        Vec position, current;
        {
            const RE::BSWriteLockGuard lock(currentWorld->worldLock);
            current = vector(native(body).motion.linearVelocity);
            referenceSpeed = referenceThrowSpace(current).length();
            if (referenceSpeed>0.3f && flight.appliedDrag>0)
                body.SetLinearVelocity(havok(applyThrowDrag(current,flight.appliedDrag,dt)));
            position = bodyPosition(body);
        }
        const auto previousSpeed = flight.previousVelocity.length();
        for (const auto& impact : contacts.impacts) {
            if (impact.launch!=flight.launch) continue;
            auto* actor = impactActor(player,impact);
            if (actor==&player) checkReturn(player,*ref,flight,position,settings,true,impact.speed);
            else if (actor) applyImpact(player,flight,*actor,std::max(impact.speed,previousSpeed),settings);
        }
        checkReturn(player,*ref,flight,position,settings,false,0.f);
        if (settings.damageEnabled || settings.crime || settings.reactions)
            sweepFlight(player,*ref,flight,position,settings);
        flight.previousPosition = position;
        flight.previousVelocity = current;
        flight.stillTime = referenceSpeed<.5f ? flight.stillTime+dt : 0.f;
        noticeDistraction(player,*ref,flight,position,contacts.touches[i],dt,flight.stillTime>.35f,settings);
        if (flight.stillTime>.35f) forget(i);
    }
    if (std::none_of(flights.begin(),flights.end(),[](const auto& flight) { return bool(flight.body); }))
        removeListener();
}

void stopTracking(const RE::TESObjectREFR& ref)
{
    for (std::size_t i=0; i<flights.size(); ++i) {
        if (flights[i].body && flights[i].id==ref.GetFormID()) {
            SKSE::log::info("Pickup took reference {:08X}; stopped its previous throw tracking",ref.GetFormID());
            forget(i);
        }
    }
}

bool flightActive() { return currentWorld || investigationsActive() || suspicionActive(); }

void cancelDistractions()
{
    clearInvestigations();
    for (auto& flight : flights) {
        flight.notice = {};
        flight.distractionThrow = false;
    }
}

void clearFlights()
{
    cancelDistractions();
    stopSuspicion();
    for (std::size_t i=0; i<flights.size(); ++i) forget(i);
    listener.drain();
    removeListener();
    reactionCooldown = 0.0f;
    suspicionCooldown = 0.0f;
}
}
