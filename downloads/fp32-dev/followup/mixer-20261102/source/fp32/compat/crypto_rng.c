/* NT 3.1 CryptoAPI RNG/hash bridge to the supplied, import-adapted NT4 RSA CSP.
   The CSP's algorithms and entropy collection are unchanged. This adapter
   exposes the original PROV_RSA_FULL provider and its registry key containers. */
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
static BOOL (WINAPI *cp_import_key)(HCRYPTPROV,const BYTE*,DWORD,HCRYPTKEY,DWORD,HCRYPTKEY*);
static BOOL (WINAPI *cp_export_key)(HCRYPTPROV,HCRYPTKEY,HCRYPTKEY,DWORD,DWORD,BYTE*,DWORD*);
static BOOL (WINAPI *cp_get_user_key)(HCRYPTPROV,DWORD,HCRYPTKEY*);
static BOOL (WINAPI *cp_destroy_key)(HCRYPTPROV,HCRYPTKEY);
static BOOL (WINAPI *cp_set_key)(HCRYPTPROV,HCRYPTKEY,DWORD,const BYTE*,DWORD);
static BOOL (WINAPI *cp_get_provider)(HCRYPTPROV,DWORD,BYTE*,DWORD*,DWORD);
static BOOL (WINAPI *cp_encrypt)(HCRYPTPROV,HCRYPTKEY,HCRYPTHASH,BOOL,DWORD,BYTE*,DWORD*,DWORD);
static BOOL (WINAPI *cp_decrypt)(HCRYPTPROV,HCRYPTKEY,HCRYPTHASH,BOOL,DWORD,BYTE*,DWORD*);
static BOOL (WINAPI *cp_sign)(HCRYPTPROV,HCRYPTHASH,DWORD,LPCWSTR,DWORD,BYTE*,DWORD*);
typedef struct HashHandle {struct HashHandle*next;HCRYPTPROV owner;HCRYPTHASH native;}HashHandle;
static HashHandle *hashes,*keys;
/* Public hash handles belong to the bridge; never dereference an unvalidated
   caller handle or assume a CSP's private hash handle encodes its provider. */
