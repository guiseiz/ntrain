#pragma once
#include "model.h"
#include <ArduinoJson.h>
#include <vector>

namespace home {
inline std::string jsonString(JsonVariantConst v) {
    if(v.is<const char*>())return v.as<const char*>();
    if(v.is<int>())return std::to_string(v.as<int>());
    return {};
}
template<class F> void each(JsonVariantConst v,F f) {
    if(v.is<JsonArrayConst>())for(JsonVariantConst a:v.as<JsonArrayConst>())f(a);
    else if(v.is<JsonObjectConst>())f(v);
}
inline bool falseStatus(JsonVariantConst v){return v.is<bool>()&&!v.as<bool>();}
inline bool parseTransit(const std::string& body,const Config& c,int64_t now,Snapshot& result) {
    if(body.empty()||body.size()>128*1024)return false;
    JsonDocument doc;
    if(deserializeJson(doc,body,DeserializationOption::NestingLimit(20)))return false;
    // 511's live JSON omits the XML/SIRI envelope. Accept both documented forms.
    JsonVariantConst service=doc["ServiceDelivery"];
    if(service.isNull())service=doc["Siri"]["ServiceDelivery"];
    if(!service.is<JsonObjectConst>()||falseStatus(service["Status"]))return false;
    auto deliveries=service["StopMonitoringDelivery"].as<JsonVariantConst>();
    if(!deliveries.is<JsonObjectConst>()&&!deliveries.is<JsonArrayConst>())return false;
    std::vector<Arrival> arrivals;bool valid=false; int64_t sourceTime=0;std::string notice;
    each(deliveries,[&](JsonVariantConst delivery){
        if(falseStatus(delivery["Status"]))return;
        int64_t stamp=isoTime(jsonString(delivery["ResponseTimestamp"]));
        if(!stamp)stamp=isoTime(jsonString(service["ResponseTimestamp"]));
        if(!fresh(stamp,now))return;
        valid=true;sourceTime=std::max(sourceTime,stamp);
        std::vector<std::string> cancelled;
        each(delivery["MonitoredStopVisitCancellation"],[&](JsonVariantConst item){
            auto id=jsonString(item["ItemRef"]);if(!id.empty())cancelled.push_back(id);
            auto trip=jsonString(item["VehicleJourneyRef"]["DatedVehicleJourneyRef"]);if(!trip.empty())cancelled.push_back(trip);
        });
        each(delivery["MonitoredStopVisit"],[&](JsonVariantConst visit){
            auto journey=visit["MonitoredVehicleJourney"],call=journey["MonitoredCall"];
            if(jsonString(journey["LineRef"])!=c.route)return;
            if(!c.directionRef.empty()&&jsonString(journey["DirectionRef"])!=c.directionRef)return;
            std::string stop=jsonString(call["StopPointRef"]);if(stop.empty())stop=jsonString(visit["MonitoringRef"]);
            if(stop!=c.stop)return;
            if(journey["Cancellation"].as<bool>()||call["Cancellation"].as<bool>()||jsonString(call["DepartureStatus"])=="cancelled"||jsonString(call["ArrivalStatus"])=="cancelled")return;
            std::string id=jsonString(visit["ItemIdentifier"]), trip=jsonString(journey["FramedVehicleJourneyRef"]["DatedVehicleJourneyRef"]);
            if((!id.empty()&&std::find(cancelled.begin(),cancelled.end(),id)!=cancelled.end())||(!trip.empty()&&std::find(cancelled.begin(),cancelled.end(),trip)!=cancelled.end()))return;
            int64_t observed=isoTime(jsonString(visit["RecordedAtTime"]));if(!observed)observed=stamp;
            if(!fresh(observed,now))return;
            int64_t at=isoTime(jsonString(call["ExpectedArrivalTime"]));
            if(!at)at=isoTime(jsonString(call["ExpectedDepartureTime"]));
            bool scheduled=!at||falseStatus(journey["Monitored"]);
            if(scheduled){at=isoTime(jsonString(call["AimedArrivalTime"]));if(!at)at=isoTime(jsonString(call["AimedDepartureTime"]));}
            if(at>now&&at<now+86400)arrivals.push_back({at,observed,scheduled});
        });
        each(delivery["StopLineNotice"],[&](JsonVariantConst item){
            std::string line=jsonString(item["LineRef"]);
            if(line.empty()||line==c.route)notice=jsonString(item["LineNote"]);
        });
    });
    if(!valid)return false;
    std::sort(arrivals.begin(),arrivals.end(),[](const Arrival&a,const Arrival&b){return a.at<b.at;});
    arrivals.erase(std::unique(arrivals.begin(),arrivals.end(),[](const Arrival&a,const Arrival&b){return a.at==b.at&&a.scheduled==b.scheduled;}),arrivals.end());
    result.arrivalCount=std::min(size_t(2),arrivals.size());
    for(int i=0;i<result.arrivalCount;i++)result.arrivals[i]=arrivals[i];
    result.transitMock=false;result.transitUpdated=sourceTime;result.transitValid=sourceTime+TRANSIT_TTL;
    if(!notice.empty()){copyText(result.notice,notice);result.noticeValid=sourceTime+TRANSIT_TTL;}
    return true;
}
}
