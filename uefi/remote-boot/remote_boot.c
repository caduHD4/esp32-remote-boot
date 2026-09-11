#include <efi.h>
#include <efilib.h>
#include "load_option.h"

static EFI_GUID global_guid=EFI_GLOBAL_VARIABLE;

static EFI_STATUS read_variable(CHAR16 *name,void **value,UINTN *size) {
    *value=NULL; *size=0;
    EFI_STATUS status=uefi_call_wrapper(RT->GetVariable,5,name,&global_guid,NULL,size,NULL);
    if(status!=EFI_BUFFER_TOO_SMALL||*size>65536) return EFI_ERROR(status)?status:EFI_BAD_BUFFER_SIZE;
    *value=AllocatePool(*size);
    if(!*value) return EFI_OUT_OF_RESOURCES;
    status=uefi_call_wrapper(RT->GetVariable,5,name,&global_guid,NULL,size,*value);
    if(EFI_ERROR(status)) { FreePool(*value); *value=NULL; }
    return status;
}

static int parse_hex(CHAR16 *p,UINTN n) {
    if(n!=4) return -1;
    int value=0;
    for(UINTN i=0;i<4;i++) {
        int c=p[i];
        int digit=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;
        if(digit<0) return -1;
        value=value*16+digit;
    }
    return value;
}

static int argument(CHAR16 *p,UINTN n,CHAR16 *key) {
    UINTN keylen=StrLen(key);
    int result=-1;
    for(UINTN off=0;off<n;) {
        while(off<n&&(p[off]==' '||p[off]=='\t')) off++;
        UINTN end=off;
        while(end<n&&p[end]&&p[end]!=' '&&p[end]!='\t') end++;
        if(end==off) break;
        if(end-off>=keylen&&CompareMem(p+off,key,keylen*sizeof(CHAR16))==0) {
            if(result>=0) return -2;
            result=parse_hex(p+off+keylen,end-off-keylen);
            if(result<0) return -2;
        }
        off=end;
    }
    return result;
}

static EFI_STATUS validate_entry(int id) {
    CHAR16 name[9];
    SPrint(name,sizeof name,L"Boot%04X",id);
    void *raw=NULL;
    UINTN size=0;
    EFI_STATUS status=read_variable(name,&raw,&size);
    if(EFI_ERROR(status)) return status;
    rb_option option;
    if(!rb_parse_option(raw,size,&option)||!(option.attributes&1)||rb_blocked(&option))
        status=EFI_ACCESS_DENIED;
    FreePool(raw);
    return status;
}

static EFI_STATUS schedule_boot(int id) {
    EFI_STATUS status=validate_entry(id);
    if(EFI_ERROR(status)) return status;

    UINT16 next=(UINT16)id;
    status=uefi_call_wrapper(
        RT->SetVariable,
        5,
        L"BootNext",
        &global_guid,
        EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,
        sizeof next,
        &next
    );
    if(EFI_ERROR(status)) return status;

    Print(L"[RemoteBoot] BootNext=Boot%04X; resetting\r\n",id);
    uefi_call_wrapper(RT->ResetSystem,4,EfiResetCold,EFI_SUCCESS,0,NULL);

    uefi_call_wrapper(RT->SetVariable,5,L"BootNext",&global_guid,0,0,NULL);
    return EFI_DEVICE_ERROR;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *system) {
    InitializeLib(image,system);
    EFI_LOADED_IMAGE *loaded=NULL;
    EFI_GUID loaded_guid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_STATUS status=uefi_call_wrapper(BS->HandleProtocol,3,image,&loaded_guid,(void**)&loaded);
    if(EFI_ERROR(status)||!loaded->LoadOptions||loaded->LoadOptionsSize%sizeof(CHAR16)||loaded->LoadOptionsSize>2048)
        return EFI_INVALID_PARAMETER;

    UINTN chars=loaded->LoadOptionsSize/sizeof(CHAR16);
    int target=argument(loaded->LoadOptions,chars,L"boot=");
    int fallback=argument(loaded->LoadOptions,chars,L"fallback=");
    if(target<0||fallback==-2) return EFI_INVALID_PARAMETER;

    status=schedule_boot(target);
    if(EFI_ERROR(status)&&fallback>=0&&fallback!=target) {
        Print(L"[RemoteBoot] Target error: %r; trying fallback\r\n",status);
        status=schedule_boot(fallback);
    }
    Print(L"[RemoteBoot] Returning: %r\r\n",status);
    return status;
}
