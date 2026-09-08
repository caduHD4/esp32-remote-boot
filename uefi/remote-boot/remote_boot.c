#include <efi.h>
#include <efilib.h>
#include "load_option.h"

static EFI_GUID global_guid=EFI_GLOBAL_VARIABLE;
static EFI_GUID loaded_guid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID fs_guid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID marker_guid={0x80529051,0x1ac3,0x4e55,{0x91,0x21,0x90,0x72,0x72,0x31,0x24,0x89}};

static EFI_STATUS read_variable(CHAR16 *name,void **value,UINTN *size) {
    *value=NULL; *size=0;
    EFI_STATUS s=uefi_call_wrapper(RT->GetVariable,5,name,&global_guid,NULL,size,NULL);
    if(s!=EFI_BUFFER_TOO_SMALL||*size>65536) return EFI_ERROR(s)?s:EFI_BAD_BUFFER_SIZE;
    *value=AllocatePool(*size); if(!*value) return EFI_OUT_OF_RESOURCES;
    s=uefi_call_wrapper(RT->GetVariable,5,name,&global_guid,NULL,size,*value);
    if(EFI_ERROR(s)) { FreePool(*value); *value=NULL; }
    return s;
}
static int parse_hex(CHAR16 *p,UINTN n) {
    if(n!=4) return -1;
    int value=0;
    for(UINTN i=0;i<4;i++) { int c=p[i]; int d=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1; if(d<0) return -1; value=value*16+d; }
    return value;
}
static int argument(CHAR16 *p,UINTN n,CHAR16 *key) {
    UINTN keylen=StrLen(key); int result=-1;
    for(UINTN off=0;off<n;) {
        while(off<n&&(p[off]==' '||p[off]=='\t')) off++;
        UINTN end=off; while(end<n&&p[end]&&p[end]!=' '&&p[end]!='\t') end++;
        if(end==off) break;
        if(end-off>=keylen&&CompareMem(p+off,key,keylen*2)==0) {
            if(result>=0) return -2;
            result=parse_hex(p+off+keylen,end-off-keylen); if(result<0) return -2;
        }
        off=end;
    }
    return result;
}
/* Expand HD() short forms by partition signature, never by the first volume. */
static EFI_DEVICE_PATH *expand_path(const rb_option *o) {
    EFI_DEVICE_PATH *path=(EFI_DEVICE_PATH*)o->path;
    if(o->path[0]!=4||o->path[1]!=1) return DuplicateDevicePath(path);
    if(rb_u16(o->path+2)!=42) return NULL;
    EFI_HANDLE *handles=NULL; UINTN count=0;
    if(EFI_ERROR(uefi_call_wrapper(BS->LocateHandleBuffer,5,ByProtocol,&fs_guid,NULL,&count,&handles))) return NULL;
    EFI_DEVICE_PATH *result=NULL; UINTN matches=0;
    for(UINTN i=0;i<count;i++) {
        EFI_DEVICE_PATH *full=DevicePathFromHandle(handles[i]);
        if(!full) continue;
        EFI_DEVICE_PATH *node=full;
        for(UINTN guard=0;guard<256&&!IsDevicePathEnd(node);guard++) {
            UINTN len=DevicePathNodeLength(node); if(len<4) break;
            uint8_t *b=(uint8_t*)node;
            if(b[0]==4&&b[1]==1&&len==42 && b[40]==o->path[40]&&b[41]==o->path[41] && rb_u32(b+4)==rb_u32(o->path+4) && CompareMem(b+24,(void*)(o->path+24),16)==0) {
                if(result) { FreePool(result); result=NULL; }
                ++matches; if(matches==1) result=AppendDevicePath(full,(EFI_DEVICE_PATH*)(o->path+42));
                break;
            }
            node=NextDevicePathNode(node);
        }
    }
    FreePool(handles);
    return matches==1?result:NULL;
}
static EFI_STATUS boot_entry(EFI_HANDLE parent,int id) {
    CHAR16 name[9]; SPrint(name,sizeof name,L"Boot%04X",id);
    void *raw=NULL; UINTN size=0; EFI_STATUS status=read_variable(name,&raw,&size);
    if(EFI_ERROR(status)) return status;
    rb_option o;
    if(!rb_parse_option(raw,size,&o)||!(o.attributes&1)||rb_blocked(&o)) { FreePool(raw); return EFI_ACCESS_DENIED; }
    EFI_DEVICE_PATH *path=expand_path(&o);
    if(!path) { FreePool(raw); return EFI_NOT_FOUND; }
    EFI_HANDLE child=NULL;
    status=uefi_call_wrapper(BS->LoadImage,6,FALSE,parent,path,NULL,0,&child);
    if(!EFI_ERROR(status)) {
        EFI_LOADED_IMAGE *loaded=NULL;
        status=uefi_call_wrapper(BS->HandleProtocol,3,child,&loaded_guid,(void**)&loaded);
        if(!EFI_ERROR(status)) {
            loaded->LoadOptions=(void*)o.optional; loaded->LoadOptionsSize=(UINT32)o.optional_size;
            // BootCurrent describes the OS entry while the child is running.
            void *old=NULL; UINTN old_size=0; EFI_STATUS old_status=read_variable(L"BootCurrent",&old,&old_size);
            UINT16 current=(UINT16)id;
            EFI_STATUS current_status=uefi_call_wrapper(RT->SetVariable,5,L"BootCurrent",&global_guid,EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,2,&current);
            Print(L"[RemoteBoot] %s\r\n",name);
            status=uefi_call_wrapper(BS->StartImage,3,child,NULL,NULL);
            if(!EFI_ERROR(current_status)) {
                if(!EFI_ERROR(old_status)) uefi_call_wrapper(RT->SetVariable,5,L"BootCurrent",&global_guid,EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,old_size,old);
                else uefi_call_wrapper(RT->SetVariable,5,L"BootCurrent",&global_guid,0,0,NULL);
            }
            if(old) FreePool(old);
            loaded->LoadOptions=NULL; loaded->LoadOptionsSize=0;
        }
        uefi_call_wrapper(BS->UnloadImage,1,child);
    }
    FreePool(path); FreePool(raw); return status;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *system) {
    InitializeLib(image,system);
    void *marker=NULL;
    if(!EFI_ERROR(uefi_call_wrapper(BS->LocateProtocol,3,&marker_guid,NULL,&marker))) return EFI_ACCESS_DENIED;
    EFI_LOADED_IMAGE *loaded=NULL;
    EFI_STATUS status=uefi_call_wrapper(BS->HandleProtocol,3,image,&loaded_guid,(void**)&loaded);
    if(EFI_ERROR(status)||!loaded->LoadOptions||loaded->LoadOptionsSize%2||loaded->LoadOptionsSize>2048) return EFI_INVALID_PARAMETER;
    UINTN chars=loaded->LoadOptionsSize/2;
    int target=argument(loaded->LoadOptions,chars,L"boot="),fallback=argument(loaded->LoadOptions,chars,L"fallback=");
    if(target<0||fallback==-2) return EFI_INVALID_PARAMETER;
    EFI_HANDLE marker_handle=NULL; UINT8 marker_data=1;
    status=uefi_call_wrapper(BS->InstallProtocolInterface,4,&marker_handle,&marker_guid,EFI_NATIVE_INTERFACE,&marker_data);
    if(EFI_ERROR(status)) return status;
    status=boot_entry(image,target);
    if(EFI_ERROR(status)&&fallback>=0&&fallback!=target) { Print(L"[RemoteBoot] Target error: %r; fallback\r\n",status); status=boot_entry(image,fallback); }
    uefi_call_wrapper(BS->UninstallProtocolInterface,3,marker_handle,&marker_guid,&marker_data);
    Print(L"[RemoteBoot] Returning: %r\r\n",status); return status;
}
