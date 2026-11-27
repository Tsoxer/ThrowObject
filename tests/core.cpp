#include "core.hpp"
#include "distraction_rules.hpp"
#include "equipment.hpp"
#include "release_clearance.hpp"
#include "held_recovery.hpp"
#include "input_filter.hpp"
#include "tolerance.hpp"
#include "voice_clip.hpp"
#include "weapon_grip.hpp"
#include "weapon_spin.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace to;

void check(bool result, const char* assertion)
{
    if (!result) {
        std::fprintf(stderr,"%s\n",assertion);
        std::exit(1);
    }
}
#define CHECK(test) check((test),#test)

Vec groundContact(Vec position, Vec velocity, float dragPerMetre, bool extended)
{
    constexpr float dt = 1.0f/240.0f;
    while (position.z > 0.0f) {
        velocity.z -= 9.81f*dt;
        velocity = extended ? applyThrowDrag(velocity,dragPerMetre,dt) : applyDrag(velocity,dragPerMetre,dt);
        position = position+velocity*dt;
    }
    return position;
}

float range(float weight, float area, ItemKind kind, float tuning, float speed, bool extended=false)
{
    const auto base = Vec{1,0,0.32f}.unit()*speed;
    return groundContact({0,0,1.4f},extended ? doubleThrowRange(base) : base,drag(weight,area,kind,tuning),extended).x;
}

std::vector<std::uint8_t> voiceFixture(bool compressed)
{
    std::vector<std::uint8_t> data{'F','U','Z','E',1,0,0,0,1,0,0,0,0,
                                  'R','I','F','F',0,0,0,0};
    const auto tag = [&](std::string_view text) { data.insert(data.end(),text.begin(),text.end()); };
    const auto size = [&](std::uint8_t length) { data.insert(data.end(),{length,0,0,0}); };
    tag(compressed ? "XWMA" : "WAVE");
    tag("fmt ");
    size(compressed ? 18 : 16);
    data.insert(data.end(),{std::uint8_t(compressed ? 0x61 : 1),std::uint8_t(compressed),
                           1,0,0x40,0x1f,0,0,0x80,0x3e,0,0,std::uint8_t(compressed ? 4 : 2),0,16,0});
    if (compressed) {
        data.insert(data.end(),{0,0});
        tag("dpds");
        size(8);
        data.insert(data.end(),{16,0,0,0,32,0,0,0});
    }
    tag("data");
    size(compressed ? 8 : 32);
    data.resize(data.size()+(compressed ? 8 : 32));
    data[17] = static_cast<std::uint8_t>(data.size()-21);
    return data;
}

