#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>

/* Canonical uppercase GUID text, including braces and the final NUL (39). */
int WINAPI fp_StringFromGUID2(REFGUID guid,LPOLESTR text,int capacity)
{
    static const WCHAR hex[]=L"0123456789ABCDEF";
    WCHAR result[39];DWORD fields[11];unsigned widths[11]={8,4,4,2,2,2,2,2,2,2,2};
    unsigned i,j,n=0;
    if(!guid||!text||capacity<39)return 0;
    fields[0]=guid->Data1;fields[1]=guid->Data2;fields[2]=guid->Data3;
    for(i=0;i<8;i++)fields[i+3]=guid->Data4[i];
    result[n++]='{';
    for(i=0;i<11;i++){
        if(i==1||i==2||i==3||i==5)result[n++]='-';
        for(j=widths[i];j;j--)result[n++]=hex[(fields[i]>>((j-1)*4))&15];
    }
    result[n++]='}';result[n++]=0;
    for(i=0;i<n;i++)text[i]=result[i];
    return n;
}

/* Use the OS RPC UUID generator, including its native concurrency handling. */
HRESULT WINAPI fp_CoCreateGuid(GUID *guid)
{
    typedef LONG (WINAPI *CreateUuid)(GUID *);
    HMODULE module;
    CreateUuid create;
    LONG status;
    DWORD error;
    if (!guid) return E_INVALIDARG;
    module = LoadLibraryA("RPCRT4.DLL");
    if (!module) return HRESULT_FROM_WIN32(GetLastError());
    create = (CreateUuid)GetProcAddress(module, "UuidCreate");
    if (!create) { error = GetLastError(); FreeLibrary(module); return HRESULT_FROM_WIN32(error); }
    status = create(guid);
    FreeLibrary(module);
    /* RPC_S_UUID_LOCAL_ONLY is a generated UUID with machine-local uniqueness. */
    if (status == RPC_S_OK || status == RPC_S_UUID_LOCAL_ONLY) return S_OK;
    return HRESULT_FROM_WIN32(status);
}
