#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <ical.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include <atomic>
#include "model.h"
#include "app_config.h"
#include "trip_updates.h"
#include "alerts.h"
#include "calendar.h"
#include "agenda.h"
#include "agenda_storage.h"
#include "render.h"
#include "schedule.h"
#include "network.h"

using namespace home;
// TLS, calendar expansion and rendering share one task; explicitly size its stack.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);
constexpr int PIN_LED=6,PIN_BUTTON=3,PIN_BATTERY=1,PIN_BATTERY_EN=21,PIN_SD_EN=16,PIN_SD_CS=14;
SPIClass panelSPI(HSPI);
GxEPD2_BW<GxEPD2_750_GDEY075T7,480> display(GxEPD2_750_GDEY075T7(10,11,12,13));
Preferences prefs;
RTC_DATA_ATTR uint32_t rtcMagic=0,previousFrame=0;
Snapshot snapshot;
RTC_DATA_ATTR CalendarCache calendars[2];
RTC_DATA_ATTR TrainBoard trains;
RTC_DATA_ATTR int64_t lastTrainCache=0;
RTC_DATA_ATTR Retry transitRetry,calendarRetry[2],wifiRetry;
constexpr const char* CAL_CACHE_KEYS[]={"calendar0","calendar1"};
constexpr const char* CAL_RETRY_KEYS[]={"c0next","c1next"};
RTC_DATA_ATTR int64_t apiPause=0;
RTC_DATA_ATTR bool previousWifi=false;
RTC_DATA_ATTR int64_t lastClockSync=0;
Config config;
uint8_t* framebuffer=nullptr;
std::atomic<int> wifiReason{0};
std::atomic<bool> wifiAssociated{false};

void initWifiDiagnostics(){
    WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t info){
        if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){wifiReason=info.wifi_sta_disconnected.reason;wifiAssociated=false;}
        if(event==ARDUINO_EVENT_WIFI_STA_CONNECTED)wifiAssociated=true;
    });
}
bool joinWifi(const Config&candidate){
    WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);
    WiFi.disconnect(false,false);delay(100);wifiReason=0;wifiAssociated=false;
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);WiFi.setSleep(false);
    WiFi.begin(candidate.ssid.c_str(),candidate.password.c_str());
    // Allow a complete scan, association and DHCP; the old 12-second limit was tight.
    uint32_t deadline=millis()+30000;
    while(WiFi.status()!=WL_CONNECTED&&int32_t(millis()-deadline)<0)delay(100);
    return WiFi.status()==WL_CONNECTED;
}
const char* wifiFailureCode(){
    if(wifiAssociated)return "dhcp_timeout";
    int reason=wifiReason.load();
    if(reason==WIFI_REASON_NO_AP_FOUND)return "network_not_found";
    if(reason==WIFI_REASON_AUTH_FAIL||reason==WIFI_REASON_HANDSHAKE_TIMEOUT||reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT)return "authentication_failed";
    if(reason==WIFI_REASON_ASSOC_FAIL)return "association_failed";
    return "connection_timeout";
}
const char* wifiFailureText(){
    const char* code=wifiFailureCode();
    if(strcmp(code,"network_not_found")==0)return "Wi-Fi network not found";
    if(strcmp(code,"authentication_failed")==0)return "Check Wi-Fi password";
    if(strcmp(code,"dhcp_timeout")==0)return "Wi-Fi address unavailable";
    return "Wi-Fi connection failed";
}
bool testWifi(const Config&candidate){
    bool ok=joinWifi(candidate);JsonDocument result;
    result["ok"]=ok;result["code"]=ok?"connected":wifiFailureCode();result["reason"]=wifiReason.load();
    if(ok)result["rssi"]=WiFi.RSSI();
    Serial.print("NTRAIN_WIFI_RESULT_V1 ");serializeJson(result,Serial);Serial.println();Serial.flush();
    WiFi.disconnect(false,false);WiFi.mode(WIFI_OFF);return ok;
}
void scanWifi(){
    WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);WiFi.disconnect(false,false);
    int count=WiFi.scanNetworks(false,false,false,180);JsonDocument result;result["ok"]=count>=0;
    auto networks=result["networks"].to<JsonArray>();
    for(int i=0;i<count&&networks.size()<32;i++){
        String name=WiFi.SSID(i);if(name.isEmpty())continue;
        bool duplicate=false;for(JsonObject item:networks)if(item["ssid"].as<String>()==name){duplicate=true;break;}
        if(duplicate)continue;
        auto entry=networks.add<JsonObject>();entry["ssid"]=name;entry["rssi"]=WiFi.RSSI(i);
        entry["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
    }
    Serial.print("NTRAIN_WIFI_SCAN_V1 ");serializeJson(result,Serial);Serial.println();Serial.flush();
    WiFi.scanDelete();WiFi.mode(WIFI_OFF);
}

