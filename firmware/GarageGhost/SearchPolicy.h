#pragma once
#include <ArduinoJson.h>
#include <string.h>
namespace searchpolicy {
inline const char* validateQuery(JsonVariantConst args){
  if(!args.is<JsonObjectConst>()||args.size()!=1||!args["query"].is<const char*>())return "invalid_search_arguments";
  const char* q=args["query"];size_t n=strlen(q);
  if(n<3||n>256)return "search_query_length";
  bool nonspace=false;
  for(size_t i=0;i<n;i++){if(uint8_t(q[i])<32)return "search_query_control_character";if(q[i]!=' ')nonspace=true;}
  return nonspace?nullptr:"empty_search_query";
}
inline bool publicHttpsUrl(const char* s){
  if(!s||strncmp(s,"https://",8)||strlen(s)>768)return false;
  const char* host=s+8;size_t len=0;bool dot=false;
  while(host[len]&&host[len]!='/'){
    char c=host[len];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'))return false;
    if(c=='.')dot=true;++len;
  }
  for(const char* p=s;*p;p++)if(uint8_t(*p)<33)return false;
  return dot&&len>3&&len<=253;
}
// Обрізання на межі UTF-8, без частини символу наприкінці.
inline size_t prefixBytes(const char* s,size_t maximum){
  size_t n=strlen(s);if(n<=maximum)return n;n=maximum;
  while(n&&((uint8_t(s[n])&0xC0)==0x80))--n;
  return n;
}
}
