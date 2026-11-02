/* Native known-answer tests with a disposable, explicitly deleted key container. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include "crypto_key_fixtures.h"
static BOOL(WINAPI *acquire)(HCRYPTPROV*,LPCSTR,LPCSTR,DWORD,DWORD);
static BOOL(WINAPI *acquirew)(HCRYPTPROV*,LPCWSTR,LPCWSTR,DWORD,DWORD);
static BOOL(WINAPI *release)(HCRYPTPROV,DWORD);
static BOOL(WINAPI *importkey)(HCRYPTPROV,const BYTE*,DWORD,HCRYPTKEY,DWORD,HCRYPTKEY*);
static BOOL(WINAPI *exportkey)(HCRYPTKEY,HCRYPTKEY,DWORD,DWORD,BYTE*,DWORD*);
static BOOL(WINAPI *userkey)(HCRYPTPROV,DWORD,HCRYPTKEY*);
static BOOL(WINAPI *destroykey)(HCRYPTKEY);
static BOOL(WINAPI *setkey)(HCRYPTKEY,DWORD,const BYTE*,DWORD);
static BOOL(WINAPI *getprov)(HCRYPTPROV,DWORD,BYTE*,DWORD*,DWORD);
static BOOL(WINAPI *encrypt)(HCRYPTKEY,HCRYPTHASH,BOOL,DWORD,BYTE*,DWORD*,DWORD);
static BOOL(WINAPI *decrypt)(HCRYPTKEY,HCRYPTHASH,BOOL,DWORD,BYTE*,DWORD*);
static BOOL(WINAPI *sign)(HCRYPTHASH,DWORD,LPCWSTR,DWORD,BYTE*,DWORD*);
static BOOL(WINAPI *enumerate)(DWORD,DWORD*,DWORD,DWORD*,LPWSTR,DWORD*);
static BOOL(WINAPI *createhash)(HCRYPTPROV,ALG_ID,HCRYPTKEY,DWORD,HCRYPTHASH*);
static BOOL(WINAPI *feed)(HCRYPTHASH,const BYTE*,DWORD,DWORD);
static BOOL(WINAPI *gethash)(HCRYPTHASH,DWORD,BYTE*,DWORD*,DWORD);
static BOOL(WINAPI *destroyhash)(HCRYPTHASH);
static HANDLE logh;static DWORD checks,failures;
static void number(const char*s,DWORD v){char b[200];DWORD n;wsprintfA(b,"%s %lu\r\n",s,v);WriteFile(logh,b,lstrlenA(b),&n,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok){failures++;number("Last error",GetLastError());}number(s,ok);}
static BOOL equal(const BYTE*a,const BYTE*b,DWORD n){while(n--)if(*a++!=*b++)return FALSE;return TRUE;}
static BOOL signed_answer(HCRYPTPROV p){HCRYPTHASH h=0;BYTE out[68];DWORD n=0;BOOL ok=FALSE;
 if(!createhash(p,CALG_SHA1,0,0,&h))return FALSE;
 if(!feed(h,(const BYTE*)"abc",3,0)||!sign(h,AT_SIGNATURE,0,0,0,&n)||n!=64)goto done;
 n=64;out[64]=out[67]=0xa5;ok=sign(h,AT_SIGNATURE,0,0,out,&n)&&n==64&&equal(out,sha1_signature_abc,64)&&out[64]==0xa5&&out[67]==0xa5;
done:destroyhash(h);return ok;}
#define LOAD(variable,name) variable=(void*)GetProcAddress(m,name);check(variable!=0,name);if(!variable)goto done
void WINAPI entry(void){HMODULE m;HCRYPTPROV p=0,q=0,temp=0;HCRYPTKEY pub=0,priv=0,stream=0,copy=0,rc2=0,sig=0,u=0,stale;
 HCRYPTHASH mac=0;BYTE out[512],salt[11],mac_value[16];DWORD n,type,i,value;WCHAR name[128],wide_container[80];char container[80];BOOL owned=FALSE,ok;
 logh=CreateFileA("C:\\FP32\\KEY.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 m=LoadLibraryA("C:\\FP32\\K32A.DLL");check(m!=0,"Production key adapter loads");if(!m)goto done;
 LOAD(acquire,"CryptAcquireContextA");LOAD(acquirew,"CryptAcquireContextW");LOAD(release,"CryptReleaseContext");LOAD(importkey,"CryptImportKey");LOAD(exportkey,"CryptExportKey");LOAD(userkey,"CryptGetUserKey");LOAD(destroykey,"CryptDestroyKey");LOAD(setkey,"CryptSetKeyParam");LOAD(getprov,"CryptGetProvParam");LOAD(encrypt,"CryptEncrypt");LOAD(decrypt,"CryptDecrypt");LOAD(sign,"CryptSignHashW");LOAD(enumerate,"CryptEnumProvidersW");LOAD(createhash,"CryptCreateHash");LOAD(feed,"CryptHashData");LOAD(gethash,"CryptGetHashParam");LOAD(destroyhash,"CryptDestroyHash");
 n=0;type=0;check(enumerate(0,0,0,&type,0,&n)&&type==PROV_RSA_FULL&&n==sizeof(L"Microsoft Base Cryptographic Provider v1.0"),"Provider size query in bytes");
 n=2;name[0]=0xa5;check(!enumerate(0,0,0,&type,name,&n)&&GetLastError()==ERROR_MORE_DATA&&name[0]==0xa5,"Short provider buffer unchanged");
 n=sizeof(name);check(enumerate(0,0,0,&type,name,&n)&&!lstrcmpW(name,L"Microsoft Base Cryptographic Provider v1.0"),"Supported provider enumeration");
 check(!enumerate(1,0,0,&type,name,&n)&&GetLastError()==ERROR_NO_MORE_ITEMS,"Enumeration terminates");
 check(!enumerate(0,&value,0,&type,name,&n)&&GetLastError()==ERROR_INVALID_PARAMETER,"Reserved enumeration parameter rejected");
 wsprintfA(container,"FP32KeyTest-%lu-%lu",GetCurrentProcessId(),GetTickCount());for(i=0;container[i];i++)wide_container[i]=(BYTE)container[i];wide_container[i]=0;
 check(acquirew(&p,wide_container,0,PROV_RSA_FULL,CRYPT_NEWKEYSET),"Create disposable Unicode-named container");if(!p)goto done;owned=TRUE;
 n=sizeof(out);check(getprov(p,PP_NAME,out,&n,0)&&!lstrcmpA((char*)out,"Microsoft Base Cryptographic Provider v1.0"),"Provider name from original CSP");
 n=4;type=0;check(getprov(p,PP_PROVTYPE,(BYTE*)&type,&n,0)&&n==4&&type==PROV_RSA_FULL,"Provider type for packaged CSP");
 n=0;check(getprov(p,PP_PROVTYPE,0,&n,0)&&n==4,"Provider type size query");
 n=3;value=0xa5a5a5a5;check(!getprov(p,PP_PROVTYPE,(BYTE*)&value,&n,0)&&GetLastError()==ERROR_MORE_DATA&&n==4&&value==0xa5a5a5a5,"Short provider type output unchanged");
 n=4;check(!getprov((HCRYPTPROV)0x12345678,PP_PROVTYPE,(BYTE*)&value,&n,0),"Invalid provider rejected by type query");
 check(!userkey(p,AT_SIGNATURE,&u),"Absent signature key reported");
 check(!importkey(p,public_exchange,7,0,0,&pub)&&GetLastError()==(DWORD)NTE_BAD_DATA&&!pub,"Truncated BLOB header rejected");
 check(!importkey(p,public_exchange,sizeof(public_exchange)-1,0,0,&pub)&&GetLastError()==(DWORD)NTE_BAD_DATA&&!pub,"Truncated public modulus rejected");
 check(!importkey(p,private_exchange,sizeof(private_exchange)-1,0,0,&priv)&&GetLastError()==(DWORD)NTE_BAD_DATA&&!priv,"Truncated private components rejected");
 check(importkey(p,public_exchange,sizeof(public_exchange),0,0,&pub),"Import RSA public key");
 if(pub){n=0;check(exportkey(pub,0,PUBLICKEYBLOB,0,0,&n)&&n==sizeof(public_exchange),"Public export size");n=sizeof(out);check(exportkey(pub,0,PUBLICKEYBLOB,0,out,&n)&&n==sizeof(public_exchange)&&equal(out,public_exchange,n),"Public export exact bytes");stale=pub;check(destroykey(pub),"Destroy public key");pub=0;check(!destroykey(stale)&&GetLastError()==(DWORD)NTE_BAD_KEY,"Destroyed key rejected");}
 check(importkey(p,private_exchange,sizeof(private_exchange),0,CRYPT_EXPORTABLE,&priv),"Import exportable RSA private key");if(!priv)goto cleanup;
 n=sizeof(out);check(exportkey(priv,0,PRIVATEKEYBLOB,0,out,&n)&&n==sizeof(private_exchange)&&equal(out,private_exchange,n),"Private export exact CRT components");
 check(userkey(p,AT_KEYEXCHANGE,&u),"Get stored exchange key");if(u){check(destroykey(u),"Destroy retrieved key handle");u=0;}
 check(!importkey(p,simple_rc4,sizeof(simple_rc4)-1,0,0,&stream)&&GetLastError()==(DWORD)NTE_BAD_DATA&&!stream,"Truncated encrypted session key rejected");
 check(importkey(p,simple_rc4,sizeof(simple_rc4),0,CRYPT_EXPORTABLE,&stream),"Import RSA-wrapped RC4 key");if(!stream)goto cleanup;
 for(i=0;i<11;i++)salt[i]=0x5a;check(setkey(stream,KP_SALT,salt,0),"Set explicit RC4 salt");
 for(i=0;i<sizeof(out);i++)out[i]=0;n=32;
 check(encrypt(stream,0,TRUE,0,out,&n,sizeof(out))&&n==32,"Encrypt RC4 plaintext");
 check(equal(out,rc4_salted_zeros,32),"RC4 output matches independent fixture");
 check(importkey(p,simple_rc4,sizeof(simple_rc4),0,0,&copy)&&setkey(copy,KP_SALT,salt,0),"Import independent decrypt key");
 if(copy){check(decrypt(copy,0,TRUE,0,out,&n)&&n==32,"Decrypt RC4 ciphertext");ok=TRUE;for(i=0;i<n;i++)if(out[i])ok=FALSE;check(ok,"RC4 plaintext restored");}
 check(acquire(&q,0,0,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT),"Independent ephemeral provider");
 if(q){check(createhash(q,CALG_SHA1,0,0,&mac),"Hash on independent provider");if(mac){n=8;check(!encrypt(stream,mac,TRUE,0,out,&n,sizeof(out))&&GetLastError()==(DWORD)NTE_BAD_HASH,"Cross-provider hash and key rejected");destroyhash(mac);mac=0;}release(q,0);q=0;}
 check(importkey(p,simple_rc2,sizeof(simple_rc2),0,0,&rc2),"Import RSA-wrapped RC2 key");
 if(rc2){check(createhash(p,CALG_MAC,rc2,0,&mac),"Create native keyed MAC");if(mac){n=sizeof(mac_value);check(feed(mac,(const BYTE*)"abc",3,0)&&gethash(mac,HP_HASHVAL,mac_value,&n,0)&&n==8,"Keyed MAC produces a native block");destroyhash(mac);mac=0;}}
 check(importkey(p,private_signature,sizeof(private_signature),0,CRYPT_EXPORTABLE,&sig),"Import RSA signature key");
 if(sig)check(signed_answer(p),"RSA SHA1 signature matches independent PKCS1 fixture");
 check(release(p,0),"Release provider with child keys");p=0;
 n=16;check(!encrypt(stream,0,TRUE,0,out,&n,sizeof(out))&&GetLastError()==(DWORD)NTE_BAD_KEY,"Released provider invalidates key handles");
 check(acquirew(&p,wide_container,0,PROV_RSA_FULL,0),"Reopen named container");
 if(p){check(userkey(p,AT_SIGNATURE,&u),"Signature key persists after reopen");if(u)destroykey(u);u=0;check(signed_answer(p),"Reopened key produces the same independent signature");}
cleanup:
 if(mac&&destroyhash)destroyhash(mac);if(q&&release)release(q,0);if(p&&release)release(p,0);
 if(owned){check(acquire(&temp,container,0,PROV_RSA_FULL,CRYPT_DELETEKEYSET)&&!temp,"Delete only the disposable container");check(!acquire(&temp,container,0,PROV_RSA_FULL,0),"Deleted container cannot be reopened");if(temp)release(temp,0);}
done:number("Checks",checks);number("Failures",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
