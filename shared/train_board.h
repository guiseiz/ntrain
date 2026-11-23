#pragma once
#include "model.h"

namespace home {
// Display history is distinct from live-feed validity. Empty/error responses
// never erase received predictions, and cannot relabel them as freshly received.
constexpr int MAX_TRAIN_ARRIVALS=4;
template<int N> struct TrainRowStorage {
    Arrival arrivals[N];
    int minutes[N]={}, count=0;
    int64_t updated=0, received=0;
};
using TrainRow=TrainRowStorage<MAX_TRAIN_ARRIVALS>;
struct TrainBoard { TrainRow rows[2]; };
struct LegacyTrainBoard { TrainRowStorage<2> rows[2]; };
// Preserve the last received pair when upgrading an existing installation.
inline bool decodeTrainBoard(const void* bytes,size_t size,TrainBoard& result){
    TrainBoard next;
    if(size==sizeof(next))std::memcpy(&next,bytes,size);
    else if(size==sizeof(LegacyTrainBoard)){
        LegacyTrainBoard old;std::memcpy(&old,bytes,size);
        for(int i=0;i<2;i++){
            const auto& source=old.rows[i];auto& target=next.rows[i];
            if(source.count<0||source.count>2)return false;
            target.count=source.count;target.updated=source.updated;target.received=source.received;
            for(int j=0;j<source.count;j++){target.arrivals[j]=source.arrivals[j];target.minutes[j]=source.minutes[j];}
        }
    }else return false;
    for(const auto& row:next.rows)if(row.count<0||row.count>MAX_TRAIN_ARRIVALS)return false;
    result=next;return true;
}
inline bool retainPredictions(TrainRow& saved,const TrainRow& incoming,int64_t now){
    if(incoming.count<1||incoming.count>MAX_TRAIN_ARRIVALS||incoming.updated<saved.updated||!fresh(incoming.updated,now))return false;
    for(int i=0;i<incoming.count;i++)if(!arrivalVisible(incoming.arrivals[i],now,incoming.updated+TRANSIT_TTL))return false;
    TrainRow next=incoming;next.received=now;
    for(int i=0;i<next.count;i++)next.minutes[i]=std::max(int64_t(1),(next.arrivals[i].at-now+59)/60);
    // A partial update can retain later cached trains, in order, without
    // duplicating incoming times or pretending the older observations are fresh.
    for(int i=0;i<saved.count&&next.count<MAX_TRAIN_ARRIVALS;i++){
        if(saved.arrivals[i].at<=next.arrivals[next.count-1].at)continue;
        next.arrivals[next.count]=saved.arrivals[i];next.minutes[next.count]=saved.minutes[i];next.count++;
    }
    saved=next;return true;
}
inline bool rowStale(const TrainRow& row,int64_t now,int visible=MAX_TRAIN_ARRIVALS){
    if(!now||!row.count||now-row.received>90||!fresh(row.updated,now))return true;
    for(int i=0;i<std::min(row.count,visible);i++)if(!arrivalVisible(row.arrivals[i],now,row.updated+TRANSIT_TTL))return true;
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
