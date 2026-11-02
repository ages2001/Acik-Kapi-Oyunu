/* Read-only Windows 2000/XP catalog decoding and signature diagnostics.
 * No certificate import, trust-store modification or driver installation.
 */
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <mscat.h>
#include <softpub.h>
static DWORD failures;
static void out(const char*s){DWORD w;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&w,NULL);}
static void hex(DWORD v){char b[11];int i;b[0]='0';b[1]='x';b[10]=0;for(i=0;i<8;i++)b[i+2]="0123456789ABCDEF"[(v>>(28-4*i))&15];out(b);}
static void wide(const WCHAR*s){char b[512];unsigned i;if(!s){out("(null)");return;}for(i=0;i+1<sizeof(b)&&s[i];i++)b[i]=(char)s[i];b[i]=0;out(b);}
static BYTE signer[4096];
static void member_file(WCHAR*path,BOOL expect_member)
{
 HANDLE f,cat;BYTE digest[64];DWORD size=sizeof(digest),i;WCHAR tag[129];CRYPTCATMEMBER*m;
 out("FILE ");wide(path);out("\r\n");
 f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 if(f==INVALID_HANDLE_VALUE){out("FILE_OPEN_ERROR ");hex(GetLastError());out("\r\n");failures++;return;}
 if(!CryptCATAdminCalcHashFromFileHandle(f,&size,digest,0)||size!=20){out("FILE_HASH_ERROR ");hex(GetLastError());out("\r\n");failures++;CloseHandle(f);return;}
 for(i=0;i<size;i++){tag[i*2]=L"0123456789ABCDEF"[digest[i]>>4];tag[i*2+1]=L"0123456789ABCDEF"[digest[i]&15];}tag[size*2]=0;
 out("WINDOWS_HASH ");wide(tag);out("\r\n");
 cat=CryptCATOpen(L"D:\\NEW.CAT",0,0,CRYPTCAT_VERSION_1,0);
 if(!cat||cat==INVALID_HANDLE_VALUE){out("MEMBER_CATALOG_OPEN_ERROR\r\n");failures++;CloseHandle(f);return;}
 m=CryptCATGetMemberInfo(cat,tag);
 if(!expect_member){if(!m)out("MODIFIED_FILE_REJECTED PASS\r\n");else{out("MODIFIED_FILE_ACCEPTED FAIL\r\n");failures++;}}
 else if(!m||!m->pIndirectData||m->pIndirectData->Digest.cbData!=size){out("MEMBER_LOOKUP_OR_DIGEST FAIL\r\n");failures++;}
 else{
  for(i=0;i<size&&digest[i]==m->pIndirectData->Digest.pbData[i];i++);
  if(i==size)out("MEMBER_DIGEST PASS\r\n");else{out("MEMBER_DIGEST FAIL\r\n");failures++;}
 }
 CryptCATClose(cat);CloseHandle(f);
}
static void probe(WCHAR*path)
{
 HANDLE cat;CRYPTCATMEMBER*m=NULL;DWORD members=0,encoding,content,format,size;
 HCERTSTORE store=NULL;HCRYPTMSG message=NULL;PCCERT_CONTEXT cert;
 out("CATALOG ");wide(path);out("\r\n");
 SetLastError(0);cat=CryptCATOpen(path,0,0,0x10001,0);
 if(cat==INVALID_HANDLE_VALUE||!cat){out("CAT_OPEN_ERROR ");hex(GetLastError());out("\r\n");}
 else{
  while((m=CryptCATEnumerateMember(cat,m))!=NULL){members++;out("MEMBER ");wide(m->pwszReferenceTag);out(" FILE ");wide(m->pwszFileName);out("\r\n");}
  out("MEMBERS ");hex(members);out("\r\n");CryptCATClose(cat);
 }
 if(!CryptQueryObject(CERT_QUERY_OBJECT_FILE,path,CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED,CERT_QUERY_FORMAT_FLAG_BINARY,0,&encoding,&content,&format,&store,&message,NULL)){out("QUERY_ERROR ");hex(GetLastError());out("\r\n");return;}
 size=sizeof(signer);
 if(!CryptMsgGetParam(message,CMSG_SIGNER_CERT_INFO_PARAM,0,signer,&size)){out("SIGNER_ERROR ");hex(GetLastError());out("\r\n");goto done;}
 cert=CertFindCertificateInStore(store,encoding,0,CERT_FIND_SUBJECT_CERT,signer,NULL);
 if(!cert){out("CERT_ERROR ");hex(GetLastError());out("\r\n");goto done;}
 out("CERT_SIGNATURE_OID ");out(cert->pCertInfo->SignatureAlgorithm.pszObjId);out("\r\n");
 SetLastError(0);
 if(CryptMsgControl(message,0,CMSG_CTRL_VERIFY_SIGNATURE,cert->pCertInfo))out("MESSAGE_SIGNATURE PASS\r\n");
 else{out("MESSAGE_SIGNATURE_ERROR ");hex(GetLastError());out("\r\n");}
 SetLastError(0);
 if(CryptVerifyCertificateSignatureEx(0,encoding,CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT,(void*)cert,CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,(void*)cert,0,NULL))out("SELF_SIGNATURE PASS\r\n");
 else{out("SELF_SIGNATURE_ERROR ");hex(GetLastError());out("\r\n");}
 CertFreeCertificateContext(cert);
done:
 CryptMsgClose(message);CertCloseStore(store,0);
}
void Entry(void)
{
 WCHAR old[]=L"D:\\OLD.CAT",current[]=L"D:\\NEW.CAT";
 probe(old);probe(current);
 member_file(L"D:\\NETRTLK.INF",TRUE);member_file(L"D:\\DRV32.SYS",TRUE);member_file(L"D:\\DRV64.SYS",TRUE);member_file(L"D:\\CHANGED.INF",FALSE);
 out("MEMBERSHIP_FAILURES ");hex(failures);out("\r\n");
 out("Signature and member checks do not establish publisher trust.\r\n");ExitProcess(failures?1:0);
}
