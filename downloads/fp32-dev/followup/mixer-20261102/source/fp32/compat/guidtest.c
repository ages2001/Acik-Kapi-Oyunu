#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
static GUID ids[64];
static HANDLE logh;
static void value(const char *name,DWORD v) {char b[128];DWORD i,n=0,w;while(*name)b[n++]=*name++;b[n++]=' ';for(i=0;i<8;i++)b[n++]="0123456789ABCDEF"[(v>>(28-i*4))&15];b[n++]='\r';b[n++]='\n';WriteFile(logh,b,n,&w,0);FlushFileBuffers(logh);}
void WINAPI entry(void) {
    HRESULT (WINAPI *create)(GUID *);HMODULE m;DWORD i,j,k,fail=0;HRESULT hr;
    logh=CreateFileA("C:\\FP32\\GUIDTEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
    m=LoadLibraryA("C:\\FP32\\F32O.DLL");create=m?(void*)GetProcAddress(m,"CoCreateGuid"):0;
    if(!create){value("Load error",GetLastError());ExitProcess(2);}
    value("Begin UUID creation",64);
    for(i=0;i<64;i++) {
        hr=create(&ids[i]);if(hr!=S_OK){value("CoCreateGuid HRESULT",hr);fail++;break;}
        for(j=0;j<i;j++){for(k=0;k<sizeof(GUID)&&((BYTE*)&ids[i])[k]==((BYTE*)&ids[j])[k];k++);if(k==sizeof(GUID))fail++;}
    }
    value("Generated UUIDs",i);value("Failures",fail);CloseHandle(logh);FreeLibrary(m);ExitProcess(fail?1:0);
}