static HashHandle *find_object(HashHandle*head,ULONG_PTR handle)
{HashHandle*h;for(h=head;h;h=h->next)if((ULONG_PTR)h==handle)return h;return 0;}
static void remove_object(HashHandle**head,HashHandle*h)
{HashHandle**p;for(p=head;*p;p=&(*p)->next)if(*p==h){*p=h->next;HeapFree(GetProcessHeap(),0,h);return;}}
#define find_hash(h) find_object(hashes,h)
#define remove_hash(h) remove_object(&hashes,h)
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
    cp_import_key=(void*)GetProcAddress(module,"CPImportKey");
    cp_export_key=(void*)GetProcAddress(module,"CPExportKey");
    cp_get_user_key=(void*)GetProcAddress(module,"CPGetUserKey");
    cp_destroy_key=(void*)GetProcAddress(module,"CPDestroyKey");
    cp_set_key=(void*)GetProcAddress(module,"CPSetKeyParam");
    cp_get_provider=(void*)GetProcAddress(module,"CPGetProvParam");
    cp_encrypt=(void*)GetProcAddress(module,"CPEncrypt");
    cp_decrypt=(void*)GetProcAddress(module,"CPDecrypt");
    cp_sign=(void*)GetProcAddress(module,"CPSignHash");
    if(!cp_acquire||!cp_random||!cp_release||!cp_create_hash||!cp_hash_data||!cp_get_hash||!cp_set_hash||!cp_destroy_hash||
       !cp_import_key||!cp_export_key||!cp_get_user_key||!cp_destroy_key||!cp_set_key||!cp_get_provider||!cp_encrypt||!cp_decrypt||!cp_sign){error=ERROR_PROC_NOT_FOUND;FreeLibrary(module);SetLastError(error);return FALSE;}
    provider_module=module;return TRUE;
}
BOOL WINAPI fp_CryptAcquireContextA(HCRYPTPROV *result,LPCSTR container,LPCSTR provider,DWORD type,DWORD flags)
{
    BOOL ok;DWORD table[7]={1,0,0,PROV_RSA_FULL,0,0,0};
    if(!result){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    *result=0;
    if(type!=PROV_RSA_FULL){SetLastError(NTE_BAD_PROV_TYPE);return FALSE;}
    if(provider&&lstrcmpA(provider,"Microsoft Base Cryptographic Provider v1.0")){SetLastError(NTE_PROV_TYPE_NOT_DEF);return FALSE;}
    lock();ok=load_provider();if(ok)ok=cp_acquire(result,(LPSTR)container,flags,table);unlock();
    if(ok&&(flags&CRYPT_DELETEKEYSET))*result=0;
    return ok;
}
BOOL WINAPI fp_CryptAcquireContextW(HCRYPTPROV *result,LPCWSTR container,LPCWSTR provider,DWORD type,DWORD flags)
{
    char *name=0;int bytes;BOOL used=FALSE,ok;DWORD error;
    if(provider&&lstrcmpW(provider,L"Microsoft Base Cryptographic Provider v1.0")){
        if(result)*result=0;SetLastError(NTE_PROV_TYPE_NOT_DEF);return FALSE;
    }
    if(container){
        bytes=WideCharToMultiByte(CP_ACP,0,container,-1,0,0,0,&used);
        if(!bytes)return FALSE;
        name=HeapAlloc(GetProcessHeap(),0,bytes);if(!name){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
        if(!WideCharToMultiByte(CP_ACP,0,container,-1,name,bytes,0,&used)||used){
            HeapFree(GetProcessHeap(),0,name);SetLastError(NTE_BAD_KEYSET);return FALSE;
        }
    }
    ok=fp_CryptAcquireContextA(result,name,0,type,flags);error=GetLastError();
    if(name)HeapFree(GetProcessHeap(),0,name);if(!ok)SetLastError(error);return ok;
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
    for(h=keys;h;h=next){
        next=h->next;if(h->owner!=context)continue;
        if(!cp_destroy_key(h->owner,h->native)){error=GetLastError();unlock();SetLastError(error);return FALSE;}
        remove_object(&keys,h);
    }
    ok=cp_release(context,flags);
    error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptCreateHash(HCRYPTPROV context,ALG_ID algorithm,HCRYPTKEY key,DWORD flags,HCRYPTHASH *result)
{
    HashHandle*h,*k=0;BOOL ok;DWORD error;
    if(!result){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}*result=0;
    lock();if(!provider_module){unlock();SetLastError(NTE_BAD_UID);return FALSE;}
    if(key){k=find_object(keys,key);if(!k||k->owner!=context){unlock();SetLastError(NTE_BAD_KEY);return FALSE;}}
    h=HeapAlloc(GetProcessHeap(),0,sizeof(*h));if(!h){unlock();SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    ok=cp_create_hash(context,algorithm,k?k->native:0,flags,&h->native);error=GetLastError();
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

/* Key handles use the same ownership records, in a separate typed list. */
static BOOL valid_blob(HCRYPTPROV owner,const BYTE*data,DWORD bytes,HCRYPTKEY parent)
{
    const BLOBHEADER*b=(const BLOBHEADER*)data;const RSAPUBKEY*r;
    ULONGLONG required;HCRYPTKEY temporary;DWORD size,error;BOOL ok;
    if(b->bVersion!=CUR_BLOB_VERSION){SetLastError(NTE_BAD_VER);return FALSE;}
    if(b->reserved){SetLastError(NTE_BAD_DATA);return FALSE;}
    /* The old CSP reads RSA fields and modulus bytes before checking cbData.
       Validate public/unencrypted-private lengths at the API boundary. */
    if(b->bType==PUBLICKEYBLOB||(b->bType==PRIVATEKEYBLOB&&!parent)){
        if(bytes<sizeof(BLOBHEADER)+sizeof(RSAPUBKEY)){SetLastError(NTE_BAD_DATA);return FALSE;}
        r=(const RSAPUBKEY*)(data+sizeof(BLOBHEADER));
        if(!r->bitlen||(r->bitlen&15)||r->magic!=(b->bType==PUBLICKEYBLOB?0x31415352:0x32415352)){
            SetLastError(NTE_BAD_DATA);return FALSE;
        }
        required=20+(ULONGLONG)(r->bitlen/8)*(b->bType==PUBLICKEYBLOB?1:2);
        if(b->bType==PRIVATEKEYBLOB)required+=(ULONGLONG)(r->bitlen/16)*5;
        if(required>bytes){SetLastError(NTE_BAD_DATA);return FALSE;}
    }else if(b->bType==SIMPLEBLOB){
        if(bytes<12){SetLastError(NTE_BAD_DATA);return FALSE;}
        if(!cp_get_user_key(owner,AT_KEYEXCHANGE,&temporary))return FALSE;
        size=0;ok=cp_export_key(owner,temporary,0,PUBLICKEYBLOB,0,0,&size);error=GetLastError();
        cp_destroy_key(owner,temporary);
        if(!ok){SetLastError(error);return FALSE;}
        if(size<20||(ULONGLONG)bytes<12+(ULONGLONG)size-20){SetLastError(NTE_BAD_DATA);return FALSE;}
    }
    return TRUE;
}
BOOL WINAPI fp_CryptImportKey(HCRYPTPROV owner,const BYTE*data,DWORD bytes,HCRYPTKEY parent,DWORD flags,HCRYPTKEY*out)
{
    HashHandle*k,*p=0;BOOL ok;DWORD error;
    if(!out){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}*out=0;
    if(!data||bytes<sizeof(BLOBHEADER)){SetLastError(NTE_BAD_DATA);return FALSE;}
    lock();if(!provider_module){unlock();SetLastError(NTE_BAD_UID);return FALSE;}
    if(parent){p=find_object(keys,parent);if(!p||p->owner!=owner){unlock();SetLastError(NTE_BAD_KEY);return FALSE;}}
    if(!valid_blob(owner,data,bytes,parent)){error=GetLastError();unlock();SetLastError(error);return FALSE;}
    k=HeapAlloc(GetProcessHeap(),0,sizeof(*k));if(!k){unlock();SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    ok=cp_import_key(owner,data,bytes,p?p->native:0,flags,&k->native);error=GetLastError();
    if(ok){k->owner=owner;k->next=keys;keys=k;*out=(HCRYPTKEY)k;}else HeapFree(GetProcessHeap(),0,k);
    unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptGetUserKey(HCRYPTPROV owner,DWORD specification,HCRYPTKEY*out)
{
    HashHandle*k;BOOL ok;DWORD error;
    if(!out){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}*out=0;
    lock();if(!provider_module){unlock();SetLastError(NTE_BAD_UID);return FALSE;}
    k=HeapAlloc(GetProcessHeap(),0,sizeof(*k));if(!k){unlock();SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    ok=cp_get_user_key(owner,specification,&k->native);error=GetLastError();
    if(ok){k->owner=owner;k->next=keys;keys=k;*out=(HCRYPTKEY)k;}else HeapFree(GetProcessHeap(),0,k);
    unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptDestroyKey(HCRYPTKEY handle)
{
    HashHandle*k;BOOL ok;DWORD error;
    lock();k=find_object(keys,handle);if(!k){unlock();SetLastError(NTE_BAD_KEY);return FALSE;}
    ok=cp_destroy_key(k->owner,k->native);error=GetLastError();if(ok)remove_object(&keys,k);unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptExportKey(HCRYPTKEY handle,HCRYPTKEY parent,DWORD type,DWORD flags,BYTE*out,DWORD*bytes)
{
    HashHandle*k,*p=0;BOOL ok;DWORD error;
    if(!bytes){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();k=find_object(keys,handle);if(!k){unlock();SetLastError(NTE_BAD_KEY);return FALSE;}
    if(parent){p=find_object(keys,parent);if(!p||p->owner!=k->owner){unlock();SetLastError(NTE_BAD_KEY);return FALSE;}}
    ok=cp_export_key(k->owner,k->native,p?p->native:0,type,flags,out,bytes);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptSetKeyParam(HCRYPTKEY handle,DWORD parameter,const BYTE*data,DWORD flags)
{
    HashHandle*k;BOOL ok;DWORD error;
    if(!data){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();k=find_object(keys,handle);if(!k){unlock();SetLastError(NTE_BAD_KEY);return FALSE;}
    ok=cp_set_key(k->owner,k->native,parameter,data,flags);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptGetProvParam(HCRYPTPROV owner,DWORD parameter,BYTE*out,DWORD*bytes,DWORD flags)
{
    BOOL ok;DWORD error,capacity,probe=0;
    if(!bytes){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();if(!provider_module){unlock();SetLastError(NTE_BAD_UID);return FALSE;}
    if(parameter==PP_PROVTYPE){
        if(flags){unlock();SetLastError(NTE_BAD_FLAGS);return FALSE;}
        /* NT4's original provider predates PP_PROVTYPE. Validate the actual
           context before exposing this bridge's fixed provider type. */
        ok=cp_get_provider(owner,PP_NAME,0,&probe,0);error=GetLastError();
        if(!ok){unlock();SetLastError(error);return FALSE;}
        capacity=*bytes;*bytes=sizeof(DWORD);
        if(out&&capacity<sizeof(DWORD)){unlock();SetLastError(ERROR_MORE_DATA);return FALSE;}
        if(out)*(DWORD*)out=PROV_RSA_FULL;
        unlock();return TRUE;
    }
    ok=cp_get_provider(owner,parameter,out,bytes,flags);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptEncrypt(HCRYPTKEY handle,HCRYPTHASH hash,BOOL final,DWORD flags,BYTE*data,DWORD*bytes,DWORD capacity)
{
    HashHandle*k,*h=0;BOOL ok;DWORD error;
    if(!bytes){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();k=find_object(keys,handle);if(!k){unlock();SetLastError(NTE_BAD_KEY);return FALSE;}
    if(hash){h=find_hash(hash);if(!h||h->owner!=k->owner){unlock();SetLastError(NTE_BAD_HASH);return FALSE;}}
    ok=cp_encrypt(k->owner,k->native,h?h->native:0,final,flags,data,bytes,capacity);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptDecrypt(HCRYPTKEY handle,HCRYPTHASH hash,BOOL final,DWORD flags,BYTE*data,DWORD*bytes)
{
    HashHandle*k,*h=0;BOOL ok;DWORD error;
    if(!bytes||!data){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();k=find_object(keys,handle);if(!k){unlock();SetLastError(NTE_BAD_KEY);return FALSE;}
    if(hash){h=find_hash(hash);if(!h||h->owner!=k->owner){unlock();SetLastError(NTE_BAD_HASH);return FALSE;}}
    ok=cp_decrypt(k->owner,k->native,h?h->native:0,final,flags,data,bytes);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptSignHashW(HCRYPTHASH handle,DWORD specification,LPCWSTR description,DWORD flags,BYTE*out,DWORD*bytes)
{
    HashHandle*h;BOOL ok;DWORD error;
    if(!bytes){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    lock();h=find_hash(handle);if(!h){unlock();SetLastError(NTE_BAD_HASH);return FALSE;}
    ok=cp_sign(h->owner,h->native,specification,description,flags,out,bytes);error=GetLastError();unlock();if(!ok)SetLastError(error);return ok;
}
BOOL WINAPI fp_CryptEnumProvidersW(DWORD index,DWORD*reserved,DWORD flags,DWORD*type,LPWSTR name,DWORD*bytes)
{
    static const WCHAR provider[]=L"Microsoft Base Cryptographic Provider v1.0";
    DWORD capacity,i;
    if(reserved||flags||!type||!bytes){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    if(index){SetLastError(ERROR_NO_MORE_ITEMS);return FALSE;}
    /* This bridge exposes exactly its packaged provider, not providers that
       its restricted AcquireContext implementation cannot open. */
    lock();if(!load_provider()){DWORD e=GetLastError();unlock();SetLastError(e);return FALSE;}unlock();
    capacity=*bytes;*bytes=sizeof(provider);*type=PROV_RSA_FULL;
    if(!name)return TRUE;
    if(capacity<sizeof(provider)){SetLastError(ERROR_MORE_DATA);return FALSE;}
    for(i=0;i<sizeof(provider)/sizeof(WCHAR);i++)name[i]=provider[i];
    return TRUE;
}
