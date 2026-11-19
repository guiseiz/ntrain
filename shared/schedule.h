#pragma once
#include "model.h"
namespace home {
constexpr int DISPLAY_REFRESH_SECONDS=3;
constexpr int NETWORK_WAKE_LEAD=5;
inline int64_t transitWake(int64_t now,int64_t due){return std::max(now+1,due-NETWORK_WAKE_LEAD);}
struct Retry {
    int failures=0; int64_t next=0;
    void success(int64_t now,int interval){failures=0;next=now+interval;}
    void failure(int64_t now,int base,int retryAfter=0){
        failures=std::min(8,failures+1);
        int wait=std::min(3600,base*(1<<std::min(5,failures-1)));
        next=now+std::max(wait,retryAfter);
    }
    bool due(int64_t now)const{return now>=next;}
};
inline int64_t nextActive(int64_t now,const Config&c){
    time_t t=now;tm local{};localtime_r(&t,&local);local.tm_min=0;local.tm_sec=0;local.tm_hour=c.activeStart;local.tm_isdst=-1;
    int64_t next=mktime(&local);if(next<=now){local.tm_mday++;local.tm_isdst=-1;next=mktime(&local);}return next;
}
inline int sleepSeconds(int64_t now,int64_t nextWork,const Snapshot&s){
    int64_t next=nextWork;
    auto expiry=[&](int64_t deadline){
        // Only expiry redraws need a lead time. A network request should wake at
        // its actual due time; waking early caused extra render/sleep cycles.
        if(deadline>now)next=std::min(next,std::max(now+1,deadline-DISPLAY_REFRESH_SECONDS));
    };
    // Train history is replaced by received predictions, never an expiry frame.
    expiry(s.calendarValid);
    // An event can finish during the panel refresh, after the snapshot was
    // filtered. Wake promptly in that case instead of waiting for the network.
    for(int i=0;i<s.eventCount;i++)next=std::min(next,std::max(now+1,s.events[i].end-DISPLAY_REFRESH_SECONDS));
    return std::max(int64_t(1),next-now);
}
}
