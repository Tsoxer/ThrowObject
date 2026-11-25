#pragma once

namespace to
{
struct CarryVisual {
    RE::NiPointer<RE::NiNode> root;
    const char* reason;
};

CarryVisual buildCarryVisual(RE::NiAVObject& source);
}
