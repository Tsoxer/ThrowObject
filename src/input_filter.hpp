#pragma once

#include <utility>
#include <vector>

namespace to
{
template<class Event, class Claim, class Pass>
decltype(auto) filterInputList(Event* const* events, Claim claim, Pass pass)
{
    if (!events || !*events) return pass(events);
    auto* head = *events;
    struct RestoreLinks {
        std::vector<std::pair<Event**,Event*>> links;
        ~RestoreLinks()
        {
            for (auto it=links.rbegin(); it!=links.rend(); ++it) *it->first=it->second;
        }
    } removed;
    for (auto** link = &head; *link;) {
        auto* value = *link;
        if (claim(*value)) {
            removed.links.emplace_back(link,value);
            *link = value->next;
        } else link = &value->next;
    }
    // Give the engine its original list object whenever no action owns input.
    return pass(removed.links.empty() ? events : &head);
}
}
