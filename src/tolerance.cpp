#include "tolerance.hpp"
#include "physics.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <utility>
#include <vector>

namespace to
{
namespace
{
constexpr std::uint32_t recordVersion = 1;
constexpr std::size_t maxSavedActors = 65536;
constexpr float nearbyDistance = 4096.f;
using RecordPair = std::array<std::uint32_t,2>;
static_assert(sizeof(RecordPair)==8);

DailyTolerance tolerance;
std::mutex toleranceLock;
using MeterClock = std::chrono::steady_clock;
MeterClock::time_point nextMeterSample;
struct RecentExhaustion {
    RE::FormID actor{};
    std::uint32_t day{};
    MeterClock::time_point visibleUntil;
};
RecentExhaustion recentExhaustion;
std::atomic_bool meterVisible{};
std::mutex meterLock;
ToleranceMeterDisplay meterDisplay;
MeterClock::time_point meterUpdated;

void hideMeter()
{
    if (meterVisible.load(std::memory_order_relaxed))
        meterVisible.store(false,std::memory_order_release);
}

std::optional<std::uint32_t> currentDay()
{
    const auto* clock = RE::Calendar::GetSingleton();
    if (!clock || !clock->gameYear || !clock->gameMonth || !clock->gameDay || !clock->gameHour) return std::nullopt;
    return toleranceDay(clock->GetYear(),clock->GetMonth(),clock->GetDay(),clock->GetHour());
}

const RE::BGSLocation* cityLocation(const RE::BGSLocation* location)
{
    // Location ancestry can be overridden by other mods. Keep the walk bounded.
    for (int depth=0; location && depth<16; ++depth,location=location->parentLoc)
        if (location->HasKeywordString("LocTypeCity")) return location;
    return nullptr;
}

bool nearbyActor(const RE::PlayerCharacter& player, const RE::Actor& npc)
{
    const auto* playerCell = player.GetParentCell();
    const auto* npcCell = npc.GetParentCell();
    if (!playerCell || !npcCell || !npc.Is3DLoaded() || npc.IsDisabled()) return false;
    if (playerCell->IsInteriorCell() || npcCell->IsInteriorCell()) return playerCell==npcCell;

    const auto* world = player.GetWorldspace();
    if (!world || world!=npc.GetWorldspace()) return false;
    const auto offset = npc.GetPosition()-player.GetPosition();
    if (offset.SqrLength()<=nearbyDistance*nearbyDistance) return true;
    if (!world->parentWorld && !world->flags.any(RE::TESWorldSpace::Flag::kSmallWorld)) return false;
    if (cityLocation(world->location)) return true;
    const auto* city = cityLocation(player.GetCurrentLocation());
    return city && city==cityLocation(npc.GetCurrentLocation());
}

std::optional<std::uint32_t> eligibleDay(RE::PlayerCharacter& player, RE::Actor& npc)
{
    if (!npc.GetFormID() || npc.IsHostileToActor(&player) || !nearbyActor(player,npc)) return std::nullopt;
    return currentDay();
}

int rollTolerance(const Settings& settings)
{
    static std::mt19937 random{static_cast<std::mt19937::result_type>(
        std::chrono::steady_clock::now().time_since_epoch().count())};
    const auto range = toleranceRange(settings.toleranceMin,settings.toleranceMax);
    return std::uniform_int_distribution<int>{range.minimum,range.maximum}(random);
}

template<class Update>
ToleranceCount withTolerance(Update update)
{
    const std::scoped_lock guard(toleranceLock);
    return update(tolerance);
}
}

ToleranceReaction recordToleratedHit(RE::PlayerCharacter& player, RE::Actor& npc, const Settings& settings,
                                     ToleranceHit hit)
{
    const auto day = eligibleDay(player,npc);
    const auto id = npc.GetFormID();
    if (!day) return ToleranceReaction::ordinary;

    const auto count = withTolerance([&](DailyTolerance& data) {
        return data.hit(*day,id,[&] { return rollTolerance(settings); },hit);
    });
    if (hit==ToleranceHit::exhaust)
        recentExhaustion = {id,*day,MeterClock::now()+std::chrono::seconds{2}};
    nextMeterSample = {};
    if (settings.log || count.escalates())
        SKSE::log::info("NPC tolerance {:08X}: {}/{} confirmed throws, {}",
                        id,count.hits,count.limit,count.escalates() ? "normal assault response" : "tolerated");
    return count.escalates() ? ToleranceReaction::limitReached : ToleranceReaction::tolerated;
}

void updateToleranceMeter(RE::PlayerCharacter& player, const Settings& settings, bool visible)
{
    if (!visible) {
        nextMeterSample = {};
        hideMeter();
        return;
    }
    const auto now = MeterClock::now();
    if (now<nextMeterSample) return;
    nextMeterSample = now+std::chrono::milliseconds{80};

    const auto actor = toleranceAimTarget(player,nearbyDistance);
    if (!actor || actor.get()==&player || actor->IsDead() || actor->IsGhost()) return hideMeter();
    auto day = eligibleDay(player,*actor);
    // Let the HUD show the completed bar briefly if the very same hit
    // turned its current target hostile before the next meter sample.
    if (!day && actor->GetFormID()==recentExhaustion.actor && now<recentExhaustion.visibleUntil &&
        nearbyActor(player,*actor) && currentDay()==recentExhaustion.day) day = recentExhaustion.day;
    if (!day) return hideMeter();
    const auto* name = actor->GetDisplayFullName();
    if (!name || !*name) name = actor->GetName();
    if (!name || !*name) return hideMeter();

    const auto count = withTolerance([&](DailyTolerance& data) {
        return data.observe(*day,actor->GetFormID(),[&] { return rollTolerance(settings); });
    });
    auto text = std::string(name)+" TM: "+std::to_string(count.hits)+"/"+std::to_string(count.limit);
    {
        const std::scoped_lock guard(meterLock);
        meterDisplay = {std::move(text),settings.toleranceMeterCorner};
        meterUpdated = now;
        meterVisible.store(true,std::memory_order_release);
    }
}

std::optional<ToleranceMeterDisplay> readToleranceMeter()
{
    if (!meterVisible.load(std::memory_order_acquire)) return std::nullopt;
    const std::scoped_lock guard(meterLock);
    if (!meterVisible.load(std::memory_order_relaxed) ||
        MeterClock::now()-meterUpdated>std::chrono::milliseconds{300}) return std::nullopt;
    return meterDisplay;
}

void saveTolerance(const SKSE::SerializationInterface& archive)
{
    const auto today = currentDay();
    std::vector<RecordPair> buffer;
    {
        const std::scoped_lock guard(toleranceLock);
        const auto day = tolerance.day();
        const auto& entries = tolerance.entries();
        if (!day || (today && today!=day) || entries.empty() || entries.size()>maxSavedActors) return;
        buffer.reserve(entries.size()+1);
        buffer.push_back({*day,static_cast<std::uint32_t>(entries.size())});
        for (const auto& [id,count] : entries) buffer.push_back({id,packTolerance(count)});
    }
    if (!archive.WriteRecord(toleranceRecord,recordVersion,buffer.data(),
                             static_cast<std::uint32_t>(buffer.size()*sizeof(RecordPair))))
        SKSE::log::warn("Could not save NPC tolerance counts");
}

void loadTolerance(const SKSE::SerializationInterface& archive, std::uint32_t version, std::uint32_t length)
{
    if (version!=recordVersion || length<sizeof(RecordPair) || length%sizeof(RecordPair)!=0) return;
    RecordPair header{};
    if (archive.ReadRecordData(header)!=sizeof(header) || header[1]>maxSavedActors ||
        header[1]!=length/sizeof(RecordPair)-1) return;

    DailyTolerance loaded;
    loaded.beginDay(header[0]);
    for (std::uint32_t n=0; n<header[1]; ++n) {
        RecordPair entry{};
        if (archive.ReadRecordData(entry)!=sizeof(entry)) return;
        const auto count = unpackTolerance(entry[1]);
        RE::FormID current{};
        if (count && entry[0] && archive.ResolveFormID(entry[0],current) && current)
            loaded.restore(current,*count);
    }
    const std::scoped_lock guard(toleranceLock);
    tolerance = std::move(loaded);
}

void resetTolerance()
{
    const std::scoped_lock guard(toleranceLock);
    tolerance = {};
    recentExhaustion = {};
    hideMeter();
}
}
