#pragma once
#include <utility>
namespace rb {
// The single API loop owns live state. Uncommitted changes reload durable state.
template<class Document> class DocumentTransaction:public Document {
    Document& live;
    void (*restore)(Document&);
    bool taken=false;
public:
    DocumentTransaction(Document& current,void (*rollback)(Document&)):live(current),restore(rollback){}
    DocumentTransaction(const DocumentTransaction&)=delete;
    DocumentTransaction& operator=(const DocumentTransaction&)=delete;
    bool take(){
        if(taken)return false;
        using std::swap;swap(static_cast<Document&>(*this),live);taken=true;return true;
    }
    void accept(){
        using std::swap;swap(static_cast<Document&>(*this),live);this->clear();taken=false;
    }
    ~DocumentTransaction(){if(taken){this->clear();restore(live);}}
};
}
