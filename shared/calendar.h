#pragma once
#include "model.h"
extern "C" {
#include <ical.h>
}
#include <map>
#include <set>
#include <vector>
#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace home {
inline constexpr const char* PACIFIC_ZONE =
"BEGIN:VTIMEZONE\r\nTZID:America/Los_Angeles\r\n"
"BEGIN:DAYLIGHT\r\nDTSTART:20070311T020000\r\nTZOFFSETFROM:-0800\r\nTZOFFSETTO:-0700\r\nRRULE:FREQ=YEARLY;BYMONTH=3;BYDAY=2SU\r\nEND:DAYLIGHT\r\n"
"BEGIN:STANDARD\r\nDTSTART:20071104T020000\r\nTZOFFSETFROM:-0700\r\nTZOFFSETTO:-0800\r\nRRULE:FREQ=YEARLY;BYMONTH=11;BYDAY=1SU\r\nEND:STANDARD\r\nEND:VTIMEZONE\r\n";
inline int64_t calendarEpoch(icaltimetype t,icaltimezone* fallback){
    if(icaltime_is_null_time(t)||!icaltime_is_valid_time(t))return 0;
    t.is_date=0;
    return icaltime_as_timet_with_zone(t,t.zone?t.zone:fallback);
}
inline bool parseCalendar(const std::string& body,int64_t now,Snapshot& result,std::string& error){
    error.clear();
    if(body.empty()||body.size()>CALENDAR_MAX_BYTES||body.find("END:VCALENDAR")==std::string::npos){error="Calendar feed missing or too large";return false;}
    icalerror_set_errors_are_fatal(0);
    auto* root=icalparser_parse_string(body.c_str());
    if(!root){error="Calendar could not be parsed";return false;}
    struct Cleanup {icalcomponent*p;~Cleanup(){icalcomponent_free(p);icalmemory_free_ring();}} cleanup{root};
    if(icalcomponent_isa(root)!=ICAL_VCALENDAR_COMPONENT||icalcomponent_count_errors(root)>0){error="Calendar format needs attention";return false;}
    if(!icalcomponent_get_timezone(root,"America/Los_Angeles")){
        auto* zone=icalparser_parse_string(PACIFIC_ZONE);if(zone)icalcomponent_add_component(root,zone);
    }
    if(!icalcomponent_get_timezone(root,"Etc/UTC")){
        auto*zone=icalparser_parse_string("BEGIN:VTIMEZONE\r\nTZID:Etc/UTC\r\nBEGIN:STANDARD\r\nDTSTART:19700101T000000\r\nTZOFFSETFROM:+0000\r\nTZOFFSETTO:+0000\r\nEND:STANDARD\r\nEND:VTIMEZONE\r\n");
        if(zone)icalcomponent_add_component(root,zone);
    }
    std::string defaultZone="America/Los_Angeles";
    for(auto*p=icalcomponent_get_first_property(root,ICAL_X_PROPERTY);p;p=icalcomponent_get_next_property(root,ICAL_X_PROPERTY)){
        const char*name=icalproperty_get_x_name(p);if(name&&std::string(name)=="X-WR-TIMEZONE")defaultZone=icalproperty_get_x(p);
    }
    auto* fallback=(defaultZone=="UTC"||defaultZone=="Etc/UTC")?icaltimezone_get_utc_timezone():icalcomponent_get_timezone(root,defaultZone.c_str());
    if(!fallback){error="Calendar timezone definition missing";return false;}
    std::vector<icalcomponent*> events;
    for(auto*e=icalcomponent_get_first_component(root,ICAL_VEVENT_COMPONENT);e;e=icalcomponent_get_next_component(root,ICAL_VEVENT_COMPONENT)){
        if(events.size()>=2500){error="Calendar has too many events";return false;}
        if(!icalcomponent_get_uid(e)){error="Calendar event missing identifier";return false;}
        if(icalcomponent_get_first_property(e,ICAL_EXRULE_PROPERTY)){error="Calendar EXRULE is unsupported";return false;}
        // Unknown timezones are an error, never silently interpreted as UTC.
        for(auto*p=icalcomponent_get_first_property(e,ICAL_ANY_PROPERTY);p;p=icalcomponent_get_next_property(e,ICAL_ANY_PROPERTY)){
            if(auto*z=icalproperty_get_first_parameter(p,ICAL_TZID_PARAMETER)){
                const char*id=icalparameter_get_tzid(z);
                if(id&&std::string(id)!="UTC"&&!icalcomponent_get_timezone(root,id)){error="Calendar timezone definition missing";return false;}
            }
            if(auto*r=icalproperty_get_first_parameter(p,ICAL_RANGE_PARAMETER)){
                if(icalparameter_get_range(r)==ICAL_RANGE_THISANDFUTURE){error="Calendar has unsupported range override";return false;}
            }
        }
        events.push_back(e);
    }
    auto uid=[](icalcomponent*e){const char*p=icalcomponent_get_uid(e);return p?std::string(p):std::string();};
    std::set<std::pair<std::string,int64_t>> overrides;
    for(auto*e:events){auto rid=icalcomponent_get_recurrenceid(e);if(!icaltime_is_null_time(rid))overrides.insert({uid(e),calendarEpoch(rid,fallback)});}
    std::vector<Event> output;std::set<std::pair<std::string,int64_t>> seen;int steps=0;
    int64_t windowEnd=now+7*86400;
    for(auto*e:events){
        if(icalcomponent_get_status(e)==ICAL_STATUS_CANCELLED)continue;
        auto start=icalcomponent_get_dtstart(e);if(icaltime_is_null_time(start)){error="Calendar event missing start";return false;}
        auto end=icalcomponent_get_dtend(e);bool allDay=start.is_date;
        int64_t base=calendarEpoch(start,fallback);
        int duration=0,days=0;
        if(allDay){
            days=icaltime_is_null_time(end)?1:int((utcEpoch(end.year,end.month,end.day)-utcEpoch(start.year,start.month,start.day))/86400);
            if(days<1)days=1;
        }else if(!icaltime_is_null_time(end))duration=calendarEpoch(end,fallback)-base;
        else duration=icaldurationtype_as_int(icalcomponent_get_duration(e));
        if(duration<0||duration>366*86400||days>366){error="Calendar event duration invalid";return false;}
        bool isOverride=!icaltime_is_null_time(icalcomponent_get_recurrenceid(e));
        auto add=[&](icaltimetype occurrence,int customDuration=-1){
            int64_t begin=calendarEpoch(occurrence,fallback),finish;
            if(allDay){auto till=occurrence;icaltime_adjust(&till,days,0,0,0);finish=calendarEpoch(till,fallback);}
            else finish=begin+std::max(1,customDuration>=0?customDuration:duration);
            if(begin>=windowEnd||finish<=now)return;
            if(!isOverride&&overrides.count({uid(e),begin}))return;
            auto compare=occurrence;
            if(!isOverride&&icalproperty_recurrence_is_excluded(e,&start,&compare))return;
            if(!seen.insert({uid(e),begin}).second)return;
            Event out;out.start=begin;out.end=finish;out.allDay=allDay;out.identity=textIdentity(uid(e));
            auto*title=icalcomponent_get_summary(e);auto*location=icalcomponent_get_location(e);
            copyText(out.title,title?title:"Untitled event");copyText(out.location,location?location:"");
            output.push_back(out);
            std::sort(output.begin(),output.end(),[](const Event&a,const Event&b){return a.start<b.start;});
            if(output.size()>MAX_EVENTS)output.resize(MAX_EVENTS);
        };
        // libical stores an iterator cursor on each component. Exclusion checks move it,
        // so collect rule properties before expanding any occurrences.
        std::vector<icalproperty*> rules,dates;
        for(auto*p=icalcomponent_get_first_property(e,ICAL_RRULE_PROPERTY);p;p=icalcomponent_get_next_property(e,ICAL_RRULE_PROPERTY))rules.push_back(p);
        for(auto*p=icalcomponent_get_first_property(e,ICAL_RDATE_PROPERTY);p;p=icalcomponent_get_next_property(e,ICAL_RDATE_PROPERTY))dates.push_back(p);
        add(start);
        if(isOverride)continue;
        for(auto*p:rules){
            auto rule=icalproperty_get_rrule(p);
            if(rule.freq<ICAL_DAILY_RECURRENCE){error="Calendar recurrence is too frequent";return false;}
            auto*iter=icalrecur_iterator_new(rule,start);
            if(!iter){error="Calendar recurrence is invalid";return false;}
            if(!rule.count){
                auto from=icaltime_from_timet_with_zone(now-std::max(duration,days*86400),start.is_date,start.zone?start.zone:fallback);
                if(icaltime_compare(from,start)>0)icalrecur_iterator_set_start(iter,from);
            }
            bool bounded=true;
            while(true){
                if(++steps>30000){bounded=false;break;}
#ifdef ARDUINO
                if(steps%64==0)delay(1);
#endif
                auto occurrence=icalrecur_iterator_next(iter);
                if(icaltime_is_null_time(occurrence)||calendarEpoch(occurrence,fallback)>=windowEnd)break;
                add(occurrence);
            }
            icalrecur_iterator_free(iter);
            if(!bounded){error="Calendar recurrence limit reached";return false;}
        }
        for(auto*p:dates){
            auto date=icalproperty_get_rdate(p);
            if(!icaltime_is_null_time(date.time))add(date.time);
            else if(!icaltime_is_null_time(date.period.start)){
                int seconds=icaltime_is_null_time(date.period.end)?icaldurationtype_as_int(date.period.duration):calendarEpoch(date.period.end,fallback)-calendarEpoch(date.period.start,fallback);
                add(date.period.start,seconds);
            }
        }
    }
    result.eventCount=output.size();for(int i=0;i<result.eventCount;i++)result.events[i]=output[i];
    result.calendarUpdated=now;result.calendarValid=now+CALENDAR_TTL;result.calendarMock=false;
    return true;
}
}
