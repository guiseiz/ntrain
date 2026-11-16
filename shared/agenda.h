#pragma once
#include "model.h"
#include <vector>

namespace home {
struct CalendarCache {
    uint32_t version=1;
    uint64_t source=0;
    int64_t updated=0,valid=0;
    Event events[MAX_EVENTS];int count=0;
};
inline void saveAgenda(CalendarCache& cache,const Snapshot& parsed,const std::string& url){
    cache=CalendarCache{};cache.source=textIdentity(url);
    cache.updated=parsed.calendarUpdated;cache.valid=parsed.calendarValid;cache.count=parsed.eventCount;
    for(int i=0;i<cache.count;i++)cache.events[i]=parsed.events[i];
}
inline void mergeAgendas(const CalendarCache (&calendars)[2],const Config& config,int64_t now,Snapshot& result){
    std::vector<Event> events;int64_t updated=0,valid=0;bool incomplete=false,first=true;
    for(int i=0;i<2;i++){
        const auto& url=calendarUrlAt(config,i);if(url.empty())continue;
        const auto& cache=calendars[i];
        if(cache.version!=1||cache.source!=textIdentity(url)||cache.count<0||cache.count>MAX_EVENTS||!cache.updated){incomplete=true;continue;}
        updated=updated?std::min(updated,cache.updated):cache.updated;
        valid=first?cache.valid:std::min(valid,cache.valid);first=false;
        for(int j=0;j<cache.count;j++){
            const auto& event=cache.events[j];if(event.end<=now)continue;
            // Invitations exported by both calendars share a UID. Different
            // events with the same title/time remain distinct.
            bool duplicate=false;
            for(const auto& existing:events)if(event.identity&&event.identity==existing.identity&&event.start==existing.start&&event.end==existing.end){duplicate=true;break;}
            if(!duplicate)events.push_back(event);
        }
    }
    std::stable_sort(events.begin(),events.end(),[](const Event&a,const Event&b){return a.start<b.start;});
    result.eventCount=std::min(size_t(MAX_EVENTS),events.size());
    for(int i=0;i<result.eventCount;i++)result.events[i]=events[i];
    result.calendarUpdated=updated;result.calendarValid=valid;result.calendarMock=false;result.calendarIncomplete=incomplete;
}
}
