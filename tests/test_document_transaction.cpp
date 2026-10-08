#include <cassert>
#include <utility>
#include "../firmware/include/document_transaction.hpp"
struct Document {
    int value=0;
    Document()=default;
    Document(const Document&)=delete;
    Document& operator=(const Document&)=delete;
    Document(Document&& other) noexcept:value(other.value){other.value=0;}
    Document& operator=(Document&& other) noexcept{value=other.value;other.value=0;return *this;}
    void clear(){value=0;}
};
static int restored=0;
static void rollback(Document& doc){doc.value=42;++restored;}
int main(){
    Document live;live.value=42;
    {rb::DocumentTransaction<Document> next(live,rollback);assert(next.take());assert(live.value==0&&next.value==42);next.value=7;next.accept();}
    assert(live.value==7&&restored==0);
    {rb::DocumentTransaction<Document> next(live,rollback);assert(next.take());assert(!next.take());next.value=99;}
    assert(live.value==42&&restored==1);
    {rb::DocumentTransaction<Document> next(live,rollback);next.value=5;next.accept();}
    assert(live.value==5&&restored==1);
    {rb::DocumentTransaction<Document> unused(live,rollback);}
    assert(live.value==5&&restored==1);
}
