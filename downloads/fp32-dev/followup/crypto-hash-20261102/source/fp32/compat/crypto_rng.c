/* NT 3.1 CryptoAPI RNG/hash bridge to the supplied, import-adapted NT4 RSA CSP.
   The CSP's algorithms and entropy collection are unchanged. This adapter
   supports ephemeral PROV_RSA_FULL contexts only, not persisted key stores. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
static CRITICAL_SECTION provider_lock;
static HMODULE provider_module;
static BOOL (WINAPI *cp_acquire)(HCRYPTPROV*,LPSTR,DWORD,void*);
static BOOL (WINAPI *cp_random)(HCRYPTPROV,DWORD,BYTE*);
static BOOL (WINAPI *cp_release)(HCRYPTPROV,DWORD);
static BOOL (WINAPI *cp_create_hash)(HCRYPTPROV,ALG_ID,HCRYPTKEY,DWORD,HCRYPTHASH*);
static BOOL (WINAPI *cp_hash_data)(HCRYPTPROV,HCRYPTHASH,const BYTE*,DWORD,DWORD);
static BOOL (WINAPI *cp_get_hash)(HCRYPTPROV,HCRYPTHASH,DWORD,BYTE*,DWORD*,DWORD);
static BOOL (WINAPI *cp_set_hash)(HCRYPTPROV,HCRYPTHASH,DWORD,const BYTE*,DWORD);
static BOOL (WINAPI *cp_destroy_hash)(HCRYPTPROV,HCRYPTHASH);
typedef struct HashHandle {struct HashHandle*next;HCRYPTPROV owner;HCRYPTHASH native;}HashHandle;
static HashHandle *hashes;
/* Public hash handles belong to the bridge; never dereference an unvalidated
   caller handle or assume a CSP's private hash handle encodes its provider. */
