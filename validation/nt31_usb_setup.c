/* Test-fixture setup using NT3.1's registry API, not an offline hive rewrite. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static unsigned length(const char*s){unsigned n=0;while(s[n])n++;return n;}
static void print(const char*s){DWORD done;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,length(s),&done,NULL);}
static void number(DWORD n){char b[12];int at=10;b[11]=0;do{b[at--]=(char)('0'+n%10);n/=10;}while(n);print(b+at+1);}
static void fail(const char*s,LONG code){print(s);print(": ");number(code);print("\r\n");ExitProcess(1);}
static int prefix(const char*s,const char*t){while(*t)if(*s++!=*t++)return 0;return 1;}
static void set(HKEY key,const char*name,DWORD type,const void*data,DWORD size){LONG r=RegSetValueExA(key,name,0,type,data,size);if(r)fail(name,r);}
void mainCRTStartup(void){
 HKEY services,adapter,key;DWORD index=0,n,one=1,zero=0,disposition;LONG r;char name[256],path[300];unsigned i,j;int found=0;
 r=RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Services",0,KEY_READ|KEY_WRITE,&services);if(r)fail("Services",r);
 while((r=RegEnumKeyA(services,index++,name,sizeof(name)))==ERROR_SUCCESS){
  if(!prefix(name,"RLTKLANT"))continue;
  print("Found service: ");print(name);print("\r\n");
  for(i=0;name[i];i++)path[i]=name[i];
  {const char*s="\\Parameters";for(j=0;s[j];j++)path[i++]=s[j];path[i]=0;}
  if(RegOpenKeyExA(services,path,0,KEY_READ|KEY_WRITE,&adapter))continue;
  /* The adapter instance is RLTKLANT plus a decimal suffix. The shared
     service Parameters key must not be mistaken for an adapter binding. */
  if(name[8]<'0'||name[8]>'9'){RegCloseKey(adapter);continue;}
  set(adapter,"UsbNtAdapter",REG_DWORD,&one,4);
  set(adapter,"UsbPermanentAddress",REG_SZ,"525400abcdef",13);
  set(adapter,"BusType",REG_DWORD,&zero,4);
  RegCloseKey(adapter);found++;print("Configured USB instance: ");print(name);print("\r\n");
 }
 if(r!=ERROR_NO_MORE_ITEMS)fail("Enumerate services",r);
 if(found!=1)fail("Expected exactly one RLTKLANT adapter",found);
 r=RegCreateKeyExA(services,"usbnt",0,NULL,REG_OPTION_NON_VOLATILE,KEY_READ|KEY_WRITE,NULL,&key,&disposition);if(r)fail("Create USBNT service",r);
 set(key,"Type",REG_DWORD,&one,4);set(key,"Start",REG_DWORD,&zero,4);set(key,"ErrorControl",REG_DWORD,&one,4);
 set(key,"Group",REG_SZ,"SCSI miniport",14);
 {const char*s="system32\\drivers\\usbnt.sys";n=length(s)+1;set(key,"ImagePath",REG_EXPAND_SZ,s,n);}
 RegCloseKey(key);RegCloseKey(services);
 print("USBNT and persistent USB adapter configured. Shut down before changing the VM NIC layout.\r\n");ExitProcess(0);
}
