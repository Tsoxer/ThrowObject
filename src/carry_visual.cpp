#include "carry_visual.hpp"
#include "carry_visual_game.hpp"

namespace to
{
namespace
{
VisualNodeState inspect(RE::NiAVObject& object)
{
    const bool visible = !object.GetAppCulled();
    const bool collision = bool(object.collisionObject);
    if (object.AsNode() && !object.AsSwitchNode())
        return {VisualNodeKind::branch,visible,collision,false,false};
    auto* shape = object.AsTriShape();
    if (!shape || shape->GetType()!=RE::BSGeometry::Type::kTriShape)
        return {VisualNodeKind::unsupported,visible,collision,false,false};

    const auto& data = shape->GetGeometryRuntimeData();
    const bool animated = data.skinInstance || object.GetControllers() ||
        std::any_of(std::begin(data.properties),std::end(data.properties),[](const auto& property) {
            return property && property->GetControllers();
        });
    return {VisualNodeKind::staticMesh,visible,collision,animated,data.rendererData!=nullptr};
}
}

CarryVisual buildCarryVisual(RE::NiAVObject& source)
{
    RE::NiPointer<RE::NiNode> root{RE::NiNode::Create(1)};
    if (!root) return {nullptr,"scene node unavailable"};
    root->name = "ThrowObjectVisual";
    const auto result = copyCarryGeometry(&source,RE::NiTransform{},true,inspect,
        [](RE::NiAVObject& branch) -> const auto& { return branch.AsNode()->GetChildren(); },
        [](const RE::NiAVObject& object) -> const auto& { return object.local; },
        [&](RE::NiAVObject& mesh, const RE::NiTransform& pose) {
            // Engine clone of a single collision-free BSTriShape. The source
            // branch and its Havok objects never enter the cloning call.
            SKSE::log::info("Cloning collision-free render leaf '{}'",mesh.name.c_str());
            RE::NiPointer<RE::NiAVObject> copy{mesh.Clone()};
            if (!copy || copy.get()==&mesh || copy->parent) return false;
            const auto state = inspect(*copy);
            if (state.kind!=VisualNodeKind::staticMesh || state.hasCollision ||
                state.hasAnimation || !state.hasRenderer) return false;
            copy->local = pose;
            copy->SetAppCulled(false);
            copy->SetUserData(nullptr);
            root->AttachChild(copy.get());
            return copy->parent==root.get();
        });

    switch (result) {
    case VisualResult::ready: return {std::move(root),nullptr};
    case VisualResult::hidden: return {nullptr,"no visible render geometry"};
    case VisualResult::unavailable: return {nullptr,"render geometry unavailable"};
    case VisualResult::unsupported: return {nullptr,"unsupported visible geometry, controller, or collision on a render leaf"};
    case VisualResult::cloneFailed: return {nullptr,"could not create detached render leaf"};
    }
    return {nullptr,"could not build carry visual"};
}
}
