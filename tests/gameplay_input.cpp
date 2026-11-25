#include "gameplay_input.hpp"
#include "input_filter.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>

using namespace to;

void check(bool result, const char* expression)
{
    if (result) return;
    std::cerr << expression << '\n';
    std::exit(1);
}
#define CHECK(expression) check((expression),#expression)

int main()
{
    GameplayInput input;
    constexpr std::array native{activateKey,unbound};
    input.publish(Phase::idle,0,true,native);
    std::thread worker([&] {
        const auto pickup = input.button(34,true,false,defaultBindings,false);
        CHECK(pickup.queued==Action::pickUp && !pickup.consume);
        CHECK(!input.button(34,false,false,defaultBindings,false).queued);
        CHECK(!input.button(34,false,true,defaultBindings,false).consume);
    });
    worker.join();
    const auto first = input.take();
    CHECK(first.count==1 && currentInput(first.requests[0],defaultBindings,Phase::idle,0,false));
    CHECK(!currentInput(first.requests[0],defaultBindings,Phase::idle,0,false,
                        first.requests[0].pressed+std::chrono::seconds{1}));
    CHECK(input.take().count==0);

    const std::array rebound{76,activateKey,mouseBase};
    CHECK(!currentInput(first.requests[0],rebound,Phase::idle,0,false));
    CHECK(!input.button(34,true,false,rebound,false).queued);
    CHECK(!input.button(76,true,false,rebound,true).queued);
    CHECK(input.button(76,true,false,rebound,false).queued==Action::pickUp);

    struct Button { int key; bool down,up; Button* next; };
    Button up{activateKey,false,true,nullptr};
    Button held{activateKey,false,false,&up};
    Button down{activateKey,true,false,&held};
    auto* head = &down;
    filterInputList(&head,[&](auto& button) {
        return input.button(button.key,button.down,button.up,rebound,false).consume;
    },[&](auto* list) {
        CHECK(list==&head && *list==&down);
        CHECK(down.next==&held && held.next==&up && !up.next);
    });
    auto batch = input.take();
    CHECK(batch.count==1 && !batch.nativeActivateHeld);

    // Vanilla owns the press even during the interval before Skyrim starts a grab.
    CHECK(!input.button(activateKey,true,false,rebound,false).consume);
    CHECK(!input.button(76,true,false,rebound,false).queued);
    CHECK(input.take().nativeActivateHeld);
    CHECK(!input.button(activateKey,false,true,rebound,false).consume);
    CHECK(!input.take().nativeActivateHeld);
    CHECK(input.button(76,true,false,rebound,false).queued==Action::pickUp);
    CHECK(!input.button(activateKey,true,false,rebound,false).consume);
    batch = input.take();
    CHECK(batch.count==1 && batch.nativeActivateHeld);
    CHECK(!currentInput(batch.requests[0],rebound,Phase::idle,0,batch.nativeActivateHeld));
    input.button(activateKey,false,true,rebound,false);

    // A native activation binding can be moved to the mouse or keyboard.
    constexpr std::array alternateNative{33,mouseBase+2};
    constexpr std::array overlappingPickup{33,18,mouseBase};
    input.publish(Phase::idle,0,true,alternateNative);
    auto route = input.button(33,true,false,overlappingPickup,false);
    CHECK(!route.queued && !route.consume);
    CHECK(input.take().nativeActivateHeld);
    input.button(33,false,true,overlappingPickup,false);
    CHECK(!input.button(mouseBase+2,true,false,rebound,false).consume);
    CHECK(input.take().nativeActivateHeld);
    input.button(mouseBase+2,false,true,rebound,false);

    input.publish(Phase::held,17,true,native);
    route = input.button(activateKey,true,false,defaultBindings,false);
    CHECK(route.queued==Action::drop && route.consume);
    batch = input.take();
    CHECK(batch.count==1 && !batch.nativeActivateHeld);
    CHECK(currentInput(batch.requests[0],defaultBindings,Phase::held,17,false));
    CHECK(!currentInput(batch.requests[0],defaultBindings,Phase::held,18,false));
    input.publish(Phase::idle,0,true,native);
    CHECK(input.button(activateKey,false,false,defaultBindings,false).consume);
    CHECK(input.button(activateKey,false,true,defaultBindings,false).consume);
    CHECK(!input.button(activateKey,true,false,defaultBindings,false).consume);
    input.button(activateKey,false,true,defaultBindings,false);

    input.publish(Phase::held,19,true,native);
    route = input.button(mouseBase,true,false,defaultBindings,false);
    CHECK(route.queued==Action::throwItem && route.consume);
    input.clear();
    CHECK(input.take().count==0);
    CHECK(!input.button(activateKey,true,false,defaultBindings,false).consume);
}
