#pragma once
#include <Arduino.h>
#include <NetworkClientSecure.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <esp_random.h>
#include <atomic>
#include "Config.h"
#include "WsCodec.h"
#include "TrustAnchors.h"

class WsTransport {
  NetworkClientSecure socket_;
  WsCodec decoder_;
  uint8_t* message_;
  std::atomic<uint32_t>& generation_;
  uint32_t mine_=0,partialSince_=0;
  bool active_=false;
  bool current()const{return generation_.load()==mine_;}
  bool writeAll(const uint8_t* p,size_t n,uint32_t deadlineMs=3000){
    uint32_t start=millis();
    while(n&&current()&&socket_.connected()&&uint32_t(millis()-start)<deadlineMs){
      size_t count=socket_.write(p,n>1024?1024:n);
      if(!count){delay(1);continue;}
      p+=count;n-=count;
    }
    return n==0&&current();
  }
public:
  int httpStatus=0;uint16_t closeCode=0;
  explicit WsTransport(uint8_t* buffer,std::atomic<uint32_t>& generation):
    decoder_(buffer,cfg::WS_MAX),message_(buffer),generation_(generation){}
  void close(){active_=false;socket_.stop();decoder_.reset();partialSince_=0;}
  bool connected(){return active_&&socket_.connected()&&current();}
  bool open(uint32_t generation){
    close();mine_=generation;httpStatus=0;closeCode=0;
    socket_.setCACert(TRUST_ROOTS);
    socket_.setHandshakeTimeout(6);
    socket_.setTimeout(1000); // Stream timeout, мс; TLS socket timeout задає connect(...,6000).
    if(!current()||!socket_.connect("generativelanguage.googleapis.com",443,6000)||!current())return false;
    uint8_t nonce[16];esp_fill_random(nonce,sizeof nonce);
    unsigned char key[32];size_t keyN=0;
    if(mbedtls_base64_encode(key,sizeof key,&keyN,nonce,sizeof nonce))return false;
    key[keyN]=0;
    String challenge=String(reinterpret_cast<char*>(key))+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t digest[20];mbedtls_sha1(reinterpret_cast<const uint8_t*>(challenge.c_str()),challenge.length(),digest);
    unsigned char accept[32];size_t acceptN=0;
    mbedtls_base64_encode(accept,sizeof accept,&acceptN,digest,sizeof digest);accept[acceptN]=0;
    // Заголовок ключа не друкується; query string не містить секретів.
    String request="GET /ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent HTTP/1.1\r\n"
      "Host: generativelanguage.googleapis.com\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: ";
    request+=reinterpret_cast<char*>(key);request+="\r\nx-goog-api-key: ";request+=GEMINI_API_KEY;request+="\r\n\r\n";
    if(!writeAll(reinterpret_cast<const uint8_t*>(request.c_str()),request.length()))return false;
    String line;line.reserve(256);size_t total=0;bool first=true,validAccept=false,upgrade=false,connection=false;
    uint32_t started=millis();
    while(current()&&socket_.connected()&&uint32_t(millis()-started)<6000){
      if(!socket_.available()){delay(2);continue;}
      int c=socket_.read();if(c<0)continue;
      if(++total>8192||line.length()>1024)return false;
      if(c!='\n'){if(c!='\r')line+=char(c);continue;}
      if(first){
        if(!line.startsWith("HTTP/1.1 "))return false;
        httpStatus=line.substring(9,12).toInt();first=false;
      }else if(!line.length()){
        active_=httpStatus==101&&validAccept&&upgrade&&connection;return active_;
      }else{
        int colon=line.indexOf(':');
        if(colon>0){
          String name=line.substring(0,colon);name.toLowerCase();
          String val=line.substring(colon+1);val.trim();
          if(name=="sec-websocket-accept")validAccept=val==reinterpret_cast<char*>(accept);
          val.toLowerCase();
          if(name=="upgrade")upgrade=val=="websocket";
          if(name=="connection")connection=val.indexOf("upgrade")>=0;
          if(name=="sec-websocket-extensions")return false;
        }
      }
      line="";
    }
    return false;
  }
  bool send(const uint8_t* bytes,size_t n,uint8_t opcode=1){
    if(!connected()||n>cfg::WS_MAX||(opcode>=8&&n>125))return false;
    uint8_t h[14];size_t hn=0;h[hn++]=0x80|opcode;
    if(n<126)h[hn++]=0x80|uint8_t(n);
    else if(n<=65535){h[hn++]=0x80|126;h[hn++]=n>>8;h[hn++]=n;}
    else{h[hn++]=0x80|127;for(int i=7;i>=0;i--)h[hn++]=uint64_t(n)>>(i*8);}
    uint8_t mask[4];esp_fill_random(mask,4);memcpy(h+hn,mask,4);hn+=4;
    if(!writeAll(h,hn))return false;
    uint8_t tmp[1024];size_t offset=0;uint32_t start=millis();
    while(offset<n&&current()&&uint32_t(millis()-start)<5000){
      size_t count=n-offset;if(count>sizeof tmp)count=sizeof tmp;
      for(size_t i=0;i<count;i++)tmp[i]=bytes[offset+i]^mask[(offset+i)&3];
      if(!writeAll(tmp,count))return false;offset+=count;
    }
    return offset==n&&current();
  }
  bool send(const String& s){return send(reinterpret_cast<const uint8_t*>(s.c_str()),s.length());}
  // 1 = готове JSON-повідомлення, 0 = немає, -1 = помилка/закриття.
  int poll(){
    if(!connected())return -1;
    if(partialSince_&&uint32_t(millis()-partialSince_)>8000)return -1;
    for(size_t i=0;i<4096&&socket_.available();i++){
      if(!current())return -1;
      int b=socket_.read();if(b<0)break;
      if(!partialSince_)partialSince_=millis();
      auto r=decoder_.feed(uint8_t(b));
      if(!decoder_.partial())partialSince_=0;
      if(r==WsCodec::Message)return 1;
      if(r==WsCodec::Ping&&!send(decoder_.controlData(),decoder_.controlSize(),10))return -1;
      if(r==WsCodec::Close){
        if(decoder_.controlSize()>=2)closeCode=(decoder_.controlData()[0]<<8)|decoder_.controlData()[1];
        return -1;
      }
      if(r==WsCodec::Error)return -1;
    }
    return 0;
  }
  char* message(){return reinterpret_cast<char*>(message_);}
  size_t messageSize(){return decoder_.size();}
};
