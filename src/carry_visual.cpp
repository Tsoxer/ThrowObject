#include "carry_visual.hpp"
#include "carry_visual_game.hpp"

#include <RE/N/NiCloningProcess.h>
#include <RE/N/NiSkinData.h>
#include <RE/N/NiTimeController.h>

#include <cstring>
#include <unordered_map>

namespace to
{
namespace
{
const RE::NiSkinPartition* skinPartition(const RE::NiSkinInstance& skin)
{
    return skin.skinPartition ? skin.skinPartition.get() :
        skin.skinData ? skin.skinData->skinPartition.get() : nullptr;
}

bool hasPartitionRenderer(const RE::NiSkinInstance& skin)
{
    const auto* partition = skinPartition(skin);
    if (!partition || !partition->partitions.data() || !partition->numPartitions ||
        partition->numPartitions>64) return false;
    const auto parts = std::span{partition->partitions.data(),partition->numPartitions};
    return std::ranges::any_of(parts,[](const auto& part) {
        return part.buffData && (part.buffData->vertexBuffer || part.buffData->rawVertexData);
    });
}

class CarrySkeleton {
public:
    CarrySkeleton(RE::NiAVObject& source, RE::NiNode& copy) : nodes{{&source,&copy}} {}

    RE::NiPointer<RE::NiAVObject> copyLeaf(RE::BSTriShape& mesh)
    {
        const auto source = mesh.GetGeometryRuntimeData().skinInstance;
        if (!source || !source->skinData || !source->bones) return {};
        const auto count = source->skinData->bones;
        if (!count || count>128) return {};
        auto* root = bone(source->rootParent);
        if (!root) return {};
        std::vector<RE::NiNode*> bones;
        bones.reserve(count);
        for (std::uint32_t i=0;i<count;++i) {
            auto* copy = bone(source->bones[i]);
            if (!copy) return {};
            bones.push_back(copy);
        }

        // Map the skeleton before asking Skyrim to clone this one render leaf.
        // No source node, controller target outside the leaf, or Havok body
        // should enter the clone as an object to duplicate.
        RE::NiCloningProcess cloning{};
        cloning.unk68 = {1.f,1.f,1.f};
        for (const auto& [original,copy] : nodes) cloning.cloneMap.emplace(original,copy);
        RE::NiPointer<RE::NiObject> object{mesh.CreateClone(cloning)};
        auto* clone = object ? object->AsTriShape() : nullptr;
        if (!clone || clone==&mesh || clone->parent || clone->collisionObject) return {};

        auto& skin = clone->GetGeometryRuntimeData().skinInstance;
        if (!skin || skin.get()==source.get()) {
            // Some leaves share the source instance on clone. Build a plain
            // private binding for those instead of changing its live bones.
            static const REL::Relocation<const RE::NiRTTI*> plainSkin{RE::NiRTTI_NiSkinInstance};
            if (source->GetRTTI()!=plainSkin.get()) return {};
            skin.reset(RE::NiSkinInstance::Create());
            if (!skin) return {};
            skin->skinData = source->skinData;
            skin->skinPartition = source->skinPartition;
        }
        if (!skin->skinData || skin->skinData->bones!=count ||
            (skin->bones && skin->bones==source->bones) ||
            (skin->boneWorldTransforms && skin->boneWorldTransforms==source->boneWorldTransforms) ||
            (skin->boneMatrices && skin->boneMatrices==source->boneMatrices) ||
            (skin->prevBoneMatrices && skin->prevBoneMatrices==source->prevBoneMatrices) ||
            (skin->skinToWorldWorldToSkinMatrix &&
                skin->skinToWorldWorldToSkinMatrix==source->skinToWorldWorldToSkinMatrix)) return {};

        if (!skin->bones) skin->bones = RE::calloc<RE::NiAVObject*>(count);
        if (!skin->boneWorldTransforms) skin->boneWorldTransforms = RE::calloc<const RE::NiTransform*>(count);
        if (!skin->bones || !skin->boneWorldTransforms) return {};
        skin->rootParent = root;
        for (std::uint32_t i=0;i<count;++i) {
            skin->bones[i] = bones[i];
            skin->boneWorldTransforms[i] = &bones[i]->world;
        }

        // Publish the private binding to the native clone linker before it
        // processes skin links. A shared source skin must never be relinked.
        const auto privateSkin = skin;
        if (const auto entry = cloning.cloneMap.find(source.get()); entry!=cloning.cloneMap.end())
            entry->second = privateSkin.get();
        else
            cloning.cloneMap.emplace(source.get(),privateSkin.get());
        mesh.ProcessClone(cloning);
        if (skin.get()!=privateSkin.get() || skin->rootParent!=root || !skin->bones ||
            !skin->boneWorldTransforms) return {};
        for (std::uint32_t i=0;i<count;++i)
            if (skin->bones[i]!=bones[i] || skin->boneWorldTransforms[i]!=&bones[i]->world) return {};
        return RE::NiPointer<RE::NiAVObject>{clone};
    }

private:
    RE::NiNode* bone(RE::NiAVObject* source, unsigned depth = 0)
    {
        if (!source || depth>64) return nullptr;
        if (const auto found = nodes.find(source); found!=nodes.end()) return found->second;
        auto* parent = source->AsNode() ? bone(source->parent,depth+1) : nullptr;
        if (!parent) return nullptr;  // The bone must belong to this world model.
        RE::NiPointer<RE::NiNode> copy{RE::NiNode::Create()};
        if (!copy) return nullptr;
        copy->name = "ThrowObjectCrossbowBone";
        copy->local = source->local;
        parent->AttachChild(copy.get());
        if (copy->parent!=parent) return nullptr;
        return nodes.emplace(source,copy.get()).first->second;
    }

