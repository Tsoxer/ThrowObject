#pragma once

#include <cstdint>
#include <type_traits>

namespace to
{
struct Recovery { std::uint32_t id{}, base{}; };
static_assert(sizeof(Recovery)==2*sizeof(std::uint32_t) && std::is_trivially_copyable_v<Recovery>);

enum class RecoveryResult { waiting, stale, restored };

template<class Enable>
RecoveryResult recoverHeldReference(Recovery& record, std::uint32_t referenceBase,
                                   bool deleted, bool discardStale, Enable enable)
{
    if (!referenceBase || referenceBase!=record.base || deleted) {
        if (!discardStale) return RecoveryResult::waiting;
        record = {};
        return RecoveryResult::stale;
    }
    if (!enable()) return RecoveryResult::waiting;

    record = {};
    return RecoveryResult::restored;
}
}
