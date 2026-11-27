#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>

namespace RE { class Actor; class PlayerCharacter; }
namespace SKSE { class SerializationInterface; }

namespace to
{
constexpr int defaultToleranceMin = 3, defaultToleranceMax = 7;
constexpr int toleranceMinLimit = 10, toleranceMaxLimit = 20;
constexpr int toleranceMeterCornerCount = 4;

struct ToleranceRange { int minimum, maximum; };

constexpr ToleranceRange toleranceRange(int minimum, int maximum)
{
    minimum = std::clamp(minimum,1,toleranceMinLimit);
    return {minimum,std::clamp(maximum,minimum,toleranceMaxLimit)};
}

inline std::optional<std::uint32_t> toleranceDay(std::uint32_t year, std::uint32_t month,
                                                 float day, float hour)
{
    constexpr std::array monthStart{0u,31u,59u,90u,120u,151u,181u,212u,243u,273u,304u,334u,365u};
    if (month>=monthStart.size()-1 || year>=std::numeric_limits<std::uint32_t>::max()/365u ||
        !(day>=1.f && day<=float(monthStart[month+1]-monthStart[month])) ||
        !(hour>=0.f && hour<24.f)) return std::nullopt;
    // Midnight belongs to the day already in progress. Only 6 AM advances it.
    return year*365u+monthStart[month]+static_cast<std::uint32_t>(day)-(hour<6.f ? 1u : 0u);
}

struct ToleranceCount {
    std::uint8_t limit{}, hits{};
    constexpr bool escalates() const { return hits>=limit; }
};

enum class ToleranceHit { ordinary, exhaust };

constexpr std::uint32_t packTolerance(ToleranceCount count)
{
    return std::uint32_t(count.limit) | (std::uint32_t(count.hits)<<8);
}

constexpr std::optional<ToleranceCount> unpackTolerance(std::uint32_t packed)
{
    const ToleranceCount count{static_cast<std::uint8_t>(packed),static_cast<std::uint8_t>(packed>>8)};
    if ((packed>>16)!=0 || count.limit<1 || count.limit>toleranceMaxLimit ||
        count.hits>count.limit) return std::nullopt;
    return count;
}

class DailyTolerance {
public:
    void beginDay(std::uint32_t day)
    {
        if (day==activeDay) return;
        counts.clear();
        activeDay = day;
    }

    template<class Roll>
    ToleranceCount& observe(std::uint32_t day, std::uint32_t actor, Roll roll)
    {
        beginDay(day);
        auto [entry,inserted] = counts.try_emplace(actor);
        auto& count = entry->second;
        if (inserted) count.limit = static_cast<std::uint8_t>(roll());
        return count;
    }

    template<class Roll>
    ToleranceCount hit(std::uint32_t day, std::uint32_t actor, Roll roll, ToleranceHit hit=ToleranceHit::ordinary)
    {
        auto& count = observe(day,actor,roll);
        if (hit==ToleranceHit::exhaust) count.hits = count.limit;
        else if (count.hits<count.limit) ++count.hits;
        return count;
    }

    void restore(std::uint32_t actor, ToleranceCount count) { counts.try_emplace(actor,count); }
    std::optional<std::uint32_t> day() const { return activeDay; }
    const auto& entries() const { return counts; }

private:
    std::optional<std::uint32_t> activeDay;
    std::unordered_map<std::uint32_t,ToleranceCount> counts;
};

enum class ToleranceReaction { ordinary, tolerated, limitReached };
struct Settings;
struct ToleranceMeterDisplay { std::string text; int corner{}; };
struct ToleranceMeterPlacement { float x, y, pivotX, pivotY; };

inline std::optional<ToleranceMeterPlacement> placeToleranceMeter(int corner,float x,float y,float width,float height)
{
    constexpr float margin = 20.f;
    if (corner<0 || corner>=toleranceMeterCornerCount || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(width) || !std::isfinite(height) || width<=2.f*margin || height<=2.f*margin)
        return std::nullopt;
    const bool right = (corner&1)!=0, bottom = (corner&2)!=0;
    return ToleranceMeterPlacement{x+(right ? width-margin : margin),y+(bottom ? height-margin : margin),
                                   right ? 1.f : 0.f,bottom ? 1.f : 0.f};
}

inline constexpr std::uint32_t toleranceRecord = 0x524C4F54;  // TOLR
ToleranceReaction recordToleratedHit(RE::PlayerCharacter& player, RE::Actor& npc, const Settings& settings,
                                     ToleranceHit hit);
void saveTolerance(const SKSE::SerializationInterface& archive);
void loadTolerance(const SKSE::SerializationInterface& archive, std::uint32_t version, std::uint32_t length);
void resetTolerance();
void updateToleranceMeter(RE::PlayerCharacter& player, const Settings& settings, bool visible);
std::optional<ToleranceMeterDisplay> readToleranceMeter();
}
