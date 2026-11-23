#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

namespace home {
constexpr uint32_t SCHEMA = 1;
constexpr int MAX_EVENTS = 16;
constexpr int TRANSIT_TTL = 180;
constexpr int CALENDAR_TTL = 6 * 3600;
constexpr size_t CALENDAR_MAX_BYTES = 1024 * 1024;
constexpr const char* LOCAL_TZ = "PST8PDT,M3.2.0,M11.1.0";
struct Arrival { int64_t at=0, observed=0; bool scheduled=false; };
struct Event { int64_t start=0, end=0; uint64_t identity=0; bool allDay=false; char title[96]={}, location[64]={}; };
struct Snapshot {
    uint32_t version=SCHEMA;
    int64_t transitUpdated=0, calendarUpdated=0, transitValid=0, calendarValid=0;
    Arrival arrivals[2]; int arrivalCount=0;
    Event events[MAX_EVENTS]; int eventCount=0;
    // Ordinary RAM is reinitialized on every deep-sleep wake. Live snapshots
    // must stay live even when the retained train/calendar caches are reused.
    bool transitMock=false, calendarMock=false;
    bool calendarIncomplete=false;
    char notice[112]={}; int64_t noticeValid=0;
};
struct Config {
    std::string ssid, password, apiKey, calendarUrl, calendarUrl2;
    std::string displayMode="dashboard";
    std::string stop="13911", stopName="Carl & Cole", route="N", directionRef="", direction="Downtown / Caltrain";
    std::string outboundStop="13909", outboundDirection="Ocean Beach";
    std::string outboundRoute;
    int gtfsDirection=1, outboundGtfsDirection=0;
    std::string timezone=LOCAL_TZ;
    int transitSeconds=61, calendarSeconds=900, nightSeconds=1800, activeStart=6, activeEnd=23;
    bool mock=true;
};
inline const std::string& rowRoute(const Config& c,int row){return row&&!c.outboundRoute.empty()?c.outboundRoute:c.route;}
inline const std::string& rowStop(const Config& c,int row){return row?c.outboundStop:c.stop;}
inline int rowDirection(const Config& c,int row){return row?c.outboundGtfsDirection:c.gtfsDirection;}
inline bool sameTransitSelection(const Config& a,const Config& b){
    for(int i=0;i<2;i++)if(rowRoute(a,i)!=rowRoute(b,i)||rowStop(a,i)!=rowStop(b,i)||rowDirection(a,i)!=rowDirection(b,i))return false;
    return true;
}
inline const std::string& calendarUrlAt(const Config& c,int index){return index?c.calendarUrl2:c.calendarUrl;}
inline bool calendarEnabled(const Config& c,int index){return c.displayMode!="trains"&&!calendarUrlAt(c,index).empty();}
inline uint64_t textIdentity(const std::string& value){
    uint64_t hash=14695981039346656037ULL;
    for(unsigned char c:value)hash=(hash^c)*1099511628211ULL;
    return hash;
}
template<size_t N> inline void copyText(char (&dest)[N], const std::string& src) {
    std::snprintf(dest,N,"%s",src.c_str());
}
inline bool fresh(int64_t observed,int64_t now,int ttl=TRANSIT_TTL) {
    return observed>0 && observed<=now+30 && now<observed+ttl;
}
inline bool arrivalVisible(const Arrival& a,int64_t now,int64_t valid) {
    return now<valid && a.at>now && a.at<=now+86400 && fresh(a.observed,now);
}
inline void expire(Snapshot& s,int64_t now) {
    int n=0;
    for(int i=0;i<s.arrivalCount && i<2;i++) if(arrivalVisible(s.arrivals[i],now,s.transitValid)) s.arrivals[n++]=s.arrivals[i];
    s.arrivalCount=n;
    n=0;
    for(int i=0;i<s.eventCount && i<MAX_EVENTS;i++) if(s.events[i].end>now) s.events[n++]=s.events[i];
    s.eventCount=n;
}
inline std::string clockText(int64_t stamp) {
    if(stamp<=0) return "--:--";
    time_t t=stamp; tm v{}; localtime_r(&t,&v); char out[24];
    std::strftime(out,sizeof(out),"%I:%M %p",&v);
    return out[0]=='0'?std::string(out+1):std::string(out);
}
inline int64_t utcEpoch(int year,int month,int day,int hour=0,int minute=0,int second=0) {
    // Gregorian civil date to Unix epoch, independent of the device timezone.
    year-=month<=2; int era=(year>=0?year:year-399)/400;
    unsigned yoe=year-era*400, doy=(153*(month+(month>2?-3:9))+2)/5+day-1;
    unsigned doe=yoe*365+yoe/4-yoe/100+doy;
    return (int64_t(era)*146097+doe-719468)*86400+hour*3600+minute*60+second;
}
inline int64_t isoTime(const std::string& s) {
    int y,m,d,h,mi,se,n=0;
    if(sscanf(s.c_str(),"%4d-%2d-%2dT%2d:%2d:%2d%n",&y,&m,&d,&h,&mi,&se,&n)!=6) return 0;
    if(y<2020||y>2099||m<1||m>12||d<1||d>31||h>23||mi>59||se>59) return 0;
    if(s[n]=='.'){++n;while(s[n]>='0'&&s[n]<='9') ++n;}
    int offset=0;
    if(s[n]=='Z'&&size_t(n+1)==s.size()) {}
    else {int oh,om,used=0; char sign=s[n];
        if((sign!='+'&&sign!='-')||sscanf(s.c_str()+n+1,"%2d:%2d%n",&oh,&om,&used)!=2||size_t(n+1+used)!=s.size()||oh>23||om>59) return 0;
        offset=(oh*60+om)*60*(sign=='+'?1:-1);
    }
    return utcEpoch(y,m,d,h,mi,se)-offset;
}
inline bool activeAt(int64_t now,const Config& c) {
    time_t t=now;tm v{};localtime_r(&t,&v);
    if(c.activeStart==c.activeEnd) return true;
    if(c.activeStart<c.activeEnd) return v.tm_hour>=c.activeStart&&v.tm_hour<c.activeEnd;
    return v.tm_hour>=c.activeStart||v.tm_hour<c.activeEnd;
}
inline uint32_t fingerprint(const uint8_t* data,size_t size) {
    uint32_t h=2166136261u;for(size_t i=0;i<size;i++)h=(h^data[i])*16777619u;return h;
}
inline Snapshot mockData(int64_t now) {
    Snapshot s; s.transitMock=s.calendarMock=true;s.transitUpdated=s.calendarUpdated=now;
    s.transitValid=now+TRANSIT_TTL;s.calendarValid=now+CALENDAR_TTL;
    s.arrivalCount=2;s.arrivals[0]={now+240,now,false};s.arrivals[1]={now+660,now,false};
    s.eventCount=6;
    s.events[0].start=now+1800;s.events[0].end=now+5400;copyText(s.events[0].title,"Pick up the kids");copyText(s.events[0].location,"School");
    s.events[1].start=now+7200;s.events[1].end=now+10800;copyText(s.events[1].title,"Dinner with friends");copyText(s.events[1].location,"At home");
    s.events[2].start=now+86400;s.events[2].end=now+172800;s.events[2].allDay=true;copyText(s.events[2].title,"A day together");copyText(s.events[2].location,"Tomorrow");
    s.events[3].start=now+2*86400+1800;s.events[3].end=s.events[3].start+3600;copyText(s.events[3].title,"Library visit");
    s.events[4].start=now+2*86400+7200;s.events[4].end=s.events[4].start+3600;copyText(s.events[4].title,"Pick up groceries");
    s.events[5].start=now+3*86400+1800;s.events[5].end=s.events[5].start+3600;copyText(s.events[5].title,"Family walk");
    return s;
}
}
