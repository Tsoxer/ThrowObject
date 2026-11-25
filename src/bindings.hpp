#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace to
{
enum class Action : std::size_t { pickUp, drop, throwItem };
enum class Device { keyboard, mouse, unsupported };
constexpr int unbound = 0, mouseBase = 256, bindingLimit = 264;
constexpr int activateKey = 18;
constexpr std::array<int, 3> defaultBindings{34, activateKey, mouseBase};

constexpr int bindingCode(Device device, unsigned id)
{
    if (device == Device::keyboard && id > 0 && id < 256) return static_cast<int>(id);
    if (device == Device::mouse && id < 8) return static_cast<int>(id) + mouseBase;
    return unbound;
}

constexpr bool validKeys(const std::array<int, 3>& keys)
{
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] < 0 || keys[i] >= bindingLimit) return false;
        for (std::size_t j = 0; keys[i] && j < i; ++j)
            if (keys[i] == keys[j]) return false;
    }
    return true;
}

constexpr std::optional<Action> matchingAction(const std::array<int, 3>& keys, int key)
{
    if (!key) return std::nullopt;
    for (std::size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == key) return static_cast<Action>(i);
    return std::nullopt;
}

// Keeps consuming the captured button through key-up, including when the
// menu closes or the new binding takes effect before the player releases it.
class KeyCapture {
public:
    struct Result { Action action; int key; };

    void listen(Action action) { waiting = action; }
    void cancel() { waiting.reset(); }
    bool isListening() const { return waiting.has_value(); }

    bool input(int key, bool down, bool up, bool onCancel = false)
    {
        if (!key) return false;
        if (key == swallowed) {
            if (!down) {
                if (up) swallowed = unbound;
                return true;
            }
            // A fresh press after a lost release must never stick.
            swallowed = unbound;
        }
        if (!waiting || !down || onCancel) return false;
        result = Result{*waiting,key};
        waiting.reset();
        swallowed = key;
        return true;
    }

    std::optional<Result> take()
    {
        const auto value = result;
        result.reset();
        return value;
    }

private:
    std::optional<Action> waiting;
    std::optional<Result> result;
    int swallowed{};
};
}
