#include "calendar.h"
#include "agenda.h"
#include "agenda_storage.h"
#include "transit.h"
#include "trip_updates.h"
#include "alerts.h"
#include "render.h"
#include "app_config.h"
#include "schedule.h"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace home;
std::string read(const char*p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
    setenv("TZ",LOCAL_TZ,1);tzset();
    if(argc<2)return 1;
    std::string cmd=argv[1];JsonDocument out;Snapshot s;Config c;
    int64_t now=argc>3?std::stoll(argv[3]):utcEpoch(2026,11,5,0,42);
    if(cmd=="tripupdates"&&argc>4&&!parseConfig(read(argv[4]),c))return 2;
    if(cmd=="calendar"){
        std::string error;bool ok=parseCalendar(read(argv[2]),now,s,error);out["ok"]=ok;out["error"]=error;
        for(int i=0;i<s.eventCount;i++){auto e=out["events"].add<JsonObject>();e["start"]=s.events[i].start;e["end"]=s.events[i].end;e["all_day"]=s.events[i].allDay;e["title"]=s.events[i].title;}
    }else if(cmd=="calendar-cache"){
        JsonDocument input;deserializeJson(input,read(argv[2]));
        std::string error;CalendarCache saved,restored;
        bool parsed=parseCalendar(input["calendar"].as<std::string>(),now,s,error);out["parsed"]=parsed;
        if(!parsed)return 2;
        saveAgenda(saved,s,"test-calendar");auto bytes=encodeAgenda(saved);out["bytes"]=bytes.size();
        if(input["truncate"].is<int>())bytes.resize(std::min(bytes.size(),size_t(input["truncate"].as<int>())));
        if(input["corrupt"].is<int>()&&input["corrupt"].as<unsigned>()<bytes.size())bytes[input["corrupt"].as<unsigned>()]^=1;
        if(input["append"].as<bool>())bytes.push_back(0);
        restored.source=123;restored.count=1;copyText(restored.events[0].title,"Keep existing cache");
        bool ok=decodeAgenda(bytes.data(),bytes.size(),restored);out["ok"]=ok;
        out["unchanged"]=restored.source==123&&restored.count==1&&std::string(restored.events[0].title)=="Keep existing cache";
        if(ok){out["source_matches"]=restored.source==saved.source;out["updated"]=restored.updated;out["valid"]=restored.valid;
            for(int i=0;i<restored.count;i++){auto e=out["events"].add<JsonObject>();const auto& event=restored.events[i];e["title"]=event.title;e["start"]=event.start;e["end"]=event.end;e["all_day"]=event.allDay;e["identity_matches"]=event.identity==saved.events[i].identity;}}
    }else if(cmd=="agendas"){
        JsonDocument input;deserializeJson(input,read(argv[2]));CalendarCache caches[2];
        c.calendarUrl="https://calendar.google.com/calendar/ical/one/public/basic.ics";
        c.calendarUrl2="https://calendar.google.com/calendar/ical/two/public/basic.ics";
        for(JsonObject update:input["updates"].as<JsonArray>()){
            int index=update["source"];if(index<0||index>1)return 2;
            std::string error;Snapshot parsed;bool ok=parseCalendar(read(update["path"]),update["now"].as<int64_t>(),parsed,error);
            if(ok){saveAgenda(caches[index],parsed,calendarUrlAt(c,index));
                if(input["persist"].as<bool>()){
                    auto bytes=encodeAgenda(caches[index]);CalendarCache restored;
                    if(!decodeAgenda(bytes.data(),bytes.size(),restored))return 2;
                    caches[index]=restored;
                }
            }
            out["accepted"].add(ok);
        }
        if(input["clear_second"].as<bool>())c.calendarUrl2.clear();
        if(input["change_second"].as<bool>())c.calendarUrl2+="changed";
        if(input["display_now"].is<int64_t>())now=input["display_now"].as<int64_t>();
        mergeAgendas(caches,c,now,s);out["updated"]=s.calendarUpdated;out["valid"]=s.calendarValid;out["incomplete"]=s.calendarIncomplete;
        for(int i=0;i<s.eventCount;i++){auto e=out["events"].add<JsonObject>();e["title"]=s.events[i].title;e["start"]=s.events[i].start;e["end"]=s.events[i].end;e["all_day"]=s.events[i].allDay;}
    }else if(cmd=="transit"){
        out["ok"]=parseTransit(read(argv[2]),c,now,s);out["updated"]=s.transitUpdated;
        if(argc>4)expire(s,std::stoll(argv[4]));
        for(int i=0;i<s.arrivalCount;i++){auto a=out["arrivals"].add<JsonObject>();a["at"]=s.arrivals[i].at;a["observed"]=s.arrivals[i].observed;a["scheduled"]=s.arrivals[i].scheduled;}
    }else if(cmd=="tripupdates"||cmd=="retained"){
        TrainBoard board;
        auto emit=[&](JsonArray rows,int64_t stamp){for(const auto& row:board.rows){
            auto d=rows.add<JsonObject>();d["updated"]=row.updated;d["received"]=row.received;d["stale"]=rowStale(row,stamp);
            for(int i=0;i<row.count;i++){auto a=d["arrivals"].add<JsonObject>();a["at"]=row.arrivals[i].at;a["minutes"]=row.minutes[i];}
        }};
        if(cmd=="tripupdates"){
            out["ok"]=parseTripUpdates(read(argv[2]),c,now,board);emit(out["rows"].to<JsonArray>(),now);
        }else{
            JsonDocument input;deserializeJson(input,read(argv[2]));
            for(JsonObject frame:input.as<JsonArray>()){
                int64_t at=frame["now"];TrainBoard incoming;
                bool ok=parseTripUpdates(read(frame["path"]),c,at,incoming);
                if(ok)for(int i=0;i<2;i++)retainPredictions(board.rows[i],incoming.rows[i],at);
                auto step=out["steps"].add<JsonObject>();step["ok"]=ok;emit(step["rows"].to<JsonArray>(),at);
            }
        }
    }else if(cmd=="alerts"){
        out["ok"]=parseAlerts(read(argv[2]),c,now,s);out["notice"]=s.notice;out["valid"]=s.noticeValid;
    }else if(cmd=="config"){
        out["ok"]=parseConfig(read(argv[2]),c);
    }else if(cmd=="schedule"){
        Retry r;for(int i=0;i<10;i++){r.failure(now,70);out["delays"].add(r.next-now);}r.success(now,70);out["recovered"]=r.next-now;out["failures"]=r.failures;
        r.failure(now,70,7200);out["retry_after"]=r.next-now;
        s=mockData(now);out["expiry_wake"]=sleepSeconds(now,now+1800,s);
        out["normal_update_wake"]=sleepSeconds(now,now+61,s);
        out["network_wake"]=transitWake(now,now+61)-now;
        out["overdue_wake"]=transitWake(now,now-20)-now;
        s.arrivals[0].at=now+40;out["departure_wake"]=sleepSeconds(now,now+61,s);
        s=mockData(now);s.arrivals[0].observed=now-150;out["observation_expiry_wake"]=sleepSeconds(now,now+61,s);
        s=Snapshot{};s.eventCount=1;
        for(int seconds:{30,3,2,1,0,-1}){s.events[0].end=now+seconds;out["event_end_wakes"].add(sleepSeconds(now,now+1800,s));}
        out["next_active"]=nextActive(now,c);out["active"]=activeAt(now,c);
    }else if(cmd=="preview"){
        std::string mode=argc>4?argv[4]:"mock";s=mockData(now);
        View v;v.now=now;v.wifi=true;v.batteryMv=3980;
        v.simulation=mode!="mock";
        if(mode=="stale"){v.now=now+600;v.wifi=false;s.transitMock=s.calendarMock=false;}
        if(mode=="offline"){v.wifi=false;s.transitMock=s.calendarMock=false;}
        if(mode=="empty"){s.arrivalCount=s.eventCount=0;s.transitMock=s.calendarMock=false;}
        if(mode=="setup"){s={};s.transitMock=s.calendarMock=false;v.setup=true;}
        if(mode=="night"){v.quiet=true;s.transitMock=s.calendarMock=false;}
        if(mode=="wide"){s.arrivals[0].at=now+14*60;s.arrivals[1].at=now+25*60;}
        if(mode=="agenda-finished"||mode=="agenda-filtered"){
            // A completed morning event must not occupy one of the six slots.
            for(int i=5;i>=0;i--)s.events[i+1]=s.events[i];
            s.eventCount=7;s.events[0].start=now-9*3600-30*60;s.events[0].end=now-9*3600;
            copyText(s.events[0].title,"School drop-off");
            if(mode=="agenda-filtered"){for(int i=0;i<6;i++)s.events[i]=s.events[i+1];s.eventCount=6;}
        }
        if(mode=="agenda-days"||mode=="agenda-same-day"){
            for(int i=0;i<s.eventCount;i++){
                s.events[i].start=now+1800+i*(mode=="agenda-days"?86400:3600);
                s.events[i].end=s.events[i].start+1800;
                s.events[i].allDay=mode=="agenda-days"&&i%2;
            }
            copyText(s.events[5].title,"A long event title that needs truncation");
            s.calendarIncomplete=true;
        }
        TrainBoard retained;const TrainBoard* history=nullptr;
        if(mode=="wake-cold"||mode=="wake-timer"){
            // Model the device's RTC caches plus a newly initialized RAM
            // snapshot. Only a cold boot used to explicitly clear demo flags.
            retained=boardFromSnapshot(s,now);history=&retained;
            CalendarCache caches[2];c.calendarUrl="https://calendar.google.com/calendar/ical/test/public/basic.ics";
            saveAgenda(caches[0],s,c.calendarUrl);s=Snapshot{};
            if(mode=="wake-cold")s.transitMock=s.calendarMock=false;
            mergeAgendas(caches,c,now,s);
        }
        uint8_t pixels[48000];Canvas canvas(pixels);render(canvas,s,c,v,history);
        std::ofstream f(argv[2],std::ios::binary);f<<"P4\n800 480\n";f.write((char*)pixels,sizeof(pixels));return 0;
    }else return 2;
    serializeJson(out,std::cout);std::cout<<std::endl;
}