    std::unordered_map<RE::NiAVObject*,RE::NiNode*> nodes;
};

bool localTransformControllers(const RE::NiAVObject& object, const RE::NiTimeController* controller)
{
    static const REL::Relocation<const RE::NiRTTI*> transformRTTI{RE::NiRTTI_NiTransformController};
    unsigned count{};
    for (; controller; controller=controller->GetNext()) {
        // Only crossbow transforms targeting this same mesh can be cloned.
        // In particular, a controller cannot refer to a live parent or sibling.
        if (++count>8 || controller->GetRTTI()!=transformRTTI.get() || controller->target!=&object)
            return false;
    }
    return count>0;
}

VisualNodeState inspect(RE::NiAVObject& object, CarryModel model = CarryModel::ordinary)
{
    const auto* geometry = object.AsGeometry();
    using Type = RE::BSGeometry::Type;
    // Geometry types are IDs. The bit patterns of particle IDs also match
    // normal static triangle shapes if they are treated as flags.
    const auto type = geometry ? geometry->GetType().get() : Type::kGeometry;
    const bool particles = model!=CarryModel::ordinary && (object.AsParticlesGeom() ||
        type==Type::kParticles || type==Type::kStripParticles || type==Type::kParticleShaderDynamicTriShape);
    const bool visible = !object.GetAppCulled() && !particles;
    const bool collision = bool(object.collisionObject);
    if (object.AsNode() && !object.AsSwitchNode())
        return {VisualNodeKind::branch,visible,collision,false,false};
    auto* shape = object.AsTriShape();
    if (!shape || shape->GetType()!=RE::BSGeometry::Type::kTriShape)
        return {VisualNodeKind::unsupported,visible,collision,false,false};

    const auto& data = shape->GetGeometryRuntimeData();
    const auto* controller = object.GetControllers();
    const bool animatedMaterial = std::any_of(std::begin(data.properties),std::end(data.properties),[](const auto& property) {
            return property && property->GetControllers();
        });
    const bool skinned = model==CarryModel::crossbow && data.skinInstance &&
        (data.rendererData || hasPartitionRenderer(*data.skinInstance));
    const bool animated = (data.skinInstance && !skinned) || controller || animatedMaterial;
    const bool canFreeze = model==CarryModel::crossbow && !collision && (!data.skinInstance || skinned) &&
        !animatedMaterial && localTransformControllers(object,controller);
    return {skinned ? VisualNodeKind::skinnedMesh : VisualNodeKind::staticMesh,visible,collision,
            animated,skinned || data.rendererData!=nullptr,canFreeze};
}
}

CarryVisual buildCarryVisual(RE::NiAVObject& source, CarryModel model)
{
    RE::NiPointer<RE::NiNode> root{RE::NiNode::Create(1)};
    if (!root) return {nullptr,"scene node unavailable"};
    root->name = "ThrowObjectVisual";
    std::optional<CarrySkeleton> skeleton;
    if (model==CarryModel::crossbow) skeleton.emplace(source,*root);
    const auto result = copyCarryGeometry(&source,RE::NiTransform{},true,
        [model](RE::NiAVObject& object) {
            const auto state = inspect(object,model);
            if (model==CarryModel::crossbow && state.visible &&
                (state.kind==VisualNodeKind::unsupported || (state.kind!=VisualNodeKind::branch &&
                    (state.hasCollision || !state.hasRenderer || (state.hasAnimation && !state.canFreezeTransform))))) {
                const auto* type = object.GetRTTI();
                const auto* controller = object.GetControllers();
                const auto* controllerType = controller ? controller->GetRTTI() : nullptr;
                SKSE::log::warn("Crossbow render rejected '{}': type {}, collision {}, controller {}, animation {}, renderer {}",
                                object.name.c_str(),type ? type->GetName() : "unknown",state.hasCollision,
                                controllerType ? controllerType->GetName() : "none",state.hasAnimation,state.hasRenderer);
            }
            return state;
        },
        [](RE::NiAVObject& branch) -> const auto& { return branch.AsNode()->GetChildren(); },
        [](const RE::NiAVObject& object) -> const auto& { return object.local; },
        [&](RE::NiAVObject& mesh, const RE::NiTransform& pose) {
            // Engine clone of a single collision-free BSTriShape. The source
            // branch and its Havok objects never enter the cloning call.
            auto* shape = skeleton ? mesh.AsTriShape() : nullptr;
            const bool skinned = shape && shape->GetGeometryRuntimeData().skinInstance;
            SKSE::log::info("Cloning collision-free {}render leaf '{}'",skinned ? "skinned " : "",mesh.name.c_str());
            auto copy = skinned ? skeleton->copyLeaf(*shape) : RE::NiPointer<RE::NiAVObject>{mesh.Clone()};
            if (!copy || copy.get()==&mesh || copy->parent) return false;
            if (model==CarryModel::crossbow && mesh.GetControllers()) {
                // Preserve the current ground-model transform in the clone.
                // Neither the original mesh nor its animation is edited.
                copy->controllers.reset();
                SKSE::log::info("Froze crossbow carry transform '{}'",mesh.name.c_str());
            }
            const auto state = inspect(*copy,skinned ? CarryModel::crossbow : CarryModel::ordinary);
            if (state.kind!=(skinned ? VisualNodeKind::skinnedMesh : VisualNodeKind::staticMesh) || state.hasCollision ||
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

std::vector<GripTriangle> carryGripTriangles(RE::NiNode& visual)
{
    constexpr std::size_t maxTriangles = 32000;
    std::vector<GripTriangle> triangles;
    using Vertex = RE::BSGraphics::Vertex;
    for (const auto& leaf : visual.GetChildren()) {
        auto* shape = leaf ? leaf->AsTriShape() : nullptr;
        if (!shape) continue;
        const auto* renderer = shape->GetGeometryRuntimeData().rendererData;
        const auto& mesh = shape->GetTrishapeRuntimeData();
        if (!renderer || !renderer->rawVertexData || !renderer->rawIndexData || !mesh.vertexCount ||
            !mesh.triangleCount || triangles.size()+mesh.triangleCount>maxTriangles) continue;

        const auto& layout = renderer->vertexDesc;
        if (!layout.HasFlag(Vertex::VF_VERTEX)) continue;
        std::uint64_t packed{};
        static_assert(sizeof(packed)==sizeof(layout));
        std::memcpy(&packed,&layout,sizeof(packed));
        const auto stride = static_cast<std::uint32_t>(packed&0xFu)*4u;
        // CPU geometry is optional. Read only a verified four-float position
        // layout, which is also bounded by the mesh's own model sphere.
        auto positionBytes = stride;
        for (unsigned a=1;a<Vertex::VA_COUNT;++a) {
            if (!layout.HasFlag(static_cast<Vertex::Flags>(1u<<a))) continue;
            positionBytes = std::min(positionBytes,layout.GetAttributeOffset(static_cast<Vertex::Attribute>(a)));
        }
        if (positionBytes!=16 || stride<positionBytes) continue;
        const auto& bound = shape->GetModelData().modelBound;
        if (!std::isfinite(bound.radius) || !(bound.radius>0.f)) continue;
        const auto tolerance = bound.radius*1.08f+1.f;
        std::vector<Vec> vertices;
        vertices.reserve(mesh.vertexCount);
        for (std::size_t i=0;i<mesh.vertexCount;++i) {
            float xyz[3];
            std::memcpy(xyz,renderer->rawVertexData+i*stride,sizeof(xyz));
            const RE::NiPoint3 p{xyz[0],xyz[1],xyz[2]};
            const auto fromCenter = p-bound.center;
            if (!(fromCenter.SqrLength()<=tolerance*tolerance)) break;
            const auto local = shape->local*p;
            vertices.push_back({local.x,local.y,local.z});
        }
        if (vertices.size()!=mesh.vertexCount) continue;
        for (std::size_t i=0;i<mesh.triangleCount;++i) {
            const auto* index = renderer->rawIndexData+3*i;
            if (index[0]>=vertices.size() || index[1]>=vertices.size() || index[2]>=vertices.size()) continue;
            triangles.push_back({vertices[index[0]],vertices[index[1]],vertices[index[2]]});
        }
    }
    return triangles;
}
}
