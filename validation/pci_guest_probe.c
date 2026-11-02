/* Guest-observed PCI names, loaded image size and six independent MTU paths.
 * Diagnostic fixture only. Reports synthetic evidence to a loopback collector.
 */
#define _WIN32_WINNT 0x0500
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <ipexport.h>
#ifndef EXPECTED_MODULE_SIZE
#error Compile against the exact test driver PE SizeOfImage
#endif
static char report[16384];static unsigned used;
static BYTE modules[131072],payload[1472],reply[4096];
static void zero(void*p,unsigned n){volatile BYTE*b=p;while(n--)*b++=0;}
static void add(const char*s){while(*s&&used+1<sizeof(report))report[used++]=*s++;report[used]=0;}
static void number(DWORD n){char b[12];int at=10;b[11]=0;do{b[at--]='0'+n%10;n/=10;}while(n);add(b+at+1);}
static int has(const char*s,const char*q){unsigned i,j;for(i=0;s[i];i++){for(j=0;q[j]&&s[i+j]==q[j];j++);if(!q[j])return 1;}return 0;}
typedef struct {
 HANDLE Section;PVOID MappedBase,ImageBase;ULONG ImageSize,Flags;
 USHORT LoadOrderIndex,InitOrderIndex,LoadCount,OffsetToFileName;
 UCHAR FullPathName[256];
} MODULE;
typedef struct {ULONG Count;MODULE Items[1];} MODULES;
typedef LONG (WINAPI*QUERY)(ULONG,PVOID,ULONG,PULONG);
static int loaded_image(void)
{
 QUERY query=(QUERY)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtQuerySystemInformation");
 MODULES*list=(MODULES*)modules;ULONG returned=0,i,offset=(ULONG)((BYTE*)&list->Items[0]-modules);
 if(!query||query(11,modules,sizeof(modules),&returned)<0||returned>sizeof(modules)||returned<offset){add("MODULE_QUERY FAIL\r\n");return 0;}
 if(list->Count>(returned-offset)/sizeof(MODULE)){add("MODULE_BOUNDS FAIL\r\n");return 0;}
 for(i=0;i<list->Count;i++){
  MODULE*m=&list->Items[i];char*name;
  if(m->OffsetToFileName>=sizeof(m->FullPathName))continue;
  m->FullPathName[255]=0;name=(char*)m->FullPathName+m->OffsetToFileName;
  if(lstrcmpiA(name,"rltklant.sys"))continue;
  add("MODULE ");add((char*)m->FullPathName);add(" IMAGE_SIZE ");number(m->ImageSize);
  add(" EXPECTED ");number(EXPECTED_MODULE_SIZE);add("\r\n");
  return m->ImageSize==EXPECTED_MODULE_SIZE;
 }
 add("MODULE RLTKLANT missing\r\n");return 0;
}
typedef HANDLE (WINAPI*OPEN)(void);
typedef BOOL (WINAPI*CLOSE)(HANDLE);
typedef DWORD (WINAPI*ECHO)(HANDLE,IPAddr,LPVOID,WORD,PIP_OPTION_INFORMATION,LPVOID,DWORD,DWORD);
void Entry(void)
{
 WSADATA w;SOCKET sock;struct sockaddr_in host;HDEVINFO set;SP_DEVINFO_DATA d;
 unsigned i,j,net,count=0,problems=0,replies=0;char*line=GetCommandLineA(),*token;
 const char*destinations[]={"10.0.2.2","10.0.3.2","10.0.4.2","10.0.5.2","10.0.6.2","10.0.7.2"};
 HANDLE h,file;HMODULE lib;OPEN open;CLOSE close;ECHO echo;DWORD written,n;int image_ok,network_ok=0;
 IP_OPTION_INFORMATION options;
 if(*line=='"'){line++;while(*line&&*line!='"')line++;if(*line)line++;}else while(*line&&*line!=' ')line++;
 while(*line==' ')line++;
 token=line;if(!*token)token="manual";add("TOKEN ");add(token);add("\r\n");
 image_ok=loaded_image();
 set=SetupDiGetClassDevsA(NULL,NULL,NULL,DIGCF_ALLCLASSES|DIGCF_PRESENT);
 if(set!=INVALID_HANDLE_VALUE){for(i=0;;i++){
  char id[512],name[256];ULONG status=0,problem=0;CONFIGRET cr;
  zero(&d,sizeof(d));d.cbSize=sizeof(d);if(!SetupDiEnumDeviceInfo(set,i,&d))break;
  if(!SetupDiGetDeviceInstanceIdA(set,&d,id,sizeof(id),NULL)||!has(id,"PCI\\VEN_10EC"))continue;
  count++;zero(name,sizeof(name));
  if(!SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_FRIENDLYNAME,NULL,(BYTE*)name,sizeof(name),NULL))
   SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_DEVICEDESC,NULL,(BYTE*)name,sizeof(name),NULL);
  cr=CM_Get_DevNode_Status(&status,&problem,d.DevInst,0);
  add("DEVICE ");add(id);add("\r\nNAME ");add(name);add("\r\nPROBLEM ");number(problem);
  add(" STATUS ");number(status);add(" CM ");number(cr);add("\r\n");
  if(cr||problem||!(status&DN_STARTED)||!has(name,"Realtek RTL"))problems++;
 }SetupDiDestroyDeviceInfoList(set);}else problems++;
 if(WSAStartup(0x0202,&w)){add("FAIL WSAStartup\r\n");goto finish;}
 network_ok=1;
 lib=LoadLibraryA("icmp.dll");if(!lib)lib=LoadLibraryA("iphlpapi.dll");
 open=lib?(OPEN)GetProcAddress(lib,"IcmpCreateFile"):NULL;
 close=lib?(CLOSE)GetProcAddress(lib,"IcmpCloseHandle"):NULL;
 echo=lib?(ECHO)GetProcAddress(lib,"IcmpSendEcho"):NULL;
 if(image_ok&&count==6&&!problems&&open&&close&&echo){h=open();if(h!=INVALID_HANDLE_VALUE){
  zero(&options,sizeof(options));options.Ttl=64;options.Flags=IP_FLAG_DF;
  for(i=0;i<sizeof(payload);i++)payload[i]=(BYTE)(i*31+7);
  for(net=0;net<6;net++){
   unsigned matched=0;IPAddr address=inet_addr(destinations[net]);
   for(i=0;i<4;i++){
    PICMP_ECHO_REPLY r=(PICMP_ECHO_REPLY)reply;zero(reply,sizeof(reply));
    n=echo(h,address,payload,sizeof(payload),&options,reply,sizeof(reply),1500);
    if(n&&r->Status==IP_SUCCESS&&r->Address==address&&r->DataSize==sizeof(payload)){
     for(j=0;j<sizeof(payload)&&((BYTE*)r->Data)[j]==payload[j];j++);
     if(j==sizeof(payload)){matched++;replies++;}
    }
    Sleep(100);
   }
   add("PATH ");add(destinations[net]);add(" REPLIES ");number(matched);add(" PAYLOAD 1472\r\n");
  }close(h);
 }}
 add("PCI_DEVICES ");number(count);add(" PROBLEMS ");number(problems);add(" ICMP_REPLIES ");number(replies);add("\r\n");
 add(image_ok&&count==6&&!problems&&replies==24?"PASS\r\n":"FAIL\r\n");
 zero(&host,sizeof(host));host.sin_family=AF_INET;host.sin_port=htons(18201);host.sin_addr.s_addr=inet_addr("10.0.2.2");
 sock=socket(AF_INET,SOCK_DGRAM,0);if(sock!=INVALID_SOCKET){sendto(sock,report,used,0,(struct sockaddr*)&host,sizeof(host));closesocket(sock);}
finish:
 if(network_ok)WSACleanup();
 WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),report,used,&written,NULL);
 file=CreateFileA("C:\\RLTTEST\\pci-proof.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file!=INVALID_HANDLE_VALUE){SetFilePointer(file,0,NULL,FILE_END);WriteFile(file,report,used,&written,NULL);CloseHandle(file);}
 ExitProcess(image_ok&&count==6&&!problems&&replies==24?0:1);
}
