#pragma once

#include "core.hpp"
#include "tolerance.hpp"

#include <string>

namespace RE { class ButtonEvent; }

namespace to
{
struct Settings {
    std::array<int, 3> keys{defaultBindings};
    bool enabled{true}, log{false}, trajectoryDebug{false}, damageEnabled{true};
    bool reactions{true}, crime{true};
    bool toleranceMeter{false};
    float pickupDistance{135.0f}, metresPerSecond{13.0f}, force{1.0f}, dragScale{1.0f}, aimDistance{3500.0f};
    float xyz[3]{0.0f, 7.0f, -4.0f}, angles[3]{};
    float handSize{1.0f}, damageScale{1.0f}, maxDamage{25.0f}, reactDistance{95.0f}, reactCooldown{9.0f};
    int bounty{10};
    int toleranceMin{defaultToleranceMin}, toleranceMax{defaultToleranceMax};
    int toleranceMeterCorner{};
    unsigned int reactionTopic{};
};
Settings getSettings();
void loadSettings();
bool saveSettings(Settings settings);
bool saveBindings(const std::array<int, 3>& keys);
bool saveToleranceMeterSettings(bool enabled,int corner);
void registerMenu();
bool menuBlocksInput();
bool toleranceMeterAvailable();
std::string bindingName(int key);
int buttonCode(const RE::ButtonEvent& event);
}
