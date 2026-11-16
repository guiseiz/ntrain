#pragma once
#include "model.h"

namespace home {
// Display history is distinct from live-feed validity. Empty/error responses
// never erase a received pair, and cannot relabel it as freshly received.
struct TrainRow {
    Arrival arrivals[2];
    int minutes[2]={}, count=0;
    int64_t updated=0, received=0;
};
struct TrainBoard { TrainRow rows[2]; };
inline bool retainPredictions(TrainRow& saved,const TrainRow& incoming,int64_t now){
    if(incoming.count<1||incoming.count>2||incoming.updated<saved.updated||!fresh(incoming.updated,now))return false;
    for(int i=0;i<incoming.count;i++)if(!arrivalVisible(incoming.arrivals[i],now,incoming.updated+TRANSIT_TTL))return false;
    TrainRow next=incoming;next.received=now;
    for(int i=0;i<next.count;i++)next.minutes[i]=std::max(int64_t(1),(next.arrivals[i].at-now+59)/60);
    // When only one prediction arrives, keep the last second slot if it still
    // describes a different, later train. Its own observation age stays visible.
    if(next.count==1&&saved.count==2&&saved.arrivals[1].at>next.arrivals[0].at){
        next.arrivals[1]=saved.arrivals[1];next.minutes[1]=saved.minutes[1];next.count=2;
    }
    saved=next;return true;
}
inline bool rowStale(const TrainRow& row,int64_t now){
    if(!now||!row.count||now-row.received>90||!fresh(row.updated,now))return true;
    for(int i=0;i<row.count;i++)if(!arrivalVisible(row.arrivals[i],now,row.updated+TRANSIT_TTL))return true;
    return false;
}
inline TrainBoard boardFromSnapshot(const Snapshot& s,int64_t now){
    TrainBoard b;
    for(int direction=0;direction<2;direction++){
        auto& r=b.rows[direction];r.count=s.arrivalCount;r.updated=s.transitUpdated;r.received=now;
        for(int i=0;i<r.count;i++){
            r.arrivals[i]=s.arrivals[i];
            if(direction)r.arrivals[i].at=now+(i?420:180);
            r.minutes[i]=std::max(int64_t(1),(r.arrivals[i].at-now+59)/60);
        }
    }
    return b;
}
}
