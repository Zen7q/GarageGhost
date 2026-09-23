#pragma once
#include <stdint.h>
#include <stddef.h>

// Обмежений інкрементальний декодер RFC 6455: серверні кадри без маски,
// continuation, ping/pong/close; без розширення permessage-deflate.
class WsCodec {
public:
  enum Result {None,Message,Ping,Pong,Close,Error};
private:
  uint8_t* data_;size_t cap_,used_=0;
  uint8_t header_[10]{},hn_=0,need_=2,op_=0;
  bool payload_=false,fin_=false,fragmented_=false,control_=false;
  uint64_t length_=0,pos_=0;
  uint8_t controlData_[125]{};
  bool failed_=false;
  Result finish(){
    payload_=false;hn_=0;need_=2;
    if(control_){
      if(op_==8)return length_==1?Error:Close;
      return op_==9?Ping:Pong;
    }
    if(fin_){fragmented_=false;data_[used_]=0;return Message;}
    fragmented_=true;return None;
  }
public:
  WsCodec(uint8_t* buffer,size_t capacity):data_(buffer),cap_(capacity){}
  void reset(){used_=hn_=0;need_=2;payload_=fragmented_=failed_=false;}
  size_t size()const{return used_;}
  const uint8_t* controlData()const{return controlData_;}
  size_t controlSize()const{return size_t(length_);}
  bool partial()const{return hn_||payload_||fragmented_;}
  Result feed(uint8_t byte){
    if(failed_)return Error;
    if(payload_){
      if(control_)controlData_[pos_]=byte;else data_[used_++]=byte;
      if(++pos_==length_)return finish();
      return None;
    }
    header_[hn_++]=byte;
    if(hn_==2){
      fin_=header_[0]&0x80;op_=header_[0]&15;control_=op_>=8;
      if((header_[0]&0x70)||(header_[1]&0x80)||
         !(op_==0||op_==1||op_==2||op_==8||op_==9||op_==10)||
         (control_&&(!fin_||(header_[1]&127)>125))){failed_=true;return Error;}
      uint8_t n=header_[1]&127;need_=n==126?4:n==127?10:2;
    }
    if(hn_<need_)return None;
    length_=header_[1]&127;
    if(length_>=126){
      uint8_t original=uint8_t(length_);length_=0;
      for(uint8_t i=2;i<need_;i++)length_=(length_<<8)|header_[i];
      if((need_==10&&(header_[2]&0x80))||(original==126&&length_<126)||
         (original==127&&length_<=65535)){failed_=true;return Error;}
    }
    if(!control_){
      if((op_==0&&!fragmented_)||(op_!=0&&fragmented_)){failed_=true;return Error;}
      if(op_!=0)used_=0;
      if(length_>cap_-used_){failed_=true;return Error;}
    }
    pos_=0;payload_=true;
    return length_?None:finish();
  }
};
