#pragma once
#include "agenda.h"

namespace home {
// Persist only the fields the agenda displays/merges. Raw CalendarCache blobs
// include unused location arrays and padding and can exhaust the 20 KiB NVS
// partition when replacing a cache. The bounded encoding needs under 2 KiB
// per source even with sixteen maximum-length titles; NVS replaces it atomically.
constexpr size_t AGENDA_STORAGE_MAX=4+8*3+1+MAX_EVENTS*(8*3+2+95)+4;
inline std::vector<uint8_t> encodeAgenda(const CalendarCache& cache){
    if(cache.version!=1||cache.count<0||cache.count>MAX_EVENTS)return {};
    std::vector<uint8_t> bytes={'N','A','C','2'};
    auto number=[&](uint64_t value,int size){for(int i=0;i<size;i++){bytes.push_back(value&255);value>>=8;}};
    number(cache.source,8);number(cache.updated,8);number(cache.valid,8);number(cache.count,1);
    for(int i=0;i<cache.count;i++){
        const auto& event=cache.events[i];size_t length=strnlen(event.title,sizeof(event.title));
        if(length>=sizeof(event.title)||event.start<=0||event.end<=event.start)return {};
        number(event.start,8);number(event.end,8);number(event.identity,8);
        number(event.allDay?1:0,1);number(length,1);
        bytes.insert(bytes.end(),event.title,event.title+length);
    }
    number(fingerprint(bytes.data(),bytes.size()),4);
    return bytes;
}
inline bool decodeAgenda(const uint8_t* bytes,size_t size,CalendarCache& cache){
    if(size<33||size>AGENDA_STORAGE_MAX||memcmp(bytes,"NAC2",4))return false;
    size_t cursor=4;
    auto number=[&](int count){uint64_t value=0;for(int i=0;i<count;i++)value|=uint64_t(bytes[cursor++])<<(8*i);return value;};
    uint32_t check=0;for(int i=0;i<4;i++)check|=uint32_t(bytes[size-4+i])<<(8*i);
    if(check!=fingerprint(bytes,size-4))return false;
    // Decode into a separate object so malformed data cannot destroy a cache.
    CalendarCache decoded;
    decoded.source=number(8);decoded.updated=number(8);decoded.valid=number(8);decoded.count=number(1);
    if(decoded.count>MAX_EVENTS)return false;
    for(int i=0;i<decoded.count;i++){
        if(cursor+26>size-4)return false;
        auto& event=decoded.events[i];
        event.start=number(8);event.end=number(8);event.identity=number(8);
        uint8_t allDay=number(1);size_t length=number(1);
        if(allDay>1||length>=sizeof(event.title)||cursor+length>size-4||event.start<=0||event.end<=event.start)return false;
        event.allDay=allDay;memcpy(event.title,bytes+cursor,length);cursor+=length;
    }
    if(cursor!=size-4)return false;
    cache=decoded;return true;
}
}
