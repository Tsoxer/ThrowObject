#pragma once

#include "core.hpp"

#include <bitset>
#include <chrono>
#include <mutex>
#include <utility>

namespace to
{
struct InputRequest {
    using Clock = std::chrono::steady_clock;
    Action action{};
    int key{};
    std::uint64_t owner{};
    Clock::time_point pressed{};
};

struct InputBatch {
    std::array<InputRequest,3> requests{};
    std::size_t count{};
    bool nativeActivateHeld{};
};

// The device worker sees bindings and a value-only gameplay snapshot. The
// player update owns the references, animation graph, and physics operations.
class GameplayInput {
public:
    void publish(Phase phase, std::uint64_t owner, bool enabled, std::array<int,2> nativeActivate)
    {
        const std::scoped_lock guard(lock);
        state = {phase,owner,enabled,nativeActivate};
    }

    struct Route { bool consume{}; std::optional<Action> queued; };

    Route button(int key, bool down, bool up, const std::array<int,3>& bindings, bool blocked)
    {
        if (key<=0 || key>=bindingLimit) return {};
        const std::scoped_lock guard(lock);
        if (down) captured.reset(key);
        if (captured.test(key)) {
            if (up) captured.reset(key);
            return {true,std::nullopt};
        }
        if (up) forwarded.reset(key);
        if (!down) return {};

        const auto action = matchingAction(bindings,key);
        const bool activate = isNativeActivate(key);
        const bool allowed = action && state.enabled && !blocked && !nativeHeld() &&
            acceptsInput(*action,state.phase,state.owner!=0) &&
            (*action!=Action::pickUp || !activate);
        if (allowed) {
            const auto first = pending.requests.begin();
            const auto end = first+pending.count;
            const auto duplicate = std::find_if(first,end,[&](const InputRequest& old) {
                return old.action==*action && old.owner==state.owner && old.key==key;
            });
            if (duplicate!=end || pending.count<pending.requests.size()) {
                // Pickup eligibility depends on the live crosshair. Let its press
                // pass through rather than claiming an input we may later reject.
                const bool consume = *action!=Action::pickUp;
                if (consume) captured.set(key);
                if (duplicate!=end) {
                    duplicate->pressed = InputRequest::Clock::now();
                    return {consume,std::nullopt};
                }
                pending.requests[pending.count++] = {*action,key,state.owner,InputRequest::Clock::now()};
                return {consume,action};
            }
        }
        if (activate && !blocked) forwarded.set(key);
        return {};
    }

    InputBatch take()
    {
        const std::scoped_lock guard(lock);
        pending.nativeActivateHeld = nativeHeld();
        return std::exchange(pending,{});
    }

    void clear()
    {
        const std::scoped_lock guard(lock);
        state = {};
        pending = {};
        captured.reset();
        forwarded.reset();
    }

private:
    struct State {
        Phase phase{Phase::idle};
        std::uint64_t owner{};
        bool enabled{};
        std::array<int,2> nativeActivate{activateKey,unbound};
    } state;

    bool isNativeActivate(int key) const
    {
        return key==state.nativeActivate[0] || key==state.nativeActivate[1];
    }

    bool nativeHeld() const
    {
        return std::any_of(state.nativeActivate.begin(),state.nativeActivate.end(),[&](int key) {
            return key>0 && key<bindingLimit && forwarded.test(key);
        });
    }

    std::mutex lock;
    std::bitset<bindingLimit> captured;
    std::bitset<bindingLimit> forwarded;
    InputBatch pending;
};

constexpr bool currentInput(const InputRequest& request, const std::array<int,3>& bindings,
                            Phase phase, std::uint64_t currentOwner, bool nativeActivateHeld,
                            InputRequest::Clock::time_point now = InputRequest::Clock::now())
{
    // An old press made while there was no gameplay update should not take
    // effect after the player returns from a menu or a loading screen.
    return now-request.pressed<=std::chrono::milliseconds{400} && !nativeActivateHeld &&
        matchingAction(bindings,request.key)==request.action &&
        request.owner==currentOwner && acceptsInput(request.action,phase,currentOwner!=0);
}
}
