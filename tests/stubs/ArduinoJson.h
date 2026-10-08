#pragma once
#include <string>
#include <cstring>
#include <cstdint>
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
struct JsonDocument { std::string text="{}"; bool allocationFailed=false;bool overflowed()const{return allocationFailed;} };
inline size_t measureJson(const JsonDocument& doc){return doc.text.size();}
inline size_t serializeJson(const JsonDocument& doc,char* data,size_t size){std::strncpy(data,doc.text.c_str(),size);return doc.text.size();}
inline bool deserializeJson(JsonDocument& doc,const String& data){doc.text=data.c_str();return doc.text.empty()||doc.text.front()!='{';}

inline bool deserializeJson(JsonDocument& doc,const char* data,size_t length) {
    doc.text.assign(data,length);return doc.text.empty()||doc.text.front()!='{';
}
template<class Writer> size_t serializeJson(const JsonDocument& doc,Writer& writer) {
    size_t count=0;
    for(size_t pos=0;pos<doc.text.size();) {
        size_t n=doc.text.size()-pos;if(n>317)n=317;
        count+=writer.write(reinterpret_cast<const uint8_t*>(doc.text.data()+pos),n);pos+=n;
    }
    return count;
}
template<class Reader> bool deserializeJson(JsonDocument& doc,Reader& reader) {
    doc.text.clear();char bytes[127];size_t count;
    while((count=reader.readBytes(bytes,sizeof bytes)))doc.text.append(bytes,count);
    return doc.text.empty()||doc.text.front()!='{';
}
