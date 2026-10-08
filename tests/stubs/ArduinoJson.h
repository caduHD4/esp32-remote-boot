#pragma once
#include <string>
#include <cstring>
class String {
    std::string value;
public:
    String()=default;
    String(const char* s):value(s){}
    String& operator=(const char* s){value=s;return *this;}
    bool reserve(size_t n){value.reserve(n);return true;}
    void concat(const char* s,size_t n){value.append(s,n);}
    size_t length()const{return value.size();}
    const char* c_str()const{return value.c_str();}
};
struct JsonDocument { std::string text="{}"; };
inline size_t measureJson(const JsonDocument& doc){return doc.text.size();}
inline size_t serializeJson(const JsonDocument& doc,char* data,size_t size){std::strncpy(data,doc.text.c_str(),size);return doc.text.size();}
inline bool deserializeJson(JsonDocument& doc,const String& data){doc.text=data.c_str();return doc.text.empty()||doc.text.front()!='{';}
