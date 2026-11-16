#pragma once
#include "model.h"
#include <ArduinoJson.h>

namespace home {
inline bool validCalendarUrl(const std::string& url){
    return url.empty()||(url.size()<=2048&&url.rfind("https://calendar.google.com/calendar/ical/",0)==0&&url.find("\r")==std::string::npos&&url.find("\n")==std::string::npos&&url.size()>=10&&url.substr(url.size()-10)=="/basic.ics");
}
inline bool parseConfig(const std::string& payload,Config& config){
    if(payload.size()>4096)return false;JsonDocument d;
    if(deserializeJson(d,payload,DeserializationOption::NestingLimit(4))||d["version"]!=SCHEMA)return false;
    Config c;
    auto read=[&](const char*k,std::string& value,size_t max){if(d[k].isNull())return true;if(!d[k].is<const char*>())return false;value=d[k].as<const char*>();return value.size()<=max&&value.find('\n')==std::string::npos&&value.find('\r')==std::string::npos;};
    if(!read("ssid",c.ssid,32)||!read("password",c.password,64)||!read("api_key",c.apiKey,128)||!read("calendar_url",c.calendarUrl,2048)||!read("stop",c.stop,16)||!read("stop_name",c.stopName,64)||!read("route",c.route,8)||!read("direction_ref",c.directionRef,32)||!read("direction",c.direction,64)||!read("timezone",c.timezone,64))return false;
    if(!read("calendar_url_2",c.calendarUrl2,2048)||!validCalendarUrl(c.calendarUrl)||!validCalendarUrl(c.calendarUrl2)||c.stop.empty()||c.route.empty())return false;
    if(!read("outbound_route",c.outboundRoute,8)||!read("outbound_stop",c.outboundStop,16)||!read("outbound_direction",c.outboundDirection,64)||c.outboundStop.empty())return false;
    auto integer=[&](const char*k,int&v,int low,int high){if(d[k].isNull())return true;if(!d[k].is<int>())return false;v=d[k].as<int>();return v>=low&&v<=high;};
    if(!integer("transit_seconds",c.transitSeconds,61,3600)||!integer("calendar_seconds",c.calendarSeconds,300,86400)||!integer("night_seconds",c.nightSeconds,300,7200)||!integer("active_start",c.activeStart,0,23)||!integer("active_end",c.activeEnd,0,23))return false;
    if(!integer("gtfs_direction",c.gtfsDirection,-1,1)||!integer("outbound_gtfs_direction",c.outboundGtfsDirection,-1,1))return false;
    if(!d["mock"].is<bool>())return false;c.mock=d["mock"].as<bool>();
    if(!c.mock&&c.ssid.empty())return false;
    config=c;return true;
}
}
