#pragma once

#include "weapon_grip.hpp"

#include <vector>

namespace to
{
enum class CarryModel { ordinary, staff, crossbow };

struct CarryVisual {
    RE::NiPointer<RE::NiNode> root;
    const char* reason;
};

CarryVisual buildCarryVisual(RE::NiAVObject& source, CarryModel model);
std::vector<GripTriangle> carryGripTriangles(RE::NiNode& visual);
}
