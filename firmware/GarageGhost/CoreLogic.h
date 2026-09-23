#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace logic {
inline bool elapsed(uint32_t now,uint32_t start,uint32_t duration) {
  return uint32_t(now-start)>=duration;
}
enum class State:uint8_t {Standby,Armed,Recording,Waiting,Speaking,Followup,Fault};
struct Token {uint32_t generation=0,turn=0;};
inline bool same(Token a,Token b){return a.generation==b.generation && a.turn==b.turn;}
inline bool mayAct(Token event,Token current,State state,bool buttonDown) {
  return same(event,current)&&state==State::Waiting&&!buttonDown;
}
enum ButtonEvent:uint8_t {None=0,Pressed=1,Held=2,ReleasedTap=4,ReleasedHold=8};
class Button {
  bool raw_=false, stable_=false, held_=false;
  uint32_t changed_=0,down_=0;
public:
  uint8_t update(bool pressed,uint32_t now,uint32_t debounce=25,uint32_t hold=350){
    uint8_t e=None;
    if(pressed!=raw_){raw_=pressed;changed_=now;}
    if(raw_!=stable_&&elapsed(now,changed_,debounce)){
      stable_=raw_;
      if(stable_){down_=now;held_=false;e|=Pressed;}
      else e|=held_?ReleasedHold:ReleasedTap;
    }
    if(stable_&&raw_&&!held_&&elapsed(now,down_,hold)){held_=true;e|=Held;}
    return e;
  }
};
// 5 с обмежують початок, а не тривалість наступної репліки.
class SpeechWindow {
  uint32_t opened_=0, onset_=0,lastVoice_=0;
  bool candidate_=false, started_=false;
public:
  void open(uint32_t now){opened_=now;candidate_=started_=false;}
  bool feed(bool voiced,uint32_t now){
    if(started_){if(voiced)lastVoice_=now;return false;}
    if(voiced){
      if(!candidate_ && !elapsed(now,opened_,5000)){candidate_=true;onset_=now;}
      if(candidate_&&elapsed(now,onset_,60)){started_=true;lastVoice_=now;return true;}
    }else candidate_=false;
    return false;
  }
  bool expired(uint32_t now)const{
    return !started_&&elapsed(now,opened_,5000)&&
      !(candidate_&&!elapsed(now,onset_,100));
  }
  bool ended(uint32_t now,uint32_t silence=900)const{
    return started_&&elapsed(now,lastVoice_,silence);
  }
};
inline uint8_t zoneMask(const char* s){
  if(!s)return 0;
  if(!strcmp(s,"desk"))return 1;
  if(!strcmp(s,"wall"))return 2;
  if(!strcmp(s,"ceiling"))return 4;
  if(!strcmp(s,"all"))return 7;
  return 0;
}
inline uint8_t setLights(uint8_t previous,uint8_t mask,bool on){
  return (on?(previous|mask):(previous&~mask))&7;
}
// 24 значущі біти INMP441 у старших бітах 32-бітного Philips I2S слота.
inline int16_t micSample(int32_t frame,int gain){
  int32_t pcm=frame>>16;
  pcm*=gain;
  if(pcm>32767)pcm=32767;
  if(pcm<-32768)pcm=-32768;
  return int16_t(pcm);
}
// Контракт нашої ASRPRO-конфігурації, а не фабричний протокол.
class WakeParser {
  char line_[16]{};size_t n_=0;bool overflow_=false;
public:
  bool feed(char c){
    if(c=='\r')return false;
    if(c=='\n'){
      line_[n_]=0;
      bool ok=!overflow_&&!strcmp(line_,"WAKE:1");
      n_=0;overflow_=false;return ok;
    }
    if(n_<sizeof(line_)-1)line_[n_++]=c;else overflow_=true;
    return false;
  }
  void reset(){n_=0;overflow_=false;}
};
}