static HashHandle *find_hash(HCRYPTHASH handle)
{HashHandle*h;for(h=hashes;h;h=h->next)if((HCRYPTHASH)h==handle)return h;return 0;}
static void remove_hash(HashHandle*h)
{HashHandle**p;for(p=&hashes;*p;p=&(*p)->next)if(*p==h){*p=h->next;HeapFree(GetProcessHeap(),0,h);return;}}
BOOL WINAPI fp_crypto_main(DWORD reason)
{
    if(reason==DLL_PROCESS_ATTACH)InitializeCriticalSection(&provider_lock);
    else if(reason==DLL_PROCESS_DETACH)DeleteCriticalSection(&provider_lock);
    return TRUE;
}
static void lock(void){EnterCriticalSection(&provider_lock);}
static void unlock(void){LeaveCriticalSection(&provider_lock);}
static BOOL load_provider(void)
{
    char path[MAX_PATH];DWORD n,i,error;HMODULE module;
    if(provider_module)return TRUE;
    n=GetModuleFileNameA(GetModuleHandleA("F32A.DLL"),path,sizeof(path));
    if(!n||n>=sizeof(path)){SetLastError(ERROR_BAD_PATHNAME);return FALSE;}
    for(i=n;i&&path[i-1]!='\\';i--);
    if(!i||i+10>sizeof(path)){SetLastError(ERROR_BAD_PATHNAME);return FALSE;}
    {const char *name="FPRSA.DLL";while(*name)path[i++]=*name++;path[i]=0;}
    module=LoadLibraryA(path);if(!module)return FALSE;
    cp_acquire=(void*)GetProcAddress(module,"CPAcquireContext");
    cp_random=(void*)GetProcAddress(module,"CPGenRandom");
    cp_release=(void*)GetProcAddress(module,"CPReleaseContext");
    cp_create_hash=(void*)GetProcAddress(module,"CPCreateHash");
    cp_hash_data=(void*)GetProcAddress(module,"CPHashData");
    cp_get_hash=(void*)GetProcAddress(module,"CPGetHashParam");
    cp_set_hash=(void*)GetProcAddress(module,"CPSetHashParam");
    cp_destroy_hash=(void*)GetProcAddress(module,"CPDestroyHash");
    if(!cp_acquire||!cp_random||!cp_release||!cp_create_hash||!cp_hash_data||!cp_get_hash||!cp_set_hash||!cp_destroy_hash){error=ERROR_PROC_NOT_FOUND;FreeLibrary(module);SetLastError(error);return FALSE;}
    provider_module=module;return TRUE;
}
BOOL WINAPI fp_CryptAcquireContextA(HCRYPTPROV *result,LPCSTR container,LPCSTR provider,DWORD type,DWORD flags)
{
    BOOL ok;DWORD table[7]={1,0,0,PROV_RSA_FULL,0,0,0};
    if(!result){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    *result=0;
    if(type!=PROV_RSA_FULL){SetLastError(NTE_BAD_PROV_TYPE);return FALSE;}
    if(provider&&lstrcmpA(provider,"Microsoft Base Cryptographic Provider v1.0")){SetLastError(NTE_PROV_TYPE_NOT_DEF);return FALSE;}
    if(container){SetLastError(NTE_BAD_KEYSET);return FALSE;}
    if(flags!=CRYPT_VERIFYCONTEXT){SetLastError(NTE_BAD_FLAGS);return FALSE;}
    lock();ok=load_provider();if(ok)ok=cp_acquire(result,0,flags,table);unlock();return ok;
}
BOOL WINAPI fp_CryptAcquireContextW(HCRYPTPROV *result,LPCWSTR container,LPCWSTR provider,DWORD type,DWORD flags)
{
    if(provider&&lstrcmpW(provider,L"Microsoft Base Cryptographic Provider v1.0")){
        if(result)*result=0;SetLastError(NTE_PROV_TYPE_NOT_DEF);return FALSE;
    }
    return fp_CryptAcquireContextA(result,container?"unsupported":0,0,type,flags);
}
BOOL WINAPI fp_CryptGenRandom(HCRYPTPROV context,DWORD length,BYTE *buffer)
{
    BOOL ok;
    if(length&&!buffer){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();if(!provider_module){unlock();SetLastError(NTE_BAD_UID);return FALSE;}
    ok=cp_random(context,length,buffer);unlock();return ok;
}
BOOL WINAPI fp_CryptReleaseContext(HCRYPTPROV context,DWORD flags)
{
    BOOL ok;HashHandle*h,*next;DWORD error;
    if(flags){SetLastError(NTE_BAD_FLAGS);return FALSE;}
    lock();if(!provider_module){unlock();SetLastError(NTE_BAD_UID);return FALSE;}
    /* CSP context cleanup is not a replacement for CPDestroyHash. Destroy
       bridge-owned native children while their provider is still alive. */
    for(h=hashes;h;h=next){
        next=h->next;if(h->owner!=context)continue;
        if(!cp_destroy_hash(h->owner,h->native)){error=GetLastError();unlock();SetLastError(error);return FALSE;}
        remove_hash(h);
    }
    ok=cp_release(context,flags);
    error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptCreateHash(HCRYPTPROV context,ALG_ID algorithm,HCRYPTKEY key,DWORD flags,HCRYPTHASH *result)
{
    HashHandle*h;BOOL ok;DWORD error;
    if(!result){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}*result=0;
    /* Key-management imports remain separate missing work. No keyed-hash
       support is claimed by this unkeyed hash addition. */
    if(key){SetLastError(NTE_BAD_KEY);return FALSE;}
    lock();if(!provider_module){unlock();SetLastError(NTE_BAD_UID);return FALSE;}
    h=HeapAlloc(GetProcessHeap(),0,sizeof(*h));if(!h){unlock();SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    ok=cp_create_hash(context,algorithm,0,flags,&h->native);error=GetLastError();
    if(ok){h->owner=context;h->next=hashes;hashes=h;*result=(HCRYPTHASH)h;}
    else HeapFree(GetProcessHeap(),0,h);
    unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptHashData(HCRYPTHASH handle,const BYTE *data,DWORD bytes,DWORD flags)
{
    HashHandle*h;BOOL ok;DWORD error;
    if(bytes&&!data){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();h=find_hash(handle);if(!h){unlock();SetLastError(NTE_BAD_HASH);return FALSE;}
    ok=cp_hash_data(h->owner,h->native,data,bytes,flags);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptGetHashParam(HCRYPTHASH handle,DWORD parameter,BYTE *data,DWORD *bytes,DWORD flags)
{
    HashHandle*h;BOOL ok;DWORD error;
    if(!bytes){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();h=find_hash(handle);if(!h){unlock();SetLastError(NTE_BAD_HASH);return FALSE;}
    ok=cp_get_hash(h->owner,h->native,parameter,data,bytes,flags);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptSetHashParam(HCRYPTHASH handle,DWORD parameter,const BYTE *data,DWORD flags)
{
    HashHandle*h;BOOL ok;DWORD error;
    if(!data){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();h=find_hash(handle);if(!h){unlock();SetLastError(NTE_BAD_HASH);return FALSE;}
    ok=cp_set_hash(h->owner,h->native,parameter,data,flags);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptDestroyHash(HCRYPTHASH handle)
{
    HashHandle*h;BOOL ok;DWORD error;
    lock();h=find_hash(handle);if(!h){unlock();SetLastError(NTE_BAD_HASH);return FALSE;}
    ok=cp_destroy_hash(h->owner,h->native);error=GetLastError();if(ok)remove_hash(h);unlock();if(!ok)SetLastError(error);return ok;
}
