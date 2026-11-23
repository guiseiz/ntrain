#pragma once
#include "train_board.h"
#include "fonts.h"
#include "icons.h"
#include <vector>
#include <memory>

namespace home {
constexpr int WIDTH=800,HEIGHT=480;
struct Canvas {
    uint8_t* pixels;
    explicit Canvas(uint8_t* p):pixels(p){std::memset(p,0,48000);}
    void pixel(int x,int y,bool black=true){if(x>=0&&x<WIDTH&&y>=0&&y<HEIGHT){auto&b=pixels[y*100+x/8];uint8_t mask=0x80>>(x%8);if(black)b|=mask;else b&=~mask;}}
    void rect(int x,int y,int w,int h,bool black=true){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)pixel(i,j,black);}
    void line(int x,int y,int w){rect(x,y,w,1);}
    static std::vector<uint32_t> codes(const std::string&s){
        std::vector<uint32_t> out;
        for(size_t i=0;i<s.size();){uint8_t c=s[i++];uint32_t v=c;int more=0;
            if(c>=0xf0){v=c&7;more=3;}else if(c>=0xe0){v=c&15;more=2;}else if(c>=0xc0){v=c&31;more=1;}
            while(more--&&i<s.size())v=(v<<6)|(uint8_t(s[i++])&63);
            out.push_back(v<32?' ':v);
        }return out;
    }
    static const Glyph& glyph(const Font&f,uint32_t c){for(int i=0;i<f.count;i++)if(f.glyphs[i].code==c)return f.glyphs[i];return f.glyphs[0];}
    int width(const std::string&s,const Font&f){auto chars=codes(s);int w=0;for(auto c:chars)w+=glyph(f,c).advance; if(chars.size()>1)w+=int(chars.size()-1)*f.tracking;return (w+32)/64;}
    void text(int x,int baseline,const std::string&s,const Font&f,int maxWidth=800,bool black=true){
        if(maxWidth<=0)return;
        auto chars=codes(s);int full=width(s,f);int start=x;int cursor=x*64;int ellipsis=width("...",f);
        for(auto code:chars){const auto&g=glyph(f,code);
            x=(cursor+32)/64;
            if((cursor-start*64+g.advance+32)/64>maxWidth-(full>maxWidth?ellipsis:0)){if(full>maxWidth)text(x,baseline,"...",f,ellipsis,black);break;}
            for(int y=0;y<g.height;y++)for(int xx=0;xx<g.width;xx++){int bit=y*g.width+xx;if(f.pixels[g.offset+bit/8]&(0x80>>(bit%8)))pixel(x+g.left+xx,baseline+g.top+y,black);}
            cursor+=g.advance+f.tracking;
        }
    }
    void wrapped(int x,int baseline,const std::string& value,const Font& f,int maxWidth,int lineHeight,int maxLines){
        std::string remaining=value;
        for(int line=0;line<maxLines&&!remaining.empty();line++){
            if(width(remaining,f)<=maxWidth||line==maxLines-1){text(x,baseline+line*lineHeight,remaining,f,maxWidth);break;}
            size_t split=std::string::npos;
            for(size_t p=remaining.find(' ');p!=std::string::npos;p=remaining.find(' ',p+1)){
                if(width(remaining.substr(0,p),f)>maxWidth)break;
                split=p;
            }
            if(split==std::string::npos){text(x,baseline+line*lineHeight,remaining,f,maxWidth);break;}
            text(x,baseline+line*lineHeight,remaining.substr(0,split),f,maxWidth);
            remaining.erase(0,split+1);
        }
    }
    void right(int x,int y,const std::string&s,const Font&f){text(x-width(s,f),y,s,f);}
    void top(int x,int y,const std::string&s,const Font&f,int maxWidth=800,bool black=true){text(x,y+f.capHeight,s,f,maxWidth,black);}
    void bitmap(int x,int y,const uint8_t*data,int w,int h){for(int j=0;j<h;j++)for(int i=0;i<w;i++){int bit=j*w+i;if(data[bit/8]&(0x80>>(bit%8)))pixel(x+i,y+j);}}
};
struct View {
    int64_t now=0; bool wifi=false, quiet=false, setup=false, simulation=false;
    int batteryMv=0; std::string error;
};
inline std::string eventDay(int64_t stamp,int64_t now) {
    time_t s=stamp,n=now;tm a{},b{};localtime_r(&s,&a);localtime_r(&n,&b);
    if(a.tm_year==b.tm_year&&a.tm_yday==b.tm_yday)return "TODAY";
    char v[32];strftime(v,sizeof(v),"%a %b %e",&a);return v;
}
inline void render(Canvas& g,const Snapshot& source,const Config& c,const View& v,const TrainBoard* history=nullptr){
    // Keep the several-kilobyte agenda copy off the constrained ESP32 task stack.
    std::unique_ptr<Snapshot> state(new Snapshot(source));Snapshot& s=*state;
    expire(s,v.now);
    TrainBoard fallback=boardFromSnapshot(source,source.transitUpdated);
    const TrainBoard& trains=history?*history:fallback;
    constexpr int M=32,C=424,CW=344;
    const bool trainsOnly=c.displayMode=="trains";
    // Figma 43:455: direction rows, each with the next two arrivals.
    // Retain the requested Inter text and Geist Mono Light (300) numerals.
    auto header=[&](int x,const std::string& badge){
        const Font& badgeFont=g.width(badge,routeFont)<=48?routeFont:g.width(badge,titleFont)<=48?titleFont:bodyFont;
        g.rect(x,28,55,55);g.top(x+(55-g.width(badge,badgeFont))/2,43+(routeFont.capHeight-badgeFont.capHeight)/2,badge,badgeFont,48,false);
        g.top(x+73,47,c.stopName,titleFont,271);
    };
    header(M,trainsOnly?rowRoute(c,0):rowRoute(c,0)==rowRoute(c,1)?c.route:"M");
    if(trainsOnly)header(C,rowRoute(c,1));
    else{
        g.top(C,32,"Family calendar",titleFont,CW);
        time_t stamp=v.now;tm local{};localtime_r(&stamp,&local);char date[40];
        strftime(date,sizeof(date),"%A, %b %e",&local);
        g.top(C,66,v.now?date:"Your upcoming week",bodyFont,CW);
    }
    g.line(M,99,CW);g.line(C,99,CW);g.rect(400,28,1,401);
    const Font* number=&bigFont;int unit=g.width("min",unitFont);
    for(const Font* font:{&bigFont,&compactFont,&narrowFont,&tinyNumberFont}){
        number=font;bool fits=true;
        for(const auto& row:trains.rows){
            if(row.count&&g.width(std::to_string(row.minutes[0]),*font)>128)fits=false;
            if(row.count>1&&g.width(std::to_string(row.minutes[1]),*font)+6+unit>172)fits=false;
        }
        if(fits)break;
    }
    for(int direction=0;direction<2;direction++){
        const auto& row=trains.rows[direction];int y=trainsOnly?174:direction?289:116;
        const int left=trainsOnly&&direction?C:M,right=left+CW,offset=left-M;
        std::string label=direction?c.outboundDirection:c.direction;
        if(!trainsOnly&&rowRoute(c,0)!=rowRoute(c,1))label=rowRoute(c,direction)+" / "+label;
        g.top(left,y,label,smallFont,CW);
        if(row.count){
            std::string values[2];for(int i=0;i<row.count;i++)values[i]=std::to_string(row.minutes[i]);
            for(int i=0;i<row.count;i++){
                int x=offset+(i?204:M);
                // Bottom-align smaller numerals when two multi-digit values need room.
                g.top(x,y+103-number->capHeight,values[i],*number,CW);
                int after=x+g.width(values[i],*number)+6;
                bool showUnit=i||row.count==1||after+unit<=offset+144;
                if(showUnit)g.top(after,y+86,"min",unitFont,right-after);
                g.top(x,y+117,clockText(row.arrivals[i].at),departureFont,right-x);
                if(i==0&&row.count>1){
                    int arrow=offset+(showUnit?156:172);
                    g.bitmap(arrow,y+55,trainNextIcon,trainNextIconWidth,trainNextIconHeight);
                }
            }
        }else{
            g.top(left,y+50,v.setup?"Connect Wi-Fi to begin":"Waiting for first times",bodyFont,CW);
        }
    }
    if(!trainsOnly){
        g.line(M,272,CW);
        if(s.eventCount==0){
            g.text(C,132,s.calendarUpdated?"THIS WEEK":"YOUR AGENDA",smallFont,CW);
            g.wrapped(C,185,s.calendarUpdated?"A little room in the day.":"Your calendar belongs here.",titleFont,CW,37,2);
            g.wrapped(C,280,s.calendarUpdated?"No upcoming events in the next 7 days.":"Add your private Google Calendar subscription link.",bodyFont,CW,29,3);
        }
        const int visibleEvents=std::min(6,s.eventCount);
        int dayGroups=0;std::string previousDay;
        for(int i=0;i<visibleEvents;i++){
            std::string day=eventDay(s.events[i].start,v.now);
            if(day!=previousDay){dayGroups++;previousDay=day;}
        }
        // Keep the existing type size and roomy rows whenever they fit. Six events
        // on six different days also fit above the sync footer with tighter spacing.
        const bool compactAgenda=visibleEvents*37+dayGroups*26+std::max(0,dayGroups-1)*8>298;
        const int rowHeight=compactAgenda?32:37,dayHeight=compactAgenda?15:26,dayGap=compactAgenda?3:8;
        int agendaY=116;previousDay.clear();
        for(int i=0;i<visibleEvents;i++){
            const auto&e=s.events[i];std::string day=eventDay(e.start,v.now);
            if(day!=previousDay){
                if(i)agendaY+=dayGap;
                g.top(C,agendaY,day,dayFont,CW);agendaY+=dayHeight;previousDay=day;
            }
            g.top(C,agendaY+3,e.allDay?"ALL DAY":clockText(e.start),timeFont,69);
            g.top(C+93,agendaY,e.title,agendaFont,251);
            agendaY+=rowHeight;
        }
        if(s.calendarIncomplete)g.text(C,429,"Waiting for a calendar update",smallFont,CW);
        else if(s.calendarUpdated&&!s.calendarMock&&v.now>=s.calendarValid)
            g.text(C,429,"STALE / LAST SYNC "+clockText(s.calendarUpdated),smallFont,CW);
    }
    g.line(M,443,736);
    std::string middle;int64_t updated=0;
    for(const auto& row:trains.rows)if(row.count)updated=updated?std::min(updated,row.updated):row.updated;
    bool stale=rowStale(trains.rows[0],v.now)||rowStale(trains.rows[1],v.now);
    if(v.setup||s.transitMock)middle="Waiting for Wi-Fi setup";
    else if(updated){
        middle=(stale?"Last received ":"Updated ")+clockText(updated);
        if(v.quiet)middle+=" / overnight pause";
        else if(stale)middle+=" / awaiting new predictions";
    }
    else if(!v.error.empty())middle=v.error;
    else middle="Waiting for first train update";
    g.top(M,457,middle,smallFont,620);
    if(v.batteryMv>0){char b[24];snprintf(b,sizeof(b),"%.2f V",v.batteryMv/1000.0);g.top(696,457,b,smallFont,45);}
    g.bitmap(747,456,batteryIcon,batteryIconWidth,batteryIconHeight);
}
}
