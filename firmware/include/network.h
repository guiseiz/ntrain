#pragma once
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp32s3/rom/miniz.h>
#include <string>

extern const uint8_t caBundle[] asm("_binary_assets_ca_bundle_bin_start");
extern const char googleRoots[] asm("_binary_assets_google_roots_pem_start");
namespace home {
inline std::string encode(const std::string&value){
    std::string out;char b[4];for(unsigned char c:value){if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out+=c;else{snprintf(b,sizeof(b),"%%%02X",c);out+=b;}}return out;
}
class LimitedStream:public Stream {
    size_t cap;uint32_t deadline;
public:
    std::string body;
    LimitedStream(size_t limit,uint32_t until):cap(limit),deadline(until){body.reserve(limit);}
    size_t write(uint8_t c)override{return write(&c,1);}
    size_t write(const uint8_t*p,size_t n)override{
        if(body.size()+n>cap||int32_t(millis()-deadline)>=0)return 0;
        body.append((const char*)p,n);return n;
    }
    int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}
};
inline uint32_t crc32(const uint8_t*p,size_t n){uint32_t crc=0xffffffff;for(size_t i=0;i<n;i++){crc^=p[i];for(int j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&-(crc&1));}return ~crc;}
inline bool inflateGzip(std::string&body,size_t limit){
    if(body.size()<18||uint8_t(body[0])!=31||uint8_t(body[1])!=139||uint8_t(body[2])!=8)return false;
    uint8_t flags=body[3];if(flags&0xe0)return false;size_t p=10;
    if(flags&4){if(p+2>body.size())return false;size_t n=uint8_t(body[p])+(uint8_t(body[p+1])<<8);p+=n+2;}
    for(int flag:{8,16})if(flags&flag){while(p<body.size()&&body[p])p++;p++;}
    if(flags&2)p+=2;if(p+8>body.size())return false;
    auto little=[&](size_t i){return uint32_t(uint8_t(body[i]))|(uint32_t(uint8_t(body[i+1]))<<8)|(uint32_t(uint8_t(body[i+2]))<<16)|(uint32_t(uint8_t(body[i+3]))<<24);};
    uint32_t expected=little(body.size()-4),crc=little(body.size()-8);if(expected>limit)return false;
    std::string output(expected,'\0');size_t got=tinfl_decompress_mem_to_mem(output.data(),output.size(),body.data()+p,body.size()-p-8,0);
    if(got!=expected||crc32((const uint8_t*)output.data(),got)!=crc)return false;body.swap(output);return true;
}
struct Response{int status=-1,retryAfter=0;std::string body;bool ok()const{return status==200;}};
inline Response getHttps(const std::string&url,size_t limit,bool binary=false){
    Response out;
    // The pinned core allocates mbedTLS objects in internal RAM. The binary bundle
    // reads only the needed trust anchor from flash instead of parsing all PEM roots.
    WiFiClientSecure client;
    // Google's cross-signed chain needs ordinary trust-anchor path selection.
    // Use only the GTS roots already trusted by our Mozilla source; the compact
    // bundle remains in use for 511. Both paths verify hostname and validity.
    if(url.rfind("https://calendar.google.com/calendar/ical/",0)==0)client.setCACert(googleRoots);
    else client.setCACertBundle(caBundle);
    client.setHandshakeTimeout(15);client.setTimeout(10);
    HTTPClient http;http.setConnectTimeout(10000);http.setTimeout(10000);http.useHTTP10(true);
    // Explicitly disable redirects: a calendar's bearer URL must stay at its validated origin.
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    const char*headers[]={"Content-Encoding","Retry-After"};http.collectHeaders(headers,2);
    if(!http.begin(client,url.c_str()))return out;
    http.addHeader("Accept-Encoding","gzip");http.addHeader("User-Agent","NTrain-E1001/1.0");
    out.status=http.GET();
    if(out.status<0){
        char message[128]={};int code=client.lastError(message,sizeof(message));
        // Report transport failure without logging URLs, credentials or response bodies.
        Serial.printf("HTTPS_ERROR status=%d tls=%d detail=%s\n",out.status,code<0?code:0,code<0?message:HTTPClient::errorToString(out.status).c_str());
    }
    auto retry=http.header("Retry-After");if(retry.length()){
        bool digits=true;for(unsigned i=0;i<retry.length();i++)if(!isdigit(retry[i]))digits=false;
        if(digits)out.retryAfter=std::max(0,int(retry.toInt()));
        else {tm t{};if(strptime(retry.c_str(),"%a, %d %b %Y %H:%M:%S GMT",&t))out.retryAfter=std::max(int64_t(0),utcEpoch(t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec)-time(nullptr));}
    }
    if(out.ok()){
        if(http.getSize()>int(limit)){out.status=-2;http.end();return out;}
        // Historical calendar exports can be hundreds of KiB. Give them the
        // same bounded transfer window as the bulk transit feed.
        LimitedStream sink(limit,millis()+22000);int written=http.writeToStream(&sink);
        if(written<0){out.status=-3;http.end();return out;}
        out.body.swap(sink.body);
        if(http.header("Content-Encoding")=="gzip"||(out.body.size()>2&&uint8_t(out.body[0])==31&&uint8_t(out.body[1])==139)){
            if(!inflateGzip(out.body,limit)){out.status=-4;out.body.clear();}
        }
        if(!binary&&out.body.size()>=3&&out.body.substr(0,3)=="\xef\xbb\xbf")out.body.erase(0,3);
    }
    http.end();return out;
}
}
