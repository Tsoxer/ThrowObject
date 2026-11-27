#pragma once

namespace to
{
enum class VisualNodeKind { branch, staticMesh, skinnedMesh, unsupported };
enum class VisualResult { ready, hidden, unavailable, unsupported, cloneFailed };

struct VisualNodeState {
    VisualNodeKind kind;
    bool visible;
    bool hasCollision;
    bool hasAnimation;
    bool hasRenderer;
    bool canFreezeTransform{};
};

// Walk source branches without cloning them. Only a verified render leaf
// reaches the clone callback. The caller freezes any approved local animation.
template<class Node, class Transform, class Inspect, class Children, class Local, class Clone>
VisualResult copyCarryGeometry(Node* source, const Transform& transform, bool root,
                               const Inspect& inspect, const Children& children,
                               const Local& local, const Clone& clone)
{
    if (!source) return VisualResult::unavailable;
    const auto state = inspect(*source);
    if (!root && !state.visible) return VisualResult::hidden;
    const auto pose = root ? transform : transform * local(*source);
    if (state.kind==VisualNodeKind::branch) {
        auto result = VisualResult::hidden;
        for (const auto& child : children(*source)) {
            if (!child) continue;  // NiNode child arrays can contain vacant slots.
            const auto next = copyCarryGeometry(child.get(),pose,false,inspect,children,local,clone);
            if (next==VisualResult::ready) result = next;
            else if (next!=VisualResult::hidden) return next;
        }
        return result;
    }
    if (state.kind==VisualNodeKind::unsupported || state.hasCollision ||
        (state.hasAnimation && !state.canFreezeTransform))
        return VisualResult::unsupported;
    if (!state.hasRenderer) return VisualResult::unavailable;
    return clone(*source,pose) ? VisualResult::ready : VisualResult::cloneFailed;
}
}
