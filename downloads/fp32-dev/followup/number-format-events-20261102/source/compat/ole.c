#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Initial STA apartment/lifetime subset. Object creation, marshaling and
   clipboard entry points remain explicit fail-fast diagnostics. */
typedef struct {ULONG refs;ULONG ole_refs;} Apartment;
static volatile LONG tls_slot=-1;
static DWORD slot(void){LONG current=tls_slot,old;DWORD fresh;if(current!=-1)return (DWORD)current;fresh=TlsAlloc();if(fresh==TLS_OUT_OF_INDEXES)return fresh;__asm__ volatile("lock cmpxchgl %2,%1":"=a"(old),"+m"(tls_slot):"r"((LONG)fresh),"0"(-1):"memory","cc");if(old!=-1){TlsFree(fresh);return (DWORD)old;}return fresh;}
HRESULT WINAPI fp_CoInitialize(LPVOID reserved){Apartment*a;DWORD index;if(reserved)return E_INVALIDARG;index=slot();if(index==TLS_OUT_OF_INDEXES)return E_OUTOFMEMORY;a=TlsGetValue(index);if(a){a->refs++;return S_FALSE;}a=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*a));if(!a)return E_OUTOFMEMORY;a->refs=1;if(!TlsSetValue(index,a)){HeapFree(GetProcessHeap(),0,a);return E_OUTOFMEMORY;}return S_OK;}
void WINAPI fp_CoUninitialize(void){Apartment*a;if(tls_slot==-1)return;a=TlsGetValue(tls_slot);if(!a)return;if(--a->refs==0){TlsSetValue(tls_slot,0);HeapFree(GetProcessHeap(),0,a);}}
HRESULT WINAPI fp_OleInitialize(LPVOID reserved){HRESULT h=fp_CoInitialize(reserved);Apartment*a;ULONG old;if(FAILED(h))return h;a=TlsGetValue(tls_slot);old=a->ole_refs++;return old?S_FALSE:S_OK;}
void WINAPI fp_OleUninitialize(void){Apartment*a;if(tls_slot==-1)return;a=TlsGetValue(tls_slot);if(a&&a->ole_refs){a->ole_refs--;fp_CoUninitialize();}}
LPVOID WINAPI fp_CoTaskMemAlloc(SIZE_T n){return HeapAlloc(GetProcessHeap(),0,n?n:1);}
void WINAPI fp_CoTaskMemFree(LPVOID p){if(p)HeapFree(GetProcessHeap(),0,p);}
#include <objbase.h>
static void guid_text(REFGUID g,char*out){const char*h="0123456789ABCDEF";BYTE b[16];int i,k=0;DWORD d=g->Data1;b[0]=d>>24;b[1]=d>>16;b[2]=d>>8;b[3]=d;b[4]=g->Data2>>8;b[5]=g->Data2;b[6]=g->Data3>>8;b[7]=g->Data3;for(i=0;i<8;i++)b[8+i]=g->Data4[i];out[k++]='{';for(i=0;i<16;i++){if(i==4||i==6||i==8||i==10)out[k++]='-';out[k++]=h[b[i]>>4];out[k++]=h[b[i]&15];}out[k++]='}';out[k]=0;}
HRESULT WINAPI fp_CoCreateInstance(REFCLSID cls,LPUNKNOWN outer,DWORD context,REFIID iid,LPVOID*out){
 static const IID factory_iid={1,0,0,{0xc0,0,0,0,0,0,0,0x46}};
 char key[80],guid[40],path[1024],expanded[1024];const char*prefix="CLSID\\";const char*suffix="\\InprocServer32";HKEY hk;DWORD size=sizeof(path),type;LONG err;int i=0,j;HMODULE mod;IClassFactory*factory;HRESULT hr;typedef HRESULT(WINAPI *GetFactory)(REFCLSID,REFIID,LPVOID*);GetFactory getfactory;
 if(!out)return E_POINTER;*out=0;if(!cls||!iid)return E_INVALIDARG;
 if(tls_slot==-1||!TlsGetValue(tls_slot))return CO_E_NOTINITIALIZED;
 if(!(context&CLSCTX_INPROC_SERVER))return E_NOTIMPL;
 guid_text(cls,guid);while(prefix[i]){key[i]=prefix[i];i++;}for(j=0;guid[j];j++)key[i++]=guid[j];for(j=0;suffix[j];j++)key[i++]=suffix[j];key[i]=0;
 err=RegOpenKeyExA(HKEY_CLASSES_ROOT,key,0,KEY_READ,&hk);if(err==ERROR_FILE_NOT_FOUND||err==ERROR_PATH_NOT_FOUND)return REGDB_E_CLASSNOTREG;if(err)return HRESULT_FROM_WIN32(err);
 err=RegQueryValueExA(hk,0,0,&type,(BYTE*)path,&size);RegCloseKey(hk);if(err)return HRESULT_FROM_WIN32(err);if(!size||size>=sizeof(path)||(type!=REG_SZ&&type!=REG_EXPAND_SZ))return REGDB_E_CLASSNOTREG;path[size]=0;
 if(type==REG_EXPAND_SZ){DWORD need=ExpandEnvironmentStringsA(path,expanded,sizeof(expanded));if(!need||need>sizeof(expanded))return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);mod=LoadLibraryA(expanded);}else mod=LoadLibraryA(path);
 if(!mod)return HRESULT_FROM_WIN32(GetLastError());getfactory=(GetFactory)GetProcAddress(mod,"DllGetClassObject");if(!getfactory){FreeLibrary(mod);return CLASS_E_CLASSNOTAVAILABLE;}
 hr=getfactory(cls,&factory_iid,(void**)&factory);if(FAILED(hr)){FreeLibrary(mod);return hr;}
 hr=factory->lpVtbl->CreateInstance(factory,outer,iid,out);factory->lpVtbl->Release(factory);
 if(FAILED(hr))FreeLibrary(mod);/* Successful server remains resident while its objects may exist. */
 return hr;
}
