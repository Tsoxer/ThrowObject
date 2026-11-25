#include "core.hpp"
#include "held_recovery.hpp"
#include "input_filter.hpp"
#include "tolerance.hpp"

#include <cstdio>
#include <cstdlib>

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

int main()
{
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