int main()
{
    CHECK(kneelDuration({}).minimum==4.f && kneelDuration({}).maximum==7.f);
    CHECK(migrateKneelDefaults({6.f,9.f},0).minimum==4.f && migrateKneelDefaults({6.f,9.f},0).maximum==7.f);
    CHECK(migrateKneelDefaults({6.f,9.f},1).minimum==6.f && migrateKneelDefaults({6.f,9.f},1).maximum==9.f);
    CHECK(migrateKneelDefaults({5.f,10.f},0).minimum==5.f && migrateKneelDefaults({5.f,10.f},0).maximum==10.f);
    CHECK(kneelDuration({12.f,4.f}).maximum==12.f);
    CHECK(kneelDuration({-4.f,90.f}).minimum==1.f && kneelDuration({-4.f,90.f}).maximum==60.f);
    CHECK(!readyToInvestigate(false,.10f,0.f,false));
    CHECK(readyToInvestigate(false,.22f,0.f,false));
    CHECK(readyToInvestigate(false,.06f,0.f,true));
    CHECK(!readyToInvestigate(true,0.f,.5f,false));
    CHECK(readyToInvestigate(true,0.f,.9f,false));
    CHECK(readyToInvestigate(true,.12f,.3f,false));
    CHECK(besideForInspection({0,0,0},{43,-42,10}));
    CHECK(!besideForInspection({0,0,0},{81,0,0}));
    CHECK(!besideForInspection({0,0,0},{10,0,-93}));
    CHECK(kneelingPose(56.f,40.f));
    CHECK(kneelingPose(28.f,20.f));
    CHECK(!kneelingPose(56.f,52.f));
    CHECK(!kneelingPose(56.f,0.f));
    CHECK(!kneelingPose(56.f,std::numeric_limits<float>::infinity()));
    using enum InspectionRoute;
    CHECK(inspectionRoute(.25f,.22f,true,false)==wait);
    CHECK(inspectionRoute(.50f,.10f,true,false)==wait);
    CHECK(inspectionRoute(.55f,.18f,true,true)==wait);
    CHECK(inspectionRoute(.55f,.18f,true,false)==kneel);
    // A walking witness can pass the resting object while recovering from a hit.
    CHECK(inspectionRoute(.65f,.20f,false,false)==approach);
    CHECK(inspectionRoute(1.25f,.20f,false,true)==approach);
    CHECK(inspectionRoute(1.25f,.20f,true,true)==kneel);
    const Vec passFrom{-200,0,60}, passTo{200,0,60};
    CHECK(hearsThrow(passFrom,passTo,{0,70,20},{0,70,112},95.f));
    CHECK(hearsThrow(passFrom,passTo,{0,-70,20},{0,-70,112},95.f));
    CHECK(!hearsThrow(passFrom,passTo,{0,100,20},{0,100,112},95.f));
    CHECK(hearsThrow({0,0,0},{0,0,0},{0,30,20},{0,30,112},95.f));
    CHECK(!hearsThrow({0,0,0},{0,0,0},{0,30,130},{0,30,220},95.f));
    const std::array<std::uint32_t,1> impactVictims{2};
    CHECK(eligibleThrowWitness(true,impactVictims,1));
    CHECK(!eligibleThrowWitness(true,impactVictims,2));
    CHECK(eligibleThrowWitness(false,impactVictims,2));
    // The guard in the live report heard the pass. The final rolling
    // position was outside hearing range and must not erase that witness.
    CHECK(hearsThrow(passFrom,passTo,{0,70,20},{0,70,112},95.f));
    CHECK(!hearsThrow({200,0,0},{200,0,0},{0,70,20},{0,70,112},95.f));
    CHECK(useSuspicionVoice(true,true,true,-25));
    CHECK(useSuspicionVoice(true,true,true,0));
    CHECK(!useSuspicionVoice(true,true,true,12));  // Another observer detected the player.
    CHECK(!useSuspicionVoice(true,true,false,-25));
    CHECK(!useSuspicionVoice(true,false,true,-25));
    CHECK(!useSuspicionVoice(false,true,true,-25));
    CHECK(!useSuspicionVoice(true,true,true,std::nullopt));
    for (const bool compressed : {false,true}) {
        auto fixture = voiceFixture(compressed);
        const auto clip = readVoiceClip(fixture);
        CHECK(clip && clip->durationMs==2 && clip->packetTable.size()==(compressed ? 8u : 0u));
        CHECK(!readVoiceClip(std::span(fixture).first(fixture.size()-1)));
        if (compressed) {
            fixture[clip->packetTable.data()-fixture.data()+4] = 8;
            CHECK(!readVoiceClip(fixture));
        }
        fixture[8] = 255;
        CHECK(!readVoiceClip(fixture));
    }
    for (std::uint32_t type=0; type<=10; ++type)
        CHECK(oneHandedWeapon(type)==(type>=1 && type<=4));
    CHECK(oneHandedWeapon(0,0x000E3C16));
    CHECK(oneHandedWeapon(6,0x000E3C16));
    CHECK(!oneHandedWeapon(6,0x010E3C16));
    CHECK(!oneHandedWeapon(8,0x000E3C17));
    CHECK(weaponEquipment(8)==Equipment::staff);
    CHECK(weaponEquipment(9)==Equipment::crossbow && !oneHandedWeapon(9));
    CHECK(weaponEquipment(1)==Equipment::oneHanded);
    CHECK(weaponEquipment(5)==Equipment::none && weaponEquipment(6)==Equipment::none &&
          weaponEquipment(7)==Equipment::none);
    CHECK(equipmentFlight(Equipment::crossbow,false).drag>equipmentFlight(Equipment::oneHanded,false).drag);
    CHECK(equipmentFlight(Equipment::backpack,true).drag>equipmentFlight(Equipment::backpack,false).drag);
    CHECK(weaponLengthAxis({70,4,9}).x==1.f);
    CHECK(weaponLengthAxis({9,70,4}).y==1.f);
    CHECK(weaponLengthAxis({4,9,70}).z==1.f);
    CHECK(weaponLengthAxis({}).length()==0.f);
    const auto standardGrip = weaponGrip({0,20,0},{2,30,1});
    CHECK(standardGrip && standardGrip->handle.length()==0.f);
    CHECK(standardGrip->rows[0].x==1.f && standardGrip->rows[1].y==1.f && standardGrip->rows[2].z==1.f);
    const auto reversedGrip = weaponGrip({-20,0,0},{30,2,3});
    CHECK(reversedGrip && reversedGrip->handle.length()==0.f && reversedGrip->rows[1].x==-1.f);
    const auto offsetGrip = weaponGrip({8,0,20},{2,3,20});
    CHECK(offsetGrip && offsetGrip->handle.x==8.f && std::abs(offsetGrip->handle.z-7.2f)<.001f);
    CHECK(offsetGrip->rows[0].x==1.f && offsetGrip->rows[1].z==1.f && offsetGrip->rows[2].y==-1.f);
    const auto centeredGrip = weaponGrip({},{2,30,3});
    CHECK(centeredGrip && std::abs(centeredGrip->handle.y+19.2f)<.001f);
    CHECK(!weaponGrip({},{}));
    const std::array<GripTriangle,2> offsetShaft{{
        {{{6,0,-2},{10,0,2},{10,100,2}}},
        {{{6,0,-2},{10,100,2},{6,100,-2}}}
    }};
    // The record describes a Y-axis weapon. This replacer runs along -X,
    // with a short offset handle around its authored pivot and a long blade.
    const std::array<GripTriangle,4> rotatedMelee{{
        {{{6,6,-1},{6,10,1},{-6,10,1}}},
        {{{6,6,-1},{-6,10,1},{-6,6,-1}}},
        {{{-6,10,1},{-6,16,-1},{-84,13,0}}},
        {{{-6,6,-1},{-6,10,1},{-84,13,0}}}
    }};
    const auto fittedMelee = fittedWeaponGrip({0,40,0},{2,45,2},GripProfile::fittedMelee,rotatedMelee);
    CHECK(fittedMelee && fittedMelee->rows[1].x==-1.f);
    CHECK(std::abs(fittedMelee->handle.x)<.001f && std::abs(fittedMelee->handle.y-8.f)<.001f);
    const auto fittedHaft = fittedWeaponGrip({8,50,0},{2,50,2},GripProfile::fittedMelee,offsetShaft);
    CHECK(fittedHaft && std::abs(fittedHaft->handle.x-8.f)<.001f && std::abs(fittedHaft->handle.y-18.f)<.001f);
    const auto noCpuGeometry = fittedWeaponGrip({0,20,0},{2,30,1},GripProfile::fittedMelee,{});
    CHECK(noCpuGeometry && noCpuGeometry->handle.length()==0.f);
    const auto fittedSpear = fittedWeaponGrip({8,50,0},{2,50,2},GripProfile::spear,offsetShaft);
    CHECK(fittedSpear && std::abs(fittedSpear->handle.x-8.f)<.001f && std::abs(fittedSpear->handle.y-45.f)<.001f);
    const auto fallbackStaff = fittedWeaponGrip({0,50,0},{2,50,2},GripProfile::staff,{});
    CHECK(fallbackStaff && std::abs(fallbackStaff->handle.y-45.f)<.001f);
    const auto untouched = fittedWeaponGrip({0,20,0},{2,30,1},GripProfile::standard,offsetShaft);
    CHECK(untouched && untouched->handle.length()==0.f);
    CHECK(std::abs(weaponVisualLocalScale(1.6f,1.35f)*1.35f-1.6f)<.001f);
    CHECK(weaponVisualLocalScale(1.25f,0.f)==1.25f);
    for (const auto elbowToWrist : {Vec{-.319f,.098f,.943f},Vec{0,0,1},Vec{1,0,0},Vec{}}) {
        const auto [x,y,z] = daggerGripRotation(elbowToWrist);
        const Vec blade{x.y,y.y,z.y};  // The weapon frame's +Y column.
        const Vec face{x.z,y.z,z.z};
        CHECK(std::abs(blade.dot(elbowToWrist))<.001f);
        CHECK(std::abs(blade.length()-1.f)<.001f && std::abs(face.length()-1.f)<.001f);
        CHECK(std::abs(blade.dot(face))<.001f);
    }
    const auto pitchSpin = weaponAngularVelocity({0,12,4},{0,0,1});
    CHECK(std::abs(pitchSpin.x+5.f*std::numbers::pi_v<float>)<.001f);
    CHECK(pitchSpin.y==0.f && pitchSpin.z==0.f);
    for (const auto velocity : {Vec{7,11,2},Vec{0,13,0},Vec{0,0,12},Vec{0,0,-12}}) {
        for (const auto length : {Vec{1,0,0},Vec{0,1,0},Vec{0,0,1},Vec{2,1,2},Vec{}}) {
            const auto spin = weaponAngularVelocity(velocity,length);
            CHECK(std::abs(spin.length()-weaponSpinRate)<.001f);
            CHECK(std::abs(spin.dot(length))<.001f);
        }
    }
    constexpr bool pickaxeIsOneHanded = oneHandedWeapon(3);
    CHECK(canRestoreRestingPickup(RestingPickupLayer::weapon,false,true,pickaxeIsOneHanded));
    CHECK(canRestoreRestingPickup(RestingPickupLayer::weapon,true,false,pickaxeIsOneHanded));
    CHECK(canRestoreRestingPickup(RestingPickupLayer::clutter,false,false,pickaxeIsOneHanded));
    CHECK(!canRestoreRestingPickup(RestingPickupLayer::other,true,true,pickaxeIsOneHanded));
    CHECK(!canRestoreRestingPickup(RestingPickupLayer::weapon,true,true,oneHandedWeapon(6)));
    CHECK(canRestoreRestingPickup(RestingPickupLayer::clutter,true,true,false));
    CHECK(!canRestoreRestingPickup(RestingPickupLayer::clutter,false,true,false));
    CHECK(!canRestoreRestingPickup(RestingPickupLayer::clutter,true,false,false));
    constexpr std::uint32_t head=1u<<0, hair=1u<<1, body=1u<<2, hands=1u<<3, forearms=1u<<4,
        ring=1u<<6, feet=1u<<7, calves=1u<<8, shield=1u<<9, circlet=1u<<12;
    CHECK(armorEquipment(head|hair|circlet)==Equipment::headwear);
    CHECK(armorEquipment(hands|forearms)==Equipment::gloves);
    CHECK(armorEquipment(feet|calves)==Equipment::boots);
    CHECK(armorEquipment(shield|hands)==Equipment::none);
    CHECK(armorEquipment(body|head|hands|feet)==Equipment::none);
    CHECK(armorEquipment(head|feet)==Equipment::none);
    CHECK(armorEquipment(ring|circlet)==Equipment::none);
    constexpr std::uint32_t back = 1u<<17, alternatePackSlot = 1u<<26;
    CHECK(armorEquipment(back)==Equipment::none);
    CHECK(backpackSlots(back) && backpackSlots(alternatePackSlot));
    CHECK(!backpackSlots(0) && !backpackSlots(body|back) && !backpackSlots(shield|back));
    CHECK(backpackLabel("Adventurer's BACKPACK") && backpackLabel("Leather_BackPack_GND.nif"));
    CHECK(backpackLabel("RuckSack") && !backpackLabel("Hooded Cloak") && !backpackLabel("Field Lantern"));
    CHECK(backpackKeyword("BagTypeBackpack") && backpackKeyword("CampfireBackpack") &&
          !backpackKeyword("CanAttachToBackpack"));
    CHECK(equipmentFlight(Equipment::none,true).speed==1.f);
    CHECK(equipmentFlight(Equipment::none,false).drag==1.f);
    CHECK(equipmentFlight(Equipment::gloves,true).drag>equipmentFlight(Equipment::gloves,false).drag);
    CHECK(equipmentFlight(Equipment::oneHanded,false).speed>equipmentFlight(Equipment::boots,false).speed);
    CHECK(equipmentFlight(Equipment::staff,false).speed<equipmentFlight(Equipment::oneHanded,false).speed);
    CHECK(resistedSwingDamage(40.f,40.f,10.f,0.f)==30.f);
    CHECK(resistedSwingDamage(40.f,80.f,20.f,0.f)==30.f);
    CHECK(resistedSwingDamage(40.f,40.f,60.f,0.f)==0.f);
    CHECK(resistedSwingDamage(40.f,0.f,0.f,0.f)==40.f);
    CHECK(resistedSwingDamage(40.f,40.f,10.f,0.f)*thrownSwingDamageScale==15.f);
    constexpr ReleaseBounds longBlade{{1.f,48.f,2.f},{Vec{1,0,0},Vec{0,1,0},Vec{0,0,1}}};
    constexpr Vec feetAxis{0,0,14}, headAxis{0,0,57};
    const ReleaseBounds arrowBundle{{2,36,2},longBlade.axes};
    const Vec arrowHand{23,0,40};
    CHECK((arrowHand-Vec{0,0,56}).length()<arrowBundle.half.length()+9.f);
    CHECK(sweptBoundsClearCapsule(arrowBundle,arrowHand,{0,0,-55},feetAxis,headAxis,16.f));
    CHECK(longBlade.valid());
    CHECK(sweptBoundsClearCapsule(longBlade,{19,20,35},{15,10,0},feetAxis,headAxis,14));
    CHECK(!sweptBoundsClearCapsule(longBlade,{10,20,35},{15,10,0},feetAxis,headAxis,14));
    CHECK(!sweptBoundsClearCapsule(longBlade,{19,20,35},{-30,0,0},feetAxis,headAxis,14));
    const auto drops = groundDropFootprints(longBlade,{10,20,0},{0,1,0},14);
    CHECK(drops[0].x==10.f && drops[0].y==90.f);
    CHECK(drops[3].x==33.f && drops[3].y==20.f);
    CHECK(drops[8].y==drops[0].y+14.f);
    CHECK(sweptBoundsClearCapsule(longBlade,drops[3]+Vec{0,0,7},{0,0,-2},
                                  feetAxis+Vec{10,20,0},headAxis+Vec{10,20,0},16));
    constexpr ReleaseBounds cabbage{{8,8,8},longBlade.axes};
    constexpr Vec eye{0,0,105}, facing{0,1,0};
    const auto visibleDrop = viewDropPoint(cabbage,eye,facing,15.f,0.f);
    CHECK(visibleDrop && visibleDrop->z<eye.z && visibleDrop->y==36.f);
    CHECK(sweptBoundsClearCapsule(cabbage,*visibleDrop,{0,0,-115},feetAxis,headAxis,16));
    CHECK(!sweptBoundsClearCapsule(cabbage,{0,0,110},{0,0,-115},feetAxis,headAxis,16));
    const auto bladeDrop = viewDropPoint(longBlade,eye,facing,15.f,0.f);
    CHECK(bladeDrop && bladeDrop->y-longBlade.extent(facing)==21.f);
    CHECK(!viewDropPoint(longBlade,eye,facing,15.f,18.f));
    CHECK(!viewDropPoint(cabbage,eye,{0,0,-1},15.f,0.f));
    CHECK(!viewDropPoint(cabbage,eye,{},15.f,0.f));
    const auto lookingDown = Vec{0,4,-3}.unit();
    const auto downhillDrop = viewDropPoint(cabbage,eye,lookingDown,15.f,0.f);
    CHECK(downhillDrop && std::abs((*downhillDrop-eye).dot(lookingDown)-36.f)<.001f &&
          downhillDrop->z<(eye+lookingDown*36.f).z);
    // A vertical sword beside the hand fits even though a sphere wide enough
    // to contain its length would overlap the player.
    constexpr ReleaseBounds uprightBlade{{1.f,2.f,48.f},longBlade.axes};
    CHECK(sweptBoundsClearCapsule(uprightBlade,{19,21,61},{},feetAxis,headAxis,16));
    CHECK(!sweptBoundsClearCapsule(uprightBlade,{8,14,61},{},feetAxis,headAxis,16));
    const auto equippedDrop = [&](const ReleaseBounds& shape, Vec held) {
        const auto outward = Vec{held.x,held.y,0.f}.unit();
        const auto points = equipmentHandDropPoints(shape,held,feetAxis,outward,16.f);
        CHECK((points.front()-held).length()==0.f);
        return std::any_of(points.begin(),points.end(),[&](Vec center) {
            return (center-held).length()<=32.f && center.z>=held.z && center.z<=held.z+10.f &&
                sweptBoundsClearCapsule(shape,center,{0,0,-80},feetAxis,headAxis,16);
        });
    };
    CHECK(equippedDrop(uprightBlade,{19,0,55}));   // A sword already clear of the shoulder.
    CHECK(equippedDrop(longBlade,{0,45,55}));      // A horizontal weapon held toward the front.
    CHECK(equippedDrop(longBlade,{0,21,55}));      // Clear the blade's narrow side near the body.
    CHECK(equippedDrop({{17,11,13},longBlade.axes},{21,0,55}));  // Helmet.
    CHECK(equippedDrop({{12,18,6},longBlade.axes},{21,0,55}));   // Boots.
    CHECK(equippedDrop({{11,8,4},longBlade.axes},{21,0,55}));    // Gloves.
    CHECK(!equippedDrop({{75,75,75},longBlade.axes},{21,0,55}));
    CHECK(!ReleaseBounds{}.valid());
    constexpr ReleaseBounds crate{{17.f,18.f,20.f},longBlade.axes};
    constexpr Vec heldCenter{25,22,42};
    const auto releases = thirdPersonReleaseCandidates(crate,heldCenter,{0,0,36},{0,1,0},{1,0,0},feetAxis,headAxis,14);
    CHECK((releases[0]-heldCenter).length()==0.f);
    CHECK(releases[1].x==heldCenter.x && releases[1].y==heldCenter.y && releases[1].z>heldCenter.z);
    CHECK(releases[3].x>heldCenter.x && releases[3].z>heldCenter.z);
    CHECK(sweptBoundsClearCapsule(crate,releases[3],{0,10,-3},feetAxis,headAxis,14));
    CHECK(!sweptBoundsClearCapsule(crate,releases[3],{-30,0,0},feetAxis,headAxis,14));

    DailyTolerance guard;
    constexpr std::uint32_t guardID = 0x22;
    const auto six = [] { return 6; };
    const auto guardStart = guard.observe(1,guardID,six);
    CHECK(guardStart.hits==0 && guardStart.limit==6);
    const auto guardHit = guard.hit(1,guardID,six,ToleranceHit::exhaust);
    CHECK(guardHit.hits==6 && guardHit.limit==6 && guardHit.escalates());
    const auto guardSave = unpackTolerance(packTolerance(guardHit));
    CHECK(guardSave && guardSave->hits==6 && guardSave->limit==6);
    CHECK(guard.hit(2,guardID,six).hits==1);
    CHECK(guard.hit(2,guardID,six,ToleranceHit::exhaust).hits==6);

    constexpr Recovery saved{0xFF001234u,0x00027F15u};
    auto recovery = saved;
    int enableAttempts = 0;
    const auto failedEnable = [&] { ++enableAttempts; return false; };
    const auto enable = [&] { ++enableAttempts; return true; };
    CHECK(recoverHeldReference(recovery,0,false,false,enable)==RecoveryResult::waiting);
    CHECK(recovery.id==saved.id && recovery.base==saved.base && enableAttempts==0);
    CHECK(recoverHeldReference(recovery,saved.base,false,true,failedEnable)==RecoveryResult::waiting);
    CHECK(recovery.id==saved.id && recovery.base==saved.base && enableAttempts==1);
    CHECK(recoverHeldReference(recovery,saved.base,false,true,enable)==RecoveryResult::restored);
    CHECK(recovery.id==0 && recovery.base==0 && enableAttempts==2);

    for (const auto referenceBase : {0u,saved.base+1,saved.base}) {
        const auto deleted = referenceBase==saved.base;
        recovery = saved;
        CHECK(recoverHeldReference(recovery,referenceBase,deleted,true,enable)==RecoveryResult::stale);
        CHECK(recovery.id==0 && recovery.base==0 && enableAttempts==2);
    }
    recovery = saved;
    CHECK(recoverHeldReference(recovery,saved.base,false,false,enable)==RecoveryResult::restored);
    CHECK(recovery.id==0 && recovery.base==0 && enableAttempts==3);

    const auto upperLeft = placeToleranceMeter(0,0.f,0.f,1920.f,1080.f);
    const auto upperRight = placeToleranceMeter(1,0.f,0.f,1920.f,1080.f);
    const auto lowerLeft = placeToleranceMeter(2,200.f,100.f,1920.f,1080.f);
    const auto lowerRight = placeToleranceMeter(3,200.f,100.f,1920.f,1080.f);
    CHECK(upperLeft && upperLeft->x==20.f && upperLeft->y==20.f &&
          upperLeft->pivotX==0.f && upperLeft->pivotY==0.f);
    CHECK(upperRight && upperRight->x==1900.f && upperRight->y==20.f &&
          upperRight->pivotX==1.f && upperRight->pivotY==0.f);
    CHECK(lowerLeft && lowerLeft->x==220.f && lowerLeft->y==1160.f &&
          lowerLeft->pivotX==0.f && lowerLeft->pivotY==1.f);
    CHECK(lowerRight && lowerRight->x==2100.f && lowerRight->y==1160.f &&
          lowerRight->pivotX==1.f && lowerRight->pivotY==1.f);
    CHECK(!placeToleranceMeter(3,0.f,0.f,0.f,1080.f));
    CHECK(!placeToleranceMeter(4,0.f,0.f,1920.f,1080.f));

    const auto priorDay = toleranceDay(201,7,16,23.f);
    const auto beforeSix = toleranceDay(201,7,17,5.f+59.f/60.f);
    const auto atSix = toleranceDay(201,7,17,6.f);
    CHECK(priorDay && atSix && beforeSix==priorDay && *atSix==*priorDay+1);
    CHECK(toleranceDay(201,7,17,23.f)==atSix);
    CHECK(toleranceDay(201,7,18,5.f)==atSix);
    CHECK(toleranceDay(202,0,1,0.f)==toleranceDay(201,11,31,23.f));
    CHECK(toleranceDay(201,1,29,6.f)==std::nullopt);
    CHECK(toleranceDay(201,12,1,6.f)==std::nullopt);
    CHECK(toleranceDay(201,7,17,24.f)==std::nullopt);
    CHECK(toleranceRange(1,20).minimum==1 && toleranceRange(1,20).maximum==20);
    CHECK(toleranceRange(40,3).minimum==10 && toleranceRange(40,3).maximum==10);
    CHECK(toleranceRange(-1,40).minimum==1 && toleranceRange(-1,40).maximum==20);

    DailyTolerance daily;
    const auto first = 0x1234u, second = 0x5678u;
    int rolls = 0;
    const auto four = [&] { ++rolls; return 4; };
    const auto seven = [&] { ++rolls; return 7; };
    CHECK(!daily.hit(*atSix,first,four).escalates());
    CHECK(daily.hit(*atSix,second,seven).limit==7);
    CHECK(daily.hit(*atSix,first,seven).hits==2);
    CHECK(daily.hit(*atSix,first,seven).hits==3 && rolls==2);

    DailyTolerance reloaded;
    reloaded.beginDay(*daily.day());
    for (const auto& [id,count] : daily.entries()) {
        const auto decoded = unpackTolerance(packTolerance(count));
        CHECK(decoded.has_value());
        reloaded.restore(id,*decoded);
    }
    CHECK(reloaded.hit(*atSix,first,seven).escalates() && rolls==2);
    CHECK(reloaded.hit(*atSix,first,seven).hits==4 && rolls==2);
    CHECK(reloaded.hit(*atSix,second,four).hits==2 && rolls==2);
    const auto nextDay = reloaded.hit(*atSix+1,first,seven);
    CHECK(nextDay.hits==1 && nextDay.limit==7 && !nextDay.escalates() && rolls==3);
    CHECK(reloaded.entries().size()==1);
    CHECK(!unpackTolerance(0) && !unpackTolerance(packTolerance({20,21})) &&
          !unpackTolerance(packTolerance({21,1})) && !unpackTolerance(0x10000u|packTolerance({4,3})));
    const auto viewed = reloaded.observe(*atSix+1,second,four);
    CHECK(viewed.hits==0 && viewed.limit==4 && rolls==4);
    CHECK(reloaded.observe(*atSix+1,second,seven).hits==0 && rolls==4);
    const auto viewedPayload = unpackTolerance(packTolerance(viewed));
    CHECK(viewedPayload && viewedPayload->hits==0 && viewedPayload->limit==4);
    CHECK(reloaded.hit(*atSix+1,second,seven).hits==1 && rolls==4);

    // The reference's last stored location can stay near the release point
    // while the body has rolled ahead. Reach must follow the live object.
    constexpr Vec release{100.f,100.f,0.f};
    constexpr Vec settled{450.f,100.f,0.f};
    constexpr float pickupLimit = 135.f;
    CHECK(pickupReach(settled,release,10.f)>pickupLimit);
    CHECK(pickupReach(settled,settled,10.f)<=pickupLimit);
    CHECK(pickupReach(release,settled,10.f)>pickupLimit);
    CHECK(pickupReach({},Vec{145.f,0.f,0.f},10.f)==pickupLimit);
    CHECK(pickupReach({},Vec{149.f,0.f,0.f},100.f)>pickupLimit);
    CHECK(pickupReach({},Vec{0.f,0.f,148.f},12.f)>pickupLimit);

    CHECK(!returningImpact(false,.8f,8.f,24.f,10.f,14.f));
    CHECK(!returningImpact(true,.07f,8.f,24.f,10.f,14.f));
    CHECK(!returningImpact(true,.9f,-8.f,24.f,10.f,14.f));
    CHECK(!returningImpact(true,.9f,8.f,13.f,10.f,14.f));
    CHECK(!returningImpact(true,.9f,8.f,24.f,15.f,14.f));
    CHECK(returningImpact(true,.9f,8.f,24.f,10.f,14.f));
    CHECK(returnKnockdown(1.f,true,14.f)==0.f);
    CHECK(returnKnockdown(5.f,false,14.f)==0.f);
    CHECK(returnKnockdown(5.f,true,2.f)==0.f);
    CHECK(returnKnockdown(5.f,true,14.f)>0.f);
    CHECK(returnKnockdown(10.f,false,14.f)>0.f);
    CHECK(returnKnockdown(100.f,true,99.f)==6.f);
    CHECK(std::abs(projectedExtent({2,4,8},{0,0,1})-8.f)<1e-5f);
    CHECK(std::abs(projectedExtent({2,4,8},{0,.6f,-.8f})-8.8f)<1e-5f);
    constexpr Vec outward{1,0,0};
    constexpr std::array identity{Vec{1,0,0},Vec{0,1,0},Vec{0,0,1}};
    constexpr std::array landmarks{Vec{-4,0,-1},Vec{2,0,.5f}};
    const auto centered = gripOpening({0,10,0},{2,8,2},{0,12,2},{0,11,2});
    CHECK((centered-Vec{.8f,8.9f,1.6f}).length()<1e-5f);
    const auto limited = gripOpening({0,10,0},{50,8,2},{50,12,2},{50,11,2});
    CHECK(std::abs((limited-Vec{0,5.5f,0}).length()-5.f)<1e-5f);
    const auto coin = hoverOffset({2,2,.2f},identity,outward,landmarks);
    CHECK(coin.x==0.f && std::abs(coin.z-2.2f)<1e-5f);
    const auto upright = hoverOffset({14,12,16},identity,outward,landmarks);
    CHECK(std::abs(upright.x-2.88f)<1e-5f);
    CHECK(std::abs(upright.z-(.5f+16.f+1.5f))<1e-5f);
    CHECK(upright.length()<.5f+16.f+4.f);
    const float diagonal = std::sqrt(.5f);
    const std::array tilted{Vec{diagonal,0,-diagonal},Vec{0,1,0},Vec{diagonal,0,diagonal}};
    const Vec bucket{14,12,16};
    const auto held = hoverOffset(bucket,tilted,outward,landmarks);
    const float oldLift = .5f+diagonal*(bucket.x+bucket.z)+4.f;
    CHECK(held.x>4.f && held.x<=6.f && held.z<oldLift);
    CHECK(held.length()<oldLift);
    CHECK((held-landmarks[1]).dot(tilted[2])>=bucket.z+1.49f);
    const auto cross = approachCapsule({-8,0,5},{8,0,5},{0,0,2},{0,0,8});
    CHECK(cross.distance<1e-5f && std::abs(cross.fraction-.5f)<1e-5f);
    CHECK(approachCapsule({-8,5,5},{8,5,5},{0,0,2},{0,0,8}).distance==5.f);
    CHECK(approachCapsule({-8,0,12},{8,0,12},{0,0,2},{0,0,8}).distance==4.f);
    CHECK(approachCapsule({3,0,0},{3,0,15},{0,0,2},{0,0,8}).distance==3.f);
    CHECK(sameCarryArea(11,2,12,2)); // Neighboring exterior cells.
    CHECK(sameCarryArea(11,0,11,0));
    CHECK(!sameCarryArea(11,0,12,0) && !sameCarryArea(11,2,12,3) && !sameCarryArea(11,2,0,2));
    CHECK(afterPickupTimeout(Phase::pickup,2.3f,false)==Phase::idle);
    CHECK(afterPickupTimeout(Phase::lifting,2.1f,true)==Phase::lifting);
    CHECK(afterPickupTimeout(Phase::lifting,2.3f,true)==Phase::held);
    CHECK(afterPickupTimeout(Phase::held,30.f,true)==Phase::held);
    Phase p = Phase::idle;
    for (const auto event : {Event::grab,Event::release,Event::drop,Event::requestThrow})
        CHECK(transition(p,event)==Phase::idle);
    p=transition(p,Event::requestPickup);
    CHECK(p==Phase::pickup && transition(p,Event::requestPickup)==p);
    CHECK(transition(p,Event::pickupEnd)==p);
    CHECK(transition(p,Event::requestThrow)==p);
    p=transition(p,Event::grab);
    CHECK(p==Phase::lifting && transition(p,Event::grab)==p);
    CHECK(transition(p,Event::drop)==Phase::idle);
    p=transition(p,Event::pickupEnd);
    CHECK(p==Phase::held && transition(p,Event::requestPickup)==p);
    CHECK(transition(p,Event::drop)==Phase::idle);
    p=transition(p,Event::requestThrow);
    CHECK(p==Phase::throwing && transition(p,Event::requestThrow)==p);
    p=transition(p,Event::release);
    CHECK(p==Phase::released && transition(p,Event::release)==p);
    CHECK(transition(p,Event::throwEnd)==Phase::idle);
    for (auto phase : {Phase::idle,Phase::pickup,Phase::lifting,Phase::held,Phase::throwing,Phase::released})
        CHECK(transition(phase,Event::cancel)==Phase::idle);
    CHECK(fps==30 && grabFrame==22 && releaseFrame==15 && pickupEndFrame==45 && throwEndFrame==28);
    CHECK((defaultBindings==std::array{34,18,256}));
    CHECK(validKeys(defaultBindings) && validKeys({0,0,256}));
    CHECK(!validKeys({34,34,256}) && !validKeys({34,18,264}));
    CHECK(bindingCode(Device::keyboard,76)==76);
    CHECK(bindingCode(Device::mouse,0)==256 && bindingCode(Device::mouse,7)==263);
    CHECK(bindingCode(Device::mouse,8)==0 && bindingCode(Device::unsupported,76)==0);
    CHECK(matchingAction({76,18,256},76)==Action::pickUp);
    CHECK(!matchingAction({0,18,256},0));
    CHECK(!acceptsInput(Action::drop,Phase::idle,false));
    CHECK(!acceptsInput(Action::drop,Phase::pickup,false));
    CHECK(!acceptsInput(Action::drop,Phase::throwing,false));
    CHECK(acceptsInput(Action::drop,Phase::held,true));
    CHECK(acceptsInput(Action::pickUp,Phase::idle,false));
    CHECK(!acceptsInput(Action::pickUp,Phase::held,true));
    CHECK(!acceptsInput(Action::throwItem,Phase::released,false));

    KeyCapture capture;
    CHECK(!capture.input(76,true,false));
    capture.listen(Action::pickUp);
    CHECK(!capture.input(256,true,false,true)); // click on Cancel
    CHECK(!capture.input(256,false,true,true));
    CHECK(capture.isListening());
    CHECK(capture.input(76,true,false));
    const auto numpad=capture.take();
    CHECK(numpad && numpad->action==Action::pickUp && numpad->key==76);
    capture.cancel(); // framework may close before key-up
    CHECK(capture.input(76,false,false));
    CHECK(capture.input(76,false,true));
    CHECK(!capture.input(76,true,false));
    capture.listen(Action::throwItem);
    CHECK(capture.input(257,true,false));
    const auto mouse=capture.take();
    CHECK(mouse && mouse->action==Action::throwItem && mouse->key==257);
    CHECK(capture.input(257,false,true));
    capture.listen(Action::drop);
    capture.cancel();
    CHECK(!capture.isListening() && !capture.input(75,true,false));

    struct MockEvent { int scan; MockEvent* next; };
    MockEvent activateUp{18,nullptr}, other{29,&activateUp}, activateDown{18,&other};
    auto* original = &activateDown;
    filterInputList(&original,
        [](const auto& event) {
            const auto action = matchingAction(defaultBindings,event.scan);
            return action && acceptsInput(*action,Phase::idle,false);
        },
        [&](auto* list) {
            CHECK(list==&original && *list==&activateDown);
            CHECK((*list)->next==&other && other.next==&activateUp);
        });
    filterInputList(&original,
        [](const auto& event) {
            const auto action = matchingAction(defaultBindings,event.scan);
            return action && acceptsInput(*action,Phase::held,true);
        },
        [&](auto* list) {
            CHECK(*list==&other && !other.next);
        });
    CHECK(original==&activateDown && activateDown.next==&other && other.next==&activateUp);

    const Vec hand{12,0,1}, forward{0,1,0};
    const float speed=13;
    const auto centeredRay = screenCenterDirection({2.f,0.f,0.f},{0.f,0.f,1.5f},forward);
    CHECK((centeredRay-forward).length()<1e-5f);
    CHECK(screenCenterDirection({1,0,0},{2,0,0},forward).length()==0.f);
    const Vec projectionX{2.f,-.4f,0.f}, projectionY{0.f,-.25f,1.5f};
    const auto offAxis = screenCenterDirection(projectionX,projectionY,forward);
    CHECK(offAxis.y>.9f && offAxis.x>.1f && offAxis.z>.1f);
    for (const float depth : {20.f,60.f,120.f}) {
        const auto fromEye = offAxis*depth;
        CHECK(std::abs(fromEye.dot(projectionX))<1e-4f);
        CHECK(std::abs(fromEye.dot(projectionY))<1e-4f);
    }
    for (const Vec direction : {centeredRay,offAxis,Vec{.25f,.65f,-.8f}.unit(),Vec{0.f,0.f,1.f}}) {
        const auto launch = cursorVelocity(direction,speed);
        CHECK((launch.unit()-direction).length()<1e-5f);
        CHECK(std::abs(launch.length()-doubleThrowRange(direction*speed).length())<1e-5f);
    }
    CHECK(!ballisticCorrection({0.f,30.f,0.f},forward,speed,9.81f,.12f));
    CHECK(ballisticCorrection({0.f,4.f,0.f},forward,speed,9.81f,.12f).has_value());
    const auto simulatedClearance = [&](FirstPersonOrigin firstSafe) {
        return selectFirstPersonRelease([&](FirstPersonOrigin origin) -> std::optional<ThrowRelease> {
            if (origin!=firstSafe) return std::nullopt;
            const auto center = origin==FirstPersonOrigin::crosshair ? Vec{0.f,35.f,0.f} : Vec{14.f,35.f,0.f};
            const auto towardCursor = aim(center,{0.f,120.f,0.f},forward,1.f);
            return ThrowRelease{center,cursorVelocity(towardCursor,speed)};
        });
    };
    const auto centeredLaunch = simulatedClearance(FirstPersonOrigin::crosshair);
    CHECK(centeredLaunch && centeredLaunch->origin==FirstPersonOrigin::crosshair);
    for (const auto fallback : {FirstPersonOrigin::forward,FirstPersonOrigin::held,
                                FirstPersonOrigin::right,FirstPersonOrigin::diagonal}) {
        const auto launch = simulatedClearance(fallback);
        CHECK(launch && launch->origin==fallback);
        CHECK(launch->velocity.x<0.f && launch->velocity.y>0.f);
        CHECK(std::abs(launch->center.x+85.f*launch->velocity.x/launch->velocity.y)<1e-4f);
    }
    const auto allClear = selectFirstPersonRelease([](FirstPersonOrigin) -> std::optional<ThrowRelease> {
        return ThrowRelease{};
    });
    CHECK(allClear && allClear->origin==FirstPersonOrigin::crosshair);
    CHECK(!selectFirstPersonRelease([](FirstPersonOrigin) -> std::optional<ThrowRelease> { return std::nullopt; }));
    for (float z : {-9.f,1.f,40.f}) {
        const Vec launch=aim(hand,{0,60,z},forward,speed);
        CHECK(std::abs(launch.length()-speed)<1e-5f);
        CHECK((z<1 && launch.z<0) || (z==1 && launch.z==0) || (z>1 && launch.z>0));
        CHECK(launch.x<0);
    }
    CHECK(std::abs(aim(hand,hand+Vec{0,-15,0},forward,speed).y-speed)<1e-5f);
    const auto lob = ballisticAim({0,0,0},{0,4,0},forward,speed,9.81f,0.12f);
    CHECK(std::abs(lob.length()-speed)<1e-5f && lob.z>1.f && lob.z<3.f);
    const auto unreachable = ballisticAim({0,0,0},{0,30,0},forward,speed,9.81f,0.12f);
    CHECK(unreachable.z==0.f);
    const auto actorThrow = ballisticActorThrow({0,8,0},forward,speed,9.81f,.12f);
    CHECK(std::abs(actorThrow.y-lob.y*2.f)<1e-5f && std::abs(actorThrow.z-lob.z)<1e-5f);
    auto velocity = actorThrow;
    Vec position{};
    for (int step=0; position.y<8.f && step<240; ++step) {
        velocity.z -= 9.81f/120.f;
        velocity = applyThrowDrag(velocity,.12f,1.f/120.f);
        const auto next = position+velocity*(1.f/120.f);
        if (next.y>=8.f)
            CHECK(std::abs(std::lerp(position.z,next.z,(8.f-position.y)/(next.y-position.y)))<.02f);
        position = next;
    }
    CHECK(position.y>=8.f);

    struct Material { float weight, area; ItemKind kind; };
    constexpr std::array materials{
        Material{1.f,.045f,ItemKind::miscellaneous}, Material{.12f,.045f,ItemKind::food},
        Material{.5f,.06f,ItemKind::bottle}, Material{1.f,.30f,ItemKind::book},
        Material{.2f,.10f,ItemKind::scroll}, Material{.1f,.01f,ItemKind::ingredient},
        Material{.3f,.025f,ItemKind::gem}, Material{.01f,.001f,ItemKind::key},
        Material{3.f,.95f,ItemKind::miscellaneous}
    };
    for (const auto& m : materials) {
        for (float tuning : {0.f,1.f,3.f}) {
            const auto dragPerMetre = drag(m.weight,m.area,m.kind,tuning);
            for (float pitch : {-.4f,0.f,.65f}) {
                const Vec start{0,0,1.4f};
                const auto base = Vec{.6f,-.8f,pitch}.unit()*speed;
                const auto old = groundContact(start,base,dragPerMetre,false);
                const auto updated = groundContact(start,doubleThrowRange(base),dragPerMetre,true);
                CHECK((updated-doubleThrowRange(old)).length()<.0001f);
            }
            const auto oldFlat = groundContact({0,0,1.4f},{0,2.f*speed,0},dragPerMetre,true);
            const auto newFlat = groundContact({0,0,1.4f},{0,2.f*speed*throwSpeedMultiplier,0},
                                               dragPerMetre*throwDragMultiplier,true);
            CHECK(newFlat.y>oldFlat.y);
        }
    }
    const auto light=range(.12f,.045f,ItemKind::food,1,speed);
    const auto dense=range(1.0f,.045f,ItemKind::miscellaneous,1,speed);
    const auto bulky=range(1.0f,.30f,ItemKind::book,1,speed);
    CHECK(dense>light+2.f);
    CHECK(dense>bulky+2.f);
    CHECK(range(.12f,.045f,ItemKind::food,0,speed)==range(9,.3f,ItemKind::book,0,speed));
    CHECK(std::abs(applyDrag({0,speed,0},.25f,.50f).y-
                   applyDrag(applyDrag({0,speed,0},.25f,.25f),.25f,.25f).y)<1e-5f);
    CHECK(damage(0,speed,speed,1,25)>=1 && damage(0,speed,speed,1,25)<2);
    CHECK(damage(2,speed,speed,1,25)>damage(.15f,speed,speed,1,25));
    CHECK(damage(12,speed,speed,1,25)>damage(2,speed,speed,1,25));
    CHECK(damage(12,speed,speed,3,25)==25);
    CHECK(damage(12,.01f,speed,1,25)==0);
    std::printf("13 m/s base launch, simulated range in metres: light %.2f to %.2f, dense %.2f to %.2f, bulky %.2f to %.2f\n",
                light,range(.12f,.045f,ItemKind::food,1,speed,true),dense,range(1.f,.045f,ItemKind::miscellaneous,1,speed,true),
                bulky,range(1.f,.30f,ItemKind::book,1,speed,true));
}
