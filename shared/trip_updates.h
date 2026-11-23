#pragma once
#include "train_board.h"
#include <string_view>
#include <vector>

namespace home {
// Bounded, allocation-free protobuf wire reader. Field numbers follow the
// official google/transit gtfs-realtime.proto. Unknown fields are skipped.
// Only FULL_DATASET feeds with explicit absolute arrival/departure times apply.
namespace pb {
struct Field { unsigned number=0,wire=0;uint64_t value=0;std::string_view bytes; };
struct Reader {
    std::string_view data;size_t pos=0;bool ok=true;
    bool varint(uint64_t& value){
        value=0;
        for(unsigned shift=0;shift<70;shift+=7){
            if(pos>=data.size()){ok=false;return false;}
            uint8_t c=data[pos++];if(shift==63&&c>1){ok=false;return false;}
            value|=uint64_t(c&127)<<shift;if(!(c&128))return true;
        }
        ok=false;return false;
    }
    bool next(Field& f){
        if(!ok||pos==data.size())return false;
        uint64_t tag=0;if(!varint(tag)||tag<8||tag>>3>0x1fffffff){ok=false;return false;}
        f={};f.number=tag>>3;f.wire=tag&7;uint64_t size=0;
        if(f.wire==0)return varint(f.value);
        if(f.wire==2){if(!varint(size))return false;}
        else if(f.wire==1)size=8;
        else if(f.wire==5)size=4;
        else {ok=false;return false;}
        if(size>data.size()-pos){ok=false;return false;}
        f.bytes=data.substr(pos,size);pos+=size;return true;
    }
};
inline bool eventTime(std::string_view bytes,int64_t& at){
    Reader r{bytes};Field f;while(r.next(f))if(f.number==2){if(f.wire!=0)return false;at=int64_t(f.value);}return r.ok;
}
inline bool trip(std::string_view bytes,const Config& c,int64_t source,int64_t now,std::vector<Arrival> (&arrivals)[2]){
    Reader r{bytes};Field f;std::string_view descriptor;int64_t observed=source;
    while(r.next(f)){
        if(f.number==1){if(f.wire!=2)return false;descriptor=f.bytes;}
        if(f.number==4){if(f.wire!=0)return false;observed=int64_t(f.value);}
    }
    if(!r.ok||descriptor.empty())return false;
    Reader d{descriptor};std::string_view route;unsigned relationship=0;int direction=-1;
    while(d.next(f)){
        if(f.number==5){if(f.wire!=2)return false;route=f.bytes;}
        if(f.number==4){if(f.wire!=0)return false;relationship=f.value;}
        if(f.number==6){if(f.wire!=0)return false;direction=f.value;}
    }
    if(!d.ok)return false;
    if((route!=rowRoute(c,0)&&route!=rowRoute(c,1))||relationship==3||relationship==7||!fresh(observed,now))return true;
    r=Reader{bytes};
    while(r.next(f))if(f.number==2){
        if(f.wire!=2)return false;
        Reader stop{f.bytes};Field sf;std::string_view id;int64_t arrival=0,departure=0;unsigned relation=0;
        while(stop.next(sf)){
            if(sf.number==4){if(sf.wire!=2)return false;id=sf.bytes;}
            if(sf.number==5){if(sf.wire!=0)return false;relation=sf.value;}
            if(sf.number==2&&(sf.wire!=2||!eventTime(sf.bytes,arrival)))return false;
            if(sf.number==3&&(sf.wire!=2||!eventTime(sf.bytes,departure)))return false;
        }
        if(!stop.ok)return false;
        if(relation==1||relation==2)continue;
        int64_t at=arrival?arrival:departure;
        for(int row=0;row<2;row++){
            if(route!=rowRoute(c,row)||id!=rowStop(c,row))continue;
            if(direction!=-1&&rowDirection(c,row)!=-1&&direction!=rowDirection(c,row))continue;
            if(at>now&&at<now+86400)arrivals[row].push_back({at,observed,false});
        }
    }
    return r.ok;
}
}
inline bool parseTripUpdates(const std::string& body,const Config& c,int64_t now,TrainBoard& result){
    if(body.empty()||body.size()>2*1024*1024)return false;
    pb::Reader feed{body};pb::Field f;int64_t stamp=0;bool header=false;unsigned incrementality=0;
    while(feed.next(f))if(f.number==1){
        if(f.wire!=2||header)return false;header=true;
        pb::Reader h{f.bytes};pb::Field hf;std::string_view version;
        while(h.next(hf)){
            if(hf.number==1){if(hf.wire!=2)return false;version=hf.bytes;}
            if(hf.number==2){if(hf.wire!=0)return false;incrementality=hf.value;}
            if(hf.number==3){if(hf.wire!=0)return false;stamp=int64_t(hf.value);}
        }
        if(!h.ok||(version!="1.0"&&version!="2.0"))return false;
    }
    if(!feed.ok||!header||incrementality||!fresh(stamp,now))return false;
    std::vector<Arrival> arrivals[2];feed=pb::Reader{body};
    while(feed.next(f))if(f.number==2){
        if(f.wire!=2)return false;
        pb::Reader entity{f.bytes};pb::Field ef;bool deleted=false;std::string_view trip;
        while(entity.next(ef)){
            if(ef.number==2){if(ef.wire!=0)return false;deleted=ef.value!=0;}
            if(ef.number==3){if(ef.wire!=2)return false;trip=ef.bytes;}
        }
        if(!entity.ok)return false;
        if(!deleted&&!trip.empty()&&!pb::trip(trip,c,stamp,now,arrivals))return false;
    }
    if(!feed.ok)return false;
    TrainBoard parsed;
    for(int i=0;i<2;i++){
        auto& a=arrivals[i];std::sort(a.begin(),a.end(),[](const Arrival&x,const Arrival&y){return x.at<y.at;});
        a.erase(std::unique(a.begin(),a.end(),[](const Arrival&x,const Arrival&y){return x.at==y.at;}),a.end());
        const size_t limit=c.displayMode=="trains"?MAX_TRAIN_ARRIVALS:2;
        auto& row=parsed.rows[i];row.updated=stamp;row.count=std::min(limit,a.size());
        for(int j=0;j<row.count;j++)row.arrivals[j]=a[j];
    }
    result=parsed;return true;
}
}
