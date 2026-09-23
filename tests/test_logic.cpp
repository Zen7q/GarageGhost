#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include "../firmware/GarageGhost/CoreLogic.h"
#include "../firmware/GarageGhost/Protocol.h"
#include "../firmware/GarageGhost/SearchPolicy.h"
#include "../firmware/GarageGhost/WsCodec.h"

static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}}while(0)
using namespace logic;
std::vector<uint8_t> frame(const std::string& s,uint8_t opcode=1,bool fin=true){
  std::vector<uint8_t> f{uint8_t((fin?128:0)|opcode)};
  if(s.size()<126)f.push_back(s.size());else{f.push_back(126);f.push_back(s.size()>>8);f.push_back(s.size());}
  f.insert(f.end(),s.begin(),s.end());return f;
}
WsCodec::Result feed(WsCodec& d,const std::vector<uint8_t>& f){
  WsCodec::Result r=WsCodec::None;for(auto b:f){r=d.feed(b);if(r==WsCodec::Error)break;}return r;
}
void buttons(){
  Button b;CHECK(b.update(true,10)==None);CHECK(b.update(false,20)==None);CHECK(b.update(true,25)==None);
  CHECK(b.update(true,49)==None);CHECK(b.update(true,50)==Pressed);
  CHECK(b.update(false,100)==None);CHECK(b.update(false,125)==ReleasedTap);
  CHECK(b.update(true,200)==None);CHECK(b.update(true,225)==Pressed);
  CHECK(b.update(true,574)==None);CHECK(b.update(true,575)==Held);CHECK(b.update(true,800)==None);
  CHECK(b.update(false,900)==None);CHECK(b.update(false,925)==ReleasedHold);
  Button wrap;CHECK(wrap.update(true,0xfffffff0)==None);CHECK(wrap.update(true,10)==Pressed);
  CHECK(wrap.update(true,360)==Held);
  std::puts("PASS debounce, tap/hold, hold-release, millis wrap");
}
void windows(){
  SpeechWindow w;w.open(100);CHECK(!w.expired(5099));CHECK(w.expired(5100));
  w.open(0);CHECK(!w.feed(true,4970));CHECK(!w.expired(5010));CHECK(w.feed(true,5030));
  CHECK(!w.expired(9000));CHECK(!w.feed(true,12000));CHECK(!w.ended(12899));CHECK(w.ended(12900));
  w.open(0);CHECK(!w.feed(true,5000));CHECK(w.expired(5000));
  w.open(0);CHECK(!w.feed(true,4950));CHECK(!w.feed(false,4980));CHECK(w.expired(5000));
  CHECK(elapsed(50,0xfffffff0,66));CHECK(!elapsed(50,0xfffffff0,67));
  std::puts("PASS five-second onset window preserves full utterance; false onset, silence");
}
void commands(){
  CHECK(zoneMask("all")==7);CHECK(zoneMask("desk")==1);CHECK(zoneMask("wall")==2);CHECK(zoneMask("ceiling")==4);
  CHECK(zoneMask("4")==0);CHECK(zoneMask("relay4")==0);CHECK(zoneMask(nullptr)==0);
  CHECK(setLights(0,7,true)==7);CHECK(setLights(7,7,true)==7);CHECK(setLights(7,2,false)==5);CHECK(setLights(5,7,false)==0);
  const char* validCall=R"({"id":"c1","name":"set_lights","args":{"zone":"all","on":true}})";
  JsonDocument doc;CHECK(!deserializeJson(doc,validCall));protocol::LightCall call;CHECK(protocol::validate(doc.as<JsonVariantConst>(),call)==nullptr);
  CHECK(call.mask==7&&call.on&&!strcmp(call.id,"c1"));
  const char* bad[]={
    R"({"id":"c","name":"toggle","args":{"zone":"all","on":true}})",
    R"({"id":"c","name":"set_lights","args":{"zone":"relay4","on":true}})",
    R"({"id":"c","name":"set_lights","args":{"zone":"all","on":"true"}})",
    R"({"id":"c","name":"set_lights","args":{"zone":"all","on":1}})",
    R"({"id":"c","name":"set_lights","args":{"zone":"all","on":null}})",
    R"({"id":"c","name":"set_lights","args":{"zone":4,"on":false}})",
    R"({"id":"c","name":"set_lights","args":{"zone":"desk","on":true,"pin":12}})",
    R"({"name":"set_lights","args":{"zone":"all","on":true}})",
    R"({"id":"","name":"set_lights","args":[]})",R"([])"};
  for(auto json:bad){doc.clear();CHECK(!deserializeJson(doc,json));call={};CHECK(protocol::validate(doc.as<JsonVariantConst>(),call)!=nullptr);}
  std::puts("PASS strict tool schema, boolean types, allowed zones, explicit/idempotent states");
}
void cancellation(){
  Token active{10,2};
  for(auto s:{State::Standby,State::Armed,State::Recording,State::Speaking,State::Followup,State::Fault})CHECK(!mayAct(active,active,s,false));
  CHECK(mayAct(active,active,State::Waiting,false));CHECK(!mayAct(active,active,State::Waiting,true));
  CHECK(!mayAct({9,2},active,State::Waiting,false));CHECK(!mayAct({10,1},active,State::Waiting,false));
  // Імітація мережевого task, що повернув toolCall після блокування; використовує той самий gate.
  std::atomic<uint32_t> generation{10};std::atomic<bool> blocked{false};std::atomic<int> actuations{0};
  std::thread network([&]{blocked=true;std::this_thread::sleep_for(std::chrono::milliseconds(40));
    if(mayAct(active,{generation.load(),2},State::Waiting,false))++actuations;});
  while(!blocked.load())std::this_thread::yield();
  generation.fetch_add(1);network.join();CHECK(actuations.load()==0);
  uint8_t alreadyApplied=setLights(0,1,true);CHECK(alreadyApplied==1); // cancel не відкочує виконане.
  std::puts("PASS cancel gate including delayed network event; no relay rollback");
}
void websocket(){
  uint8_t buffer[1025]{};WsCodec d(buffer,1024);
  CHECK(feed(d,frame("{\"setupComplete\":{}}"))==WsCodec::Message);CHECK(!strcmp((char*)buffer,"{\"setupComplete\":{}}"));
  CHECK(feed(d,frame("{\"toolCall\":",1,false))==WsCodec::None);
  CHECK(feed(d,frame("hi",9))==WsCodec::Ping);CHECK(d.controlSize()==2);CHECK(d.controlData()[0]=='h');
  CHECK(feed(d,frame("{\"functionCalls\":[]}}",0,true))==WsCodec::Message);
  JsonDocument j;CHECK(!deserializeJson(j,buffer,d.size()));CHECK(j["toolCall"]["functionCalls"].is<JsonArray>());
  CHECK(feed(d,frame(std::string(500,'x'),2))==WsCodec::Message);CHECK(d.size()==500);
  CHECK(feed(d,frame("",10))==WsCodec::Pong);
  CHECK(feed(d,frame(std::string("\x03\xe8",2),8))==WsCodec::Close);
  for(auto f:std::vector<std::vector<uint8_t>>{{0x81,0x80},{0xc1,0},{0x09,0},{0x80,0},{0x81,126,0,1},{0x81,127,0x80,0,0,0,0,0,0,0}}){d.reset();CHECK(feed(d,f)==WsCodec::Error);}
  d.reset();CHECK(feed(d,frame(std::string(1025,'x')))==WsCodec::Error);
  d.reset();CHECK(feed(d,frame("a",1,false))==WsCodec::None);CHECK(feed(d,frame("b",1))==WsCodec::Error);
  d.reset();auto truncated=frame("abc");truncated.pop_back();CHECK(feed(d,truncated)==WsCodec::None);CHECK(d.partial());
  // Потокове декодування справжньої форми toolCall → строгий parser → gate.
  d.reset();std::string wire=R"({"toolCall":{"functionCalls":[{"id":"z9","name":"set_lights","args":{"zone":"wall","on":false}}]}})";
  CHECK(feed(d,frame(wire))==WsCodec::Message);j.clear();CHECK(!deserializeJson(j,buffer,d.size()));
  protocol::LightCall c;CHECK(!protocol::validate(j["toolCall"]["functionCalls"][0],c));CHECK(c.mask==2&&!c.on);
  std::puts("PASS WebSocket fragmentation/control/extended lengths/truncation/oversize and JSON pipeline");
}
void microphoneAndWake(){
  CHECK(micSample(0x10000000,1)==4096);CHECK(micSample(int32_t(0xf0000000),1)==-4096);
  CHECK(micSample(0x7fffff00,4)==32767);CHECK(micSample(int32_t(0x80000000),4)==-32768);
  WakeParser p;unsigned wakes=0;for(char c:std::string("noise\nWAKE:1\r\nWAKE:2\nWAKE:1\n"))wakes+=p.feed(c);CHECK(wakes==2);
  for(char c:std::string(100,'x')+"WAKE:1\n")CHECK(!p.feed(c));
  p.reset();for(char c:std::string("WAKE:"))CHECK(!p.feed(c));p.reset();CHECK(!p.feed('1'));CHECK(!p.feed('\n'));
  std::puts("PASS signed mic conversion/clipping, exact bounded ASR UART framing");
}
void search(){
  JsonDocument d;deserializeJson(d,R"({"query":"погода Львів сьогодні"})");CHECK(!searchpolicy::validateQuery(d.as<JsonVariantConst>()));
  for(auto s:{R"({"query":null})",R"({"query":4})",R"({"query":"  "})",R"({"query":"a\nb"})",R"({"query":"test","url":"file:///x"})"}){
    d.clear();deserializeJson(d,s);CHECK(searchpolicy::validateQuery(d.as<JsonVariantConst>()));}
  CHECK(searchpolicy::publicHttpsUrl("https://example.com/news"));
  for(auto s:{"http://example.com","javascript:alert(1)","https://user@example.com/","https://localhost/","https://evil.com/\n"})CHECK(!searchpolicy::publicHttpsUrl(s));
  CHECK(searchpolicy::prefixBytes("друже",3)==2);CHECK(searchpolicy::prefixBytes("друже",4)==4);CHECK(searchpolicy::prefixBytes("abc",10)==3);
  std::puts("PASS search query/URL restrictions, UTF-8 excerpt boundary");
}
void fuzz(){
  struct {uint8_t before[16],buffer[65],after[16];} memory;
  uint32_t random=0x134abcd;
  for(int run=0;run<10000;run++){
    memset(&memory,0xa5,sizeof memory);WsCodec d(memory.buffer,64);
    for(int k=0;k<512;k++){random^=random<<13;random^=random>>17;random^=random<<5;
      auto r=d.feed(random&255);if(r==WsCodec::Error)break;}
    for(auto b:memory.before)CHECK(b==0xa5);for(auto b:memory.after)CHECK(b==0xa5);
  }
  std::puts("PASS 10,000 deterministic malformed frame streams with buffer guards");
}
int main(){buttons();windows();commands();cancellation();websocket();microphoneAndWake();search();fuzz();std::printf("ALL PASS: %u assertions\n",checks);}
