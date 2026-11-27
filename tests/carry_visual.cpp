#include "carry_visual.hpp"
#include "core.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace to;

void check(bool result, const char* expression)
{
    if (result) return;
    std::cerr << expression << '\n';
    std::exit(1);
}
#define CHECK(expression) check((expression),#expression)

struct ModelNode {
    std::string name;
    int offset;
    VisualNodeState state;
    std::vector<std::unique_ptr<ModelNode>> children;
};

struct Transform {
    int offset{};
    Transform operator*(Transform other) const { return {offset*10+other.offset}; }
};

constexpr VisualNodeState mesh{VisualNodeKind::staticMesh,true,false,false,true};
constexpr VisualNodeState collisionBranch{VisualNodeKind::branch,true,true,false,false};

int main()
{
    using Instance = std::pair<std::string,int>;
    std::vector<Instance> attempts;
    std::string failClone;
    const auto stage = [&](const ModelNode* root, bool crossbow = false) {
        attempts.clear();
        return copyCarryGeometry(root,Transform{},true,
            [&](const ModelNode& node) {
                auto state = node.state;
                state.canFreezeTransform &= crossbow;
                if (state.kind==VisualNodeKind::skinnedMesh && !crossbow)
                    state.kind = VisualNodeKind::unsupported;
                return state;
            },
            [](const ModelNode& node) -> const auto& { return node.children; },
            [](const ModelNode& node) -> Transform { return {node.offset}; },
            [&](const ModelNode& node, Transform pose) {
                CHECK(!node.state.hasCollision);
                attempts.emplace_back(node.name,pose.offset);
                return node.name!=failClone;
            });
    };

    // Recreate the reported topology with a root convex body and a visible leaf.
    // The fixture contains no Skyrim assets or engine code.
    ModelNode plate{"DwePlate01",9,collisionBranch,{}};
    plate.children.push_back(std::make_unique<ModelNode>(ModelNode{"Plate render",2,mesh,{}}));
    CHECK(stage(&plate)==VisualResult::ready);
    CHECK((attempts==std::vector<Instance>{{"Plate render",2}}));

    const ModelNode small{"Small static shape",8,mesh,{}};
    CHECK(stage(&small)==VisualResult::ready);
    CHECK((attempts==std::vector<Instance>{{"Small static shape",0}}));

    ModelNode complex{"Complex collision root",9,collisionBranch,{}};
    complex.children.push_back(nullptr);
    auto nested = std::make_unique<ModelNode>(ModelNode{"Nested collision branch",3,collisionBranch,{}});
    nested->children.push_back(std::make_unique<ModelNode>(ModelNode{"Outer render",5,mesh,{}}));
    nested->children.push_back(std::make_unique<ModelNode>(ModelNode{"Inner render",7,mesh,{}}));
    auto& inner = *nested->children.back();
    complex.children.push_back(std::move(nested));
    CHECK(stage(&complex)==VisualResult::ready);
    CHECK((attempts==std::vector<Instance>{{"Outer render",35},{"Inner render",37}}));

    inner.state.visible = false;
    inner.state.hasCollision = true;
    CHECK(stage(&complex)==VisualResult::ready && attempts.size()==1);
    inner.state.visible = true;
    CHECK(stage(&complex)==VisualResult::unsupported && attempts.size()==1);
    CHECK(inner.state.hasCollision);
    inner.state = mesh;
    inner.state.hasAnimation = true;
    CHECK(stage(&complex)==VisualResult::unsupported && attempts.size()==1);
    CHECK(stage(&complex,true)==VisualResult::unsupported && attempts.size()==1);
    // A crossbow's stock can have a local transform controller. It can be
    // snapshotted only when the renderer is loaded and the leaf has no Havok.
    inner.state.canFreezeTransform = true;
    CHECK(stage(&complex)==VisualResult::unsupported && attempts.size()==1);
    CHECK(stage(&complex,true)==VisualResult::ready && attempts.size()==2);
    CHECK(inner.state.hasAnimation);
    inner.state.hasCollision = true;
    CHECK(stage(&complex,true)==VisualResult::unsupported && attempts.size()==1);
    inner.state.hasCollision = false;
    inner.state.hasAnimation = false;
    inner.state.hasRenderer = false;
    CHECK(stage(&complex)==VisualResult::unavailable && attempts.size()==1);
    inner.state = mesh;
    failClone = inner.name;
    CHECK(stage(&complex)==VisualResult::cloneFailed && attempts.size()==2);
    failClone.clear();
    CHECK(stage(static_cast<ModelNode*>(nullptr))==VisualResult::unavailable && attempts.empty());
    const ModelNode empty{"No loaded children",0,collisionBranch,{}};
    CHECK(stage(&empty)==VisualResult::hidden && attempts.empty());

    // RC2 rejected each of these shapes because the mesh had no standalone
    // renderer. The crossbow adapter supplies the skin partition's renderer.
    ModelNode crossbow{"Crossbow world root",9,collisionBranch,{}};
    crossbow.children.push_back(std::make_unique<ModelNode>(ModelNode{
        "Crossbow:0",4,{VisualNodeKind::skinnedMesh,true,false,false,true},{}}));
    auto& skin = *crossbow.children.back();
    CHECK(stage(&crossbow)==VisualResult::unsupported && attempts.empty());
    CHECK(stage(&crossbow,true)==VisualResult::ready);
    CHECK((attempts==std::vector<Instance>{{"Crossbow:0",4}}));
    skin.state.hasRenderer = false;
    CHECK(stage(&crossbow,true)==VisualResult::unavailable && attempts.empty());
    skin.state.hasRenderer = true;
    skin.state.hasCollision = true;
    CHECK(stage(&crossbow,true)==VisualResult::unsupported && attempts.empty());

    // Ordinary movement and vanilla E do not request a custom pickup or drop.
    CHECK(!matchingAction(defaultBindings,32));  // D
    CHECK(!acceptsInput(Action::drop,Phase::idle,false));
    auto phase = Phase::idle;
    for (int cycle = 0; cycle<24; ++cycle) {
        phase = transition(phase,Event::requestPickup);
        CHECK(phase==Phase::pickup);
        CHECK(stage(&plate)==VisualResult::ready);
        phase = transition(transition(phase,Event::grab),Event::pickupEnd);
        CHECK(phase==Phase::held && !matchingAction(defaultBindings,32));
        CHECK(acceptsInput(Action::drop,phase,true));
        phase = transition(phase,Event::drop);
        CHECK(phase==Phase::idle && plate.state.hasCollision);
    }
}