void serialSetup(uint32_t milliseconds){
    // USB is the only configuration interface. No radio provisioning or writable network endpoint.
    Serial.println("NTRAIN_READY_V1");
    Serial.println("NTRAIN_WIFI_SETUP_V1");
    uint32_t until=millis()+milliseconds;std::string line;
    while(int32_t(millis()-until)<0){
        while(Serial.available()){
            char ch=Serial.read();
            if(ch=='\n'){
                if(line=="PING"){Serial.println("NTRAIN_READY_V1");Serial.println("NTRAIN_WIFI_SETUP_V1");}
                else if(line=="STATUS"){
                    JsonDocument status;status["clock"]=time(nullptr);
                    status["firmware_version"]="0.1.2";
                    status["capabilities"].add("transit-rows-v1");
                    for(int i=0;i<2;i++){
                        auto source=status["calendars"].add<JsonObject>();
                        source["configured"]=!calendarUrlAt(config,i).empty();source["cached_events"]=calendars[i].count;
                        source["updated"]=calendars[i].updated;source["next"]=calendarRetry[i].next;
                    }
                    Serial.print("NTRAIN_STATUS_V1 ");serializeJson(status,Serial);Serial.println();
                }
                else if(line=="REFRESH_CALENDARS"){
                    // Explicit USB refresh for initial setup/troubleshooting;
                    // independent of the protected 511 request deadline.
                    for(int i=0;i<2;i++){calendarRetry[i]=Retry{};prefs.remove(CAL_RETRY_KEYS[i]);}
                    Serial.println("NTRAIN_CALENDAR_REFRESH_V1");Serial.flush();until=millis();
                }
                else if(line=="SCAN_WIFI"){scanWifi();until=millis()+15000;}
                else if(line=="TEST_SAVED_WIFI"){testWifi(config);until=millis()+15000;}
                else if(line.rfind("TEST_WIFI ",0)==0){Config candidate;
                    if(parseConfig(line.substr(10),candidate)&&!candidate.ssid.empty())testWifi(candidate);
                    else Serial.println("NTRAIN_CONFIG_REJECTED_V1");
                    until=millis()+15000;
                }
                else {Config candidate;if(parseConfig(line,candidate)){
                    if(!candidate.mock&&!testWifi(candidate)){
                        // Keep the previous settings intact when the candidate cannot join Wi-Fi.
                        until=millis()+15000;
                    }else if(prefs.putString("config",line.c_str())==line.size()){
                        for(int i=0;i<2;i++)if(calendarUrlAt(candidate,i)!=calendarUrlAt(config,i)){
                            prefs.remove(CAL_CACHE_KEYS[i]);prefs.remove(CAL_RETRY_KEYS[i]);
                        }
                        // Keep the request deadline across configuration resets.
                        if(!sameTransitSelection(candidate,config))prefs.remove("trains");
                        Serial.println("NTRAIN_CONFIG_SAVED_V1");Serial.flush();delay(200);ESP.restart();
                    }else Serial.println("NTRAIN_CONFIG_STORAGE_ERROR");
                }else Serial.println("NTRAIN_CONFIG_REJECTED_V1");}
                line.clear();
            }else if(ch!='\r'){
                if(line.size()<4120)line+=ch;
                else {line.clear();Serial.println("NTRAIN_CONFIG_TOO_LARGE");}
            }
        }
        delay(10);
    }
}
int batteryMv(){
    pinMode(PIN_BATTERY_EN,OUTPUT);digitalWrite(PIN_BATTERY_EN,HIGH);
    analogReadResolution(12);analogSetPinAttenuation(PIN_BATTERY,ADC_11db);delay(10);
    int sum=0;for(int i=0;i<8;i++){sum+=analogReadMilliVolts(PIN_BATTERY);delay(1);}
    digitalWrite(PIN_BATTERY_EN,LOW);return sum/4;
}
void draw(const View&view){
    Canvas canvas(framebuffer);render(canvas,snapshot,config,view,&trains);
    uint32_t hash=fingerprint(framebuffer,48000);
    if(hash==previousFrame){Serial.println("DISPLAY_UNCHANGED");return;}
    // GPIO8 is the shared SPI MISO pin. Explicit mapping avoids the Arduino 2.x
    // HSPI fallback to nonexistent default pins; the e-paper transfer is write-only.
    panelSPI.begin(7,8,9,10);
    display.epd2.selectSPI(panelSPI,SPISettings(2000000,MSBFIRST,SPI_MODE0));
    display.init(0,true,10,false);display.setRotation(0);display.setFullWindow();
    display.firstPage();do {display.fillScreen(GxEPD_WHITE);display.drawBitmap(0,0,framebuffer,800,480,GxEPD_BLACK);}while(display.nextPage());
    display.hibernate();panelSPI.end();previousFrame=hash;
    Serial.printf("DISPLAY_REFRESHED hash=%08x heap=%u psram=%u stack_free=%u\n",hash,ESP.getFreeHeap(),ESP.getFreePsram(),uxTaskGetStackHighWaterMark(nullptr));
}
void sleepFor(int seconds){
    WiFi.disconnect(false,false);WiFi.mode(WIFI_OFF);
    digitalWrite(PIN_LED,HIGH);digitalWrite(PIN_BATTERY_EN,LOW);digitalWrite(PIN_SD_EN,LOW);
    Serial.printf("SLEEP seconds=%d\n",seconds);Serial.flush();
    esp_sleep_enable_timer_wakeup(uint64_t(std::max(1,seconds))*1000000);
    if(digitalRead(PIN_BUTTON)==HIGH)esp_sleep_enable_ext1_wakeup(1ULL<<PIN_BUTTON,ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}
void setup(){
    Serial.begin(115200);delay(100);
    initWifiDiagnostics();
    pinMode(PIN_LED,OUTPUT);digitalWrite(PIN_LED,HIGH);
    pinMode(PIN_SD_EN,OUTPUT);digitalWrite(PIN_SD_EN,LOW);pinMode(PIN_SD_CS,OUTPUT);digitalWrite(PIN_SD_CS,HIGH);
    pinMode(PIN_BATTERY_EN,OUTPUT);digitalWrite(PIN_BATTERY_EN,LOW);pinMode(PIN_BUTTON,INPUT_PULLUP);
    // Permit large TLS/feed/parser allocations in the board's verified 8 MB PSRAM.
    if(psramFound())heap_caps_malloc_extmem_enable(512);
    framebuffer=(uint8_t*)ps_malloc(48000);
    if(!framebuffer){Serial.println("ERROR_NO_FRAMEBUFFER");delay(10000);ESP.restart();}
    prefs.begin("ntrain",false);
    bool cold=esp_sleep_get_wakeup_cause()!=ESP_SLEEP_WAKEUP_TIMER&&esp_sleep_get_wakeup_cause()!=ESP_SLEEP_WAKEUP_EXT1;
    if(rtcMagic!=0x4e545233||cold){
        snapshot=Snapshot{};trains=TrainBoard{};lastTrainCache=0;transitRetry=Retry{};wifiRetry=Retry{};previousFrame=0;previousWifi=false;rtcMagic=0x4e545233;
        snapshot.transitMock=snapshot.calendarMock=false;
        // Independent source caches survive failures, deep sleep and power loss.
        for(int i=0;i<2;i++){
            calendars[i]=CalendarCache{};calendarRetry[i]=Retry{};
            auto& cache=calendars[i];
            size_t storedSize=prefs.getBytesLength(CAL_CACHE_KEYS[i]);
            if(storedSize==sizeof(cache)){
                // Read existing installations; replace with compact storage on
                // the next successful sync, without deleting the old cache first.
                prefs.getBytes(CAL_CACHE_KEYS[i],&cache,sizeof(cache));
                if(cache.version!=1||cache.count<0||cache.count>MAX_EVENTS)cache=CalendarCache{};
            }else if(storedSize&&storedSize<=AGENDA_STORAGE_MAX){
                std::vector<uint8_t> bytes(storedSize);
                if(prefs.getBytes(CAL_CACHE_KEYS[i],bytes.data(),bytes.size())==bytes.size())decodeAgenda(bytes.data(),bytes.size(),cache);
            }
            calendarRetry[i].next=prefs.getLong64(CAL_RETRY_KEYS[i],0);
        }
        transitRetry.next=prefs.getLong64("t_next",0);
        apiPause=prefs.getLong64("api_pause",0);
        TrainBoard cachedTrains;
        if(prefs.getBytesLength("trains")==sizeof(cachedTrains)&&prefs.getBytes("trains",&cachedTrains,sizeof(cachedTrains))==sizeof(cachedTrains)){
            bool valid=true;for(const auto& row:cachedTrains.rows)if(row.count<0||row.count>2)valid=false;
            if(valid)trains=cachedTrains;
        }
    }
    auto saved=prefs.getString("config","");bool configured=parseConfig(saved.c_str(),config);
    // Migrate the former 70-second default to the requested one-minute cadence.
    if(config.transitSeconds==70)config.transitSeconds=61;
    setenv("TZ",config.timezone.c_str(),1);tzset();
    if(cold||digitalRead(PIN_BUTTON)==LOW)serialSetup(configured?7000:15000);
    for(int i=0;i<2;i++)if(calendars[i].source!=textIdentity(calendarUrlAt(config,i))){
        bool changed=calendars[i].source!=0;
        calendars[i]=CalendarCache{};calendars[i].source=textIdentity(calendarUrlAt(config,i));
        if(changed)calendarRetry[i]=Retry{};
    }
    int64_t now=time(nullptr);
    if(config.mock||!configured){
        if(now<1700000000)now=1793839320; // Explicit sample date only; never used for real predictions.
        snapshot=mockData(now);trains=boardFromSnapshot(snapshot,now);View v;v.now=now;v.batteryMv=batteryMv();v.error="Configure with USB setup";draw(v);
        // Mock mode is a commissioning screen, with a bounded USB setup window on reset.
        sleepFor(1800);return;
    }
    bool hasClock=now>=1700000000;
    bool active=!hasClock||activeAt(now,config);
    bool needTransit=active&&!config.apiKey.empty()&&now+NETWORK_WAKE_LEAD>=std::max(apiPause,transitRetry.next);
    bool needCalendar=false;for(int i=0;i<2;i++)needCalendar|=!calendarUrlAt(config,i).empty()&&calendarRetry[i].due(now);
    bool needTime=!hasClock||now-lastClockSync>86400;
    bool wantsNetwork=(needTransit||needCalendar||needTime)&&(!hasClock||wifiRetry.due(now));
    View view;view.now=now;view.wifi=previousWifi;view.quiet=!active;view.batteryMv=batteryMv();
    // Keep the last image visible while fetching. Commit one complete new frame
    // after the request finishes instead of clearing still-valid times first.
    if(wantsNetwork){
        previousWifi=view.wifi=joinWifi(config);
        if(view.wifi){
            Serial.printf("WIFI_CONNECTED rssi=%d\n",WiFi.RSSI());
            wifiRetry.success(now,0);
            if(needTime){
                configTzTime(config.timezone.c_str(),"time.google.com","pool.ntp.org");
                tm t; if(getLocalTime(&t,6000)){lastClockSync=time(nullptr);hasClock=true;}
            }
            now=time(nullptr);view.now=now;
            if(hasClock){
                active=activeAt(now,config);view.quiet=!active;
                int64_t due=std::max(apiPause,transitRetry.next);
                if(active&&!config.apiKey.empty()&&now<due&&due-now<=NETWORK_WAKE_LEAD){
                    // Wake just early enough to associate, then respect the exact
                    // request deadline without an extra render/sleep cycle.
                    while(time(nullptr)<due)delay(50);
                    now=time(nullptr);
                }
                if(active&&!config.apiKey.empty()&&now>=apiPause&&transitRetry.due(now)){
                    // Reserve the next request BEFORE making it, so reset loops cannot burst the API.
                    transitRetry.next=now+config.transitSeconds;prefs.putLong64("t_next",transitRetry.next);
                    Serial.printf("TRANSIT_FETCH at=%lld next=%lld\n",now,transitRetry.next);
                    // One bulk feed supplies both directions; no separate alert calls.
                    auto response=getHttps("https://api.511.org/transit/tripupdates?api_key="+encode(config.apiKey)+"&agency=SF",2*1024*1024,true);
                    TrainBoard incoming;
                    if(response.ok()&&parseTripUpdates(response.body,config,time(nullptr),incoming)){
                        int64_t received=time(nullptr)+DISPLAY_REFRESH_SECONDS;
                        for(int i=0;i<2;i++)retainPredictions(trains.rows[i],incoming.rows[i],received);
                        transitRetry.success(now,config.transitSeconds);
                        // RTC holds every update. A small NVS checkpoint every five
                        // minutes also retains history after complete power loss.
                        if(!lastTrainCache||time(nullptr)-lastTrainCache>=300){
                            prefs.putBytes("trains",&trains,sizeof(trains));lastTrainCache=time(nullptr);
                        }
                        Serial.printf("TRANSIT_OK inbound=%d outbound=%d source=%lld bytes=%u elapsed_ms=%u\n",incoming.rows[0].count,incoming.rows[1].count,incoming.rows[0].updated,unsigned(response.body.size()),unsigned(millis()));
                    }else{
                        transitRetry.failure(time(nullptr),config.transitSeconds,response.retryAfter);
                        prefs.putLong64("t_next",transitRetry.next);view.error=response.status==401||response.status==403?"Check 511 API key":"Transit update unavailable";
                        Serial.printf("TRANSIT_ERROR status=%d\n",response.status);
                        if(response.status==429){apiPause=transitRetry.next;prefs.putLong64("api_pause",apiPause);}
                    }
                }
                for(int i=0;i<2;i++){
                    const auto& url=calendarUrlAt(config,i);auto& retry=calendarRetry[i];
                    if(url.empty()||!retry.due(time(nullptr)))continue;
                    retry.next=time(nullptr)+config.calendarSeconds;prefs.putLong64(CAL_RETRY_KEYS[i],retry.next);
                    Serial.printf("CALENDAR_FETCH source=%d\n",i+1);
                    auto response=getHttps(url,CALENDAR_MAX_BYTES);std::string parseError;
                    std::unique_ptr<Snapshot> parsed(new Snapshot);
                    if(response.ok()&&parseCalendar(response.body,time(nullptr),*parsed,parseError)){
                        retry.success(time(nullptr),active?config.calendarSeconds:std::max(config.calendarSeconds,config.nightSeconds));
                        saveAgenda(calendars[i],*parsed,url);
                        auto stored=encodeAgenda(calendars[i]);
                        if(stored.empty()||prefs.putBytes(CAL_CACHE_KEYS[i],stored.data(),stored.size())!=stored.size())Serial.printf("CALENDAR_CACHE_ERROR source=%d\n",i+1);
                        else Serial.printf("CALENDAR_CACHE_SAVED source=%d bytes=%u\n",i+1,unsigned(stored.size()));
                        Serial.printf("CALENDAR_OK source=%d events=%d bytes=%u\n",i+1,calendars[i].count,unsigned(response.body.size()));
                    }else{
                        retry.failure(time(nullptr),config.calendarSeconds,response.retryAfter);
                        view.error=parseError.empty()?"Calendar update unavailable":parseError;
                        Serial.printf("CALENDAR_ERROR source=%d status=%d parse=%s\n",i+1,response.status,parseError.c_str());
                    }
                    prefs.putLong64(CAL_RETRY_KEYS[i],retry.next);
                }
            }else view.error="Waiting for clock sync";
        }else{wifiRetry.failure(now,70);view.error=wifiFailureText();Serial.printf("WIFI_UNAVAILABLE reason=%d status=%d\n",wifiReason.load(),int(WiFi.status()));}
        WiFi.disconnect(false,false);WiFi.mode(WIFI_OFF);
    }
    now=time(nullptr);mergeAgendas(calendars,config,now,snapshot);view.now=hasClock?now+DISPLAY_REFRESH_SECONDS:0;
    Serial.printf("AGENDA_READY first=%d second=%d merged=%d incomplete=%d\n",calendars[0].count,calendars[1].count,snapshot.eventCount,snapshot.calendarIncomplete);
    Serial.printf("DEVICE_STATUS now=%lld live=%d wifi=%d active=%d battery_mv=%d wake=%d reset=%d\n",now,!snapshot.transitMock,view.wifi,active,view.batteryMv,int(esp_sleep_get_wakeup_cause()),int(esp_reset_reason()));
    if(!hasClock){snapshot.eventCount=0;snapshot.calendarValid=0;}
    if(config.apiKey.empty()&&view.wifi&&view.error.empty())view.error="Add your 511 API key";
    expire(snapshot,now+DISPLAY_REFRESH_SECONDS);draw(view);
    // Measure sleep from the end of the panel refresh, not its start.
    now=time(nullptr);
    int64_t next=now+(active?config.transitSeconds:config.nightSeconds);
    if(active&&!config.apiKey.empty())next=transitWake(now,std::max(transitRetry.next,apiPause));
    if(!active)next=std::min(next,nextActive(now,config));
    for(int i=0;i<2;i++)if(!calendarUrlAt(config,i).empty())next=std::min(next,std::max(now+5,calendarRetry[i].next));
    if(!previousWifi&&wifiRetry.next>now)next=std::max(next,wifiRetry.next);
    if(!hasClock)next=now+300;
    sleepFor(sleepSeconds(now,next,snapshot));
}
void loop(){}
