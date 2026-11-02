/* Known-answer and ownership tests through the production CSP bridge. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
static BOOL(WINAPI *acquire)(HCRYPTPROV*,LPCSTR,LPCSTR,DWORD,DWORD);
static BOOL(WINAPI *release)(HCRYPTPROV,DWORD);
static BOOL(WINAPI *create)(HCRYPTPROV,ALG_ID,HCRYPTKEY,DWORD,HCRYPTHASH*);
static BOOL(WINAPI *feed)(HCRYPTHASH,const BYTE*,DWORD,DWORD);
static BOOL(WINAPI *get)(HCRYPTHASH,DWORD,BYTE*,DWORD*,DWORD);
static BOOL(WINAPI *set)(HCRYPTHASH,DWORD,const BYTE*,DWORD);
static BOOL(WINAPI *destroy)(HCRYPTHASH);
static const BYTE md5[16]={0x90,0x01,0x50,0x98,0x3c,0xd2,0x4f,0xb0,0xd6,0x96,0x3f,0x7d,0x28,0xe1,0x7f,0x72};
static const BYTE sha1[20]={0xa9,0x99,0x3e,0x36,0x47,0x06,0x81,0x6a,0xba,0x3e,0x25,0x71,0x78,0x50,0xc2,0x6c,0x9c,0xd0,0xd8,0x9d};
static const BYTE empty_sha1[20]={0xda,0x39,0xa3,0xee,0x5e,0x6b,0x4b,0x0d,0x32,0x55,0xbf,0xef,0x95,0x60,0x18,0x90,0xaf,0xd8,0x07,0x09};
static HANDLE logh;static DWORD checks,failures,worker_errors[4];
static void number(const char*s,DWORD v){char b[180];DWORD n;wsprintfA(b,"%s %lu\r\n",s,v);WriteFile(logh,b,lstrlenA(b),&n,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok){failures++;number("Last error",GetLastError());}number(s,ok);}
static BOOL equal(const BYTE*a,const BYTE*b,DWORD n){while(n--)if(*a++!=*b++)return FALSE;return TRUE;}
static DWORD WINAPI worker(void*argument){DWORD id=(DWORD)argument,i,n;HCRYPTPROV p=0;HCRYPTHASH h;BYTE out[20];
 if(!acquire(&p,0,0,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT))return ++worker_errors[id];
 for(i=0;i<64;i++){h=0;n=sizeof(out);if(!create(p,CALG_SHA1,0,0,&h)||!feed(h,(const BYTE*)"abc",3,0)||!get(h,HP_HASHVAL,out,&n,0)||n!=20||!equal(out,sha1,20))worker_errors[id]++;if(h&&!destroy(h))worker_errors[id]++;}
 if(!release(p,0))worker_errors[id]++;return worker_errors[id];}
void WINAPI entry(void){HMODULE m;HCRYPTPROV p=0;HCRYPTHASH h=0,stale;BYTE out[24];DWORD n,value,i,id,exitcode;HANDLE threads[4];BOOL ok;
 logh=CreateFileA("C:\\FP32\\HASH.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 m=LoadLibraryA("C:\\FP32\\C32A.DLL");check(m!=0,"Production CSP adapter loads");if(!m)goto done;
 acquire=(void*)GetProcAddress(m,"CryptAcquireContextA");release=(void*)GetProcAddress(m,"CryptReleaseContext");create=(void*)GetProcAddress(m,"CryptCreateHash");feed=(void*)GetProcAddress(m,"CryptHashData");get=(void*)GetProcAddress(m,"CryptGetHashParam");set=(void*)GetProcAddress(m,"CryptSetHashParam");destroy=(void*)GetProcAddress(m,"CryptDestroyHash");
 check(acquire&&release&&create&&feed&&get&&set&&destroy,"All hash exports resolve");if(!acquire||!release||!create||!feed||!get||!set||!destroy)goto done;
 check(acquire(&p,0,0,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT),"Acquire original NT4 RSA provider");if(!p)goto done;
 check(!create(p,CALG_MD5,0,0,0)&&GetLastError()==ERROR_INVALID_PARAMETER,"NULL hash output rejected");
 check(!create(p,CALG_SHA_256,0,0,&h)&&!h,"Unsupported algorithm fails without a handle");
 check(!create(p,CALG_MD5,0,0x80000000,&h)&&!h,"Invalid creation flags rejected");
 check(create(p,CALG_MD5,0,0,&h),"Create MD5 hash");if(!h)goto cleanup;
 check(feed(h,(const BYTE*)"a",1,0)&&feed(h,(const BYTE*)"bc",2,0),"Hash data in two chunks");
 check(feed(h,0,0,0),"Empty input is accepted");
 n=4;value=0;check(get(h,HP_ALGID,(BYTE*)&value,&n,0)&&n==4&&value==CALG_MD5,"Algorithm identifier preserved");
 n=4;value=0;check(get(h,HP_HASHSIZE,(BYTE*)&value,&n,0)&&n==4&&value==16,"Hash size parameter preserved");
 n=0;check(get(h,HP_HASHVAL,0,&n,0)&&n==16,"Digest size query");
 for(i=0;i<sizeof(out);i++)out[i]=0xa5;n=15;ok=get(h,HP_HASHVAL,out,&n,0);
 check(!ok&&GetLastError()==ERROR_MORE_DATA&&n==16,"Short digest buffer reports required size");
 ok=TRUE;for(i=0;i<sizeof(out);i++)if(out[i]!=0xa5)ok=FALSE;check(ok,"Short output remains untouched");
 n=16;check(get(h,HP_HASHVAL,out,&n,0)&&n==16&&equal(out,md5,16),"RFC MD5 abc known answer");
 check(out[16]==0xa5&&out[23]==0xa5,"Digest write preserves trailing guards");
 check(!feed(h,(const BYTE*)"d",1,0),"Finalized hash rejects additional data");
 check(!get(h,HP_HASHVAL,out,0,0)&&GetLastError()==ERROR_INVALID_PARAMETER,"NULL size output rejected");
 check(!set(h,HP_HASHVAL,0,0)&&GetLastError()==ERROR_INVALID_PARAMETER,"NULL hash value rejected");
 stale=h;check(destroy(h),"Destroy hash");h=0;
 check(!destroy(stale)&&GetLastError()==(DWORD)NTE_BAD_HASH,"Destroyed handle rejected");
 n=20;check(!get((HCRYPTHASH)0x12345678,HP_HASHVAL,out,&n,0)&&GetLastError()==(DWORD)NTE_BAD_HASH,"Invalid pointer handle rejected safely");
 check(create(p,CALG_SHA1,0,0,&h),"Create empty SHA1 hash");
 if(h){n=20;check(get(h,HP_HASHVAL,out,&n,0)&&n==20&&equal(out,empty_sha1,20),"SHA1 empty known answer");check(destroy(h),"Destroy empty hash");h=0;}
 check(create(p,CALG_SHA1,0,0,&h),"Create explicit-value SHA1 hash");
 if(h){check(set(h,HP_HASHVAL,sha1,0),"Set explicit hash value");n=20;check(get(h,HP_HASHVAL,out,&n,0)&&equal(out,sha1,20),"Explicit hash value readback");check(destroy(h),"Destroy explicit-value hash");h=0;}
 check(create(p,CALG_MD5,0,0,&h),"Create child before provider release");
 check(!release(p,1),"Rejected provider release preserves context");
 if(h){check(feed(h,(const BYTE*)"abc",3,0),"Child remains valid after rejected release");}
 check(release(p,0),"Release provider with child");p=0;
 if(h){check(!feed(h,(const BYTE*)"abc",3,0)&&GetLastError()==(DWORD)NTE_BAD_HASH,"Released provider invalidates bridge child");h=0;}
 for(i=0;i<4;i++){threads[i]=CreateThread(0,0,worker,(void*)i,0,&id);check(threads[i]!=0,"Concurrent worker started");}
 for(i=0;i<4;i++)if(threads[i]){check(WaitForSingleObject(threads[i],60000)==WAIT_OBJECT_0,"Concurrent worker completed");check(GetExitCodeThread(threads[i],&exitcode)&&!exitcode&&!worker_errors[i],"Concurrent SHA1 known answers");CloseHandle(threads[i]);}
cleanup:if(h)destroy(h);if(p)release(p,0);
done:number("Checks",checks);number("Failures",failures);number("Concurrent hash cycles",256);CloseHandle(logh);ExitProcess(failures?2:0);
}
