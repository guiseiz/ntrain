#pragma once
#include "transit.h"
namespace home {
inline bool parseAlerts(const std::string&body,const Config&c,int64_t now,Snapshot&s){
    if(body.size()>128*1024)return false;JsonDocument d;
    if(deserializeJson(d,body,DeserializationOption::NestingLimit(20)))return false;
    if(!d["Header"].is<JsonObject>()||!d["Entities"].is<JsonArray>())return false;
    int64_t stamp=d["Header"]["Timestamp"]|int64_t(0);
    if(!fresh(stamp,now,1800))return false;
    std::string text;int64_t until=now+1800;
    each(d["Entities"],[&](JsonVariantConst entity){
        auto a=entity["Alert"];if(a.isNull())return;
        bool inPeriod=a["ActivePeriods"].isNull()||a["ActivePeriods"].size()==0;
        int64_t end=now+1800;
        each(a["ActivePeriods"],[&](JsonVariantConst p){int64_t start=p["Start"]|int64_t(0),stop=p["End"]|int64_t(0);if((!start||start<=now)&&(!stop||stop>now)){inPeriod=true;if(stop)end=std::min(end,stop);}});
        if(!inPeriod)return;
        bool relevant=false;
        each(a["InformedEntities"],[&](JsonVariantConst e){
            auto agency=jsonString(e["AgencyId"]),route=jsonString(e["RouteId"]),stop=jsonString(e["StopId"]);
            if(!agency.empty()&&agency!="SF")return;
            if(!route.empty()&&route!=c.route)return;
            if(!stop.empty()&&stop!=c.stop)return;
            relevant=true;
        });
        if(!relevant||!text.empty())return;
        each(a["HeaderText"]["Translations"],[&](JsonVariantConst t){auto lang=jsonString(t["Language"]);if(text.empty()&&(lang.empty()||lang=="en"))text=jsonString(t["Text"]);});
        until=std::min(until,end);
    });
    copyText(s.notice,text);s.noticeValid=until;return true;
}
}
