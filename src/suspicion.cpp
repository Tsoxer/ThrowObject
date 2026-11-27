#include "suspicion.hpp"
#include "voice_clip.hpp"

#include <RE/I/ID.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace to
{
namespace
{
struct Response { std::string_view master; std::string filename; };
std::array<std::vector<Response>,2> responses;
std::unordered_map<RE::FormID,std::vector<std::string>> recordings;
std::size_t nextRecording{};

struct Playback {
    RE::BSSoundHandle sound;
    RE::ActorHandle actor;
    RE::NiPointer<RE::NiAVObject> root, source;
    float age{}, timeout{};
};
std::vector<Playback> active;

std::vector<std::uint8_t> readRecording(const std::string& path)
{
    RE::BSResourceNiBinaryStream archive(path);
    std::array<std::uint8_t,12> header{};
    if (!archive.good() || !archive.read(header.data(),12) || !voiceTag(header,"FUZE")) return {};
    constexpr std::uint32_t maxSize = 1024*1024;
    const auto lipSize = voiceLE(std::span(header).subspan(8,4));
    if (lipSize>maxSize-20) return {};
    const auto payload = 12+lipSize;
    std::vector<std::uint8_t> result(payload+8);
    std::copy(header.begin(),header.end(),result.begin());
    if (!archive.read(result.data()+12,lipSize+8) || !voiceTag(std::span(result).subspan(payload),"RIFF"))
        return {};
    const auto riffSize = voiceLE(std::span(result).subspan(payload+4,4));
    if (riffSize<4 || riffSize>maxSize-result.size()) return {};
    result.resize(result.size()+riffSize);
    if (!archive.read(result.data()+payload+8,riffSize)) return {};
    return result;
}

bool playRecording(RE::Actor& target, const RE::BGSSoundOutput& output, const std::string& path,
                   float volume, float duration)
{
    RE::NiPointer<RE::NiAVObject> root{target.Get3D()};
    auto* audio = RE::BSAudioManager::GetSingleton();
    if (!root || !audio) return false;
    RE::NiPointer<RE::NiAVObject> source{root->GetObjectByName("NPC Head [Head]")};
    if (!source) source = root;

    RE::BSResource::ID file;
    file.GenerateFromPath(path.c_str());
    RE::BSSoundHandle sound;
    constexpr std::uint32_t positionalFlags = 0x19;
    constexpr std::uint32_t priority = 128;
    audio->BuildSoundDataFromFile(sound,file,positionalFlags,priority);
    if (!sound.IsValid()) return false;
    if (!sound.SetPosition(source->world.translate)) {
        sound.Stop();
        return false;
    }
    sound.SetObjectToFollow(source.get());
    // 1.6.1170 expects the secondary BSISoundOutputModel base of the DOBJ
    // form. The pinned CommonLib wrapper passes the unadjusted TESForm.
    using SetOutputModel = void(RE::BSSoundHandle*,const RE::BSISoundOutputModel*);
    static const REL::Relocation<SetOutputModel> model{REL::ID(67624)};
    model(&sound,static_cast<const RE::BSISoundOutputModel*>(&output));
    sound.SetVolume(volume);
    if (!sound.Play()) {
        sound.Stop();
        return false;
    }
    active.push_back({sound,target.GetHandle(),std::move(root),std::move(source),0.f,duration+2.f});
    SKSE::log::info("Queued positional suspicion voice for {:08X}: {}",target.GetFormID(),path);
    return true;
}
}

void loadSuspicionVoices()
{
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return;
    for (auto& group : responses) group.clear();
    recordings.clear();
    using Subtype = RE::DIALOGUE_DATA::Subtype;
    std::array<std::unordered_set<RE::FormID>,2> seen;
    for (const auto* topic : data->GetFormArray<RE::TESTopic>()) {
        if (!topic || topic->IsDeleted() || !topic->topicInfos) continue;
        const auto kind = topic->data.subtype.get();
        const bool hearing = kind==Subtype::kNormalToAlert;
        if (!hearing && kind!=Subtype::kAlertToNormal && kind!=Subtype::kLostToNormal) continue;
        const std::size_t group = hearing ? 0 : 1;
        for (std::uint32_t i=0; i<topic->numTopicInfos; ++i) {
            const auto* info = topic->topicInfos[i];
            if (!info || info->IsDeleted()) continue;
            const auto* shared = info->dataInfo;
            if (shared && shared->parentTopic &&
                shared->parentTopic->data.subtype.get()==Subtype::kSharedInfo) info = shared;
            const auto* parent = info->parentTopic;
            const auto* quest = parent && !parent->IsDeleted() ? parent->ownerQuest : nullptr;
            const auto* master = quest ? quest->GetFile(0) : nullptr;
            if (info->IsDeleted() || !info->GetFile(0) || !master) continue;
            const auto name = master->GetFilename();
            if (name!="Skyrim.esm" && name!="Dawnguard.esm" && name!="Dragonborn.esm" &&
                name!="HearthFires.esm") continue;
            if (!seen[group].insert(info->GetFormID()).second) continue;
            std::string_view questID = quest->GetFormEditorID();
            std::string_view topicID = parent->GetFormEditorID();
            if (questID.size()+topicID.size()>25) {
                questID = questID.substr(0,10);
                topicID = topicID.substr(0,25-questID.size());
            }
            responses[group].push_back({name,fmt::format("{}_{}_{:08X}_1.fuz",questID,topicID,info->GetLocalFormID())});
        }
    }
    SKSE::log::info("Indexed {} hearing and {} settling voice records",responses[0].size(),responses[1].size());
}

bool playSuspicion(RE::Actor& target, bool allowConcurrent)
{
    if ((!allowConcurrent && !active.empty()) || active.size()>=32) return false;
    const auto actorHandle = target.GetHandle();
    if (std::any_of(active.begin(),active.end(),[&](const auto& line) { return line.actor==actorHandle; })) return false;
    const auto* base = target.GetActorBase();
    const auto* voice = base ? base->voiceType : nullptr;
    if (!voice || !voice->Is(RE::FormType::VoiceType)) return false;
    const auto* voiceName = voice->formEditorID.c_str();
    if (!voiceName || !*voiceName) return false;
    const auto* defaults = RE::BGSDefaultObjectManager::GetSingleton();
    if (!defaults) return false;
    const auto* output = defaults->GetObject<RE::BGSSoundOutput>(RE::DEFAULT_OBJECT::kDialogueOutputModel3D);
    if (!output || !output->data.flags.any(RE::BGSSoundOutput::Data::Flag::kAttenuatesWithDistance))
        return false;
    const auto* category = defaults->GetObject<RE::BGSSoundCategory>(RE::DEFAULT_OBJECT::kDialogueVoiceCategory);
    const auto volume = category ? std::clamp(category->GetCategoryVolume(),0.f,1.f) : 1.f;
    if (!(volume>0.f)) return false;
    auto [entry,needsLookup] = recordings.try_emplace(voice->GetFormID());
    auto& paths = entry->second;
    if (needsLookup) {
        for (const auto& group : responses) {
            for (const auto& response : group) {
                auto path = fmt::format("Sound\\Voice\\{}\\{}\\{}",response.master,voiceName,response.filename);
                if (RE::BSResourceNiBinaryStream stream(path); stream.good()) paths.push_back(std::move(path));
                if (paths.size()>=6) break;
            }
            if (!paths.empty()) break;
        }
        SKSE::log::info("Located {} suspicion recordings for voice {}",paths.size(),voiceName);
    }
    for (std::size_t i=0; i<paths.size(); ++i) {
        const auto& path = paths[nextRecording++%paths.size()];
        const auto bytes = readRecording(path);
        const auto clip = readVoiceClip(bytes);
        if (clip && playRecording(target,*output,path,volume,clip->durationMs*.001f)) return true;
    }
    if (!paths.empty())
        SKSE::log::warn("No playable suspicion recording for voice {}",voiceName);
    return false;
}

void updateSuspicion(float dt)
{
    std::erase_if(active,[&](auto& line) {
        line.age += dt;
        auto actor = line.actor.get();
        if (!actor || actor->IsDisabled() || actor->Get3D()!=line.root.get() || line.age>line.timeout) {
            if (line.sound.IsValid()) line.sound.Stop();
            return true;
        }
        // Allow the audio worker to accept the queued start before polling it.
        return line.age>.75f && !line.sound.IsPlaying();
    });
}

void stopSuspicion()
{
    for (auto& line : active)
        if (line.sound.IsValid()) line.sound.Stop();
    active.clear();
}

bool suspicionActive() { return !active.empty(); }
}
