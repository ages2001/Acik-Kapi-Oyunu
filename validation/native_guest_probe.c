/* Native NT5 USB runtime evidence: actual SetupAPI properties and ICMP replies.
 * Reports only synthetic fixture diagnostics to the host's loopback collector.
 * No CRT; supports Windows 2000 x86 and XP x64. */
#define _WIN32_WINNT 0x0500
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <ipexport.h>
static char report[8192];static unsigned used;
static void zero(void*p,unsigned n){volatile BYTE*b=p;while(n--)*b++=0;}
static void add(const char*s){while(*s&&used+1<sizeof(report))report[used++]=*s++;report[used]=0;}
static void number(DWORD n){char b[12];int at=10;b[11]=0;do{b[at--]='0'+n%10;n/=10;}while(n);add(b+at+1);}
static int has(const char*s,const char*q){unsigned i,j;for(i=0;s[i];i++){for(j=0;q[j]&&s[i+j]==q[j];j++);if(!q[j])return 1;}return 0;}
typedef HANDLE (WINAPI*OPEN)(void);
typedef BOOL (WINAPI*CLOSE)(HANDLE);
typedef DWORD (WINAPI*ECHO)(HANDLE,IPAddr,LPVOID,WORD,PIP_OPTION_INFORMATION,LPVOID,DWORD,DWORD);
void Entry(void){
 WSADATA w;SOCKET sock;struct sockaddr_in host;HDEVINFO set;SP_DEVINFO_DATA d;
 unsigned i,j,count=0,problems=0,replies=0;char*line=GetCommandLineA(),*token;
 HANDLE h,file;HMODULE lib;OPEN open;CLOSE close;ECHO echo;DWORD written,n;
 static BYTE payload[1472],buffer[4096];IP_OPTION_INFORMATION options;
 if(*line=='"'){line++;while(*line&&*line!='"')line++;if(*line)line++;}else while(*line&&*line!=' ')line++;
 while(*line==' ')line++;token=line;if(!*token)token="manual";
 add("TOKEN ");add(token);add("\r\n");
 set=SetupDiGetClassDevsA(NULL,NULL,NULL,DIGCF_ALLCLASSES|DIGCF_PRESENT);
 if(set!=INVALID_HANDLE_VALUE){for(i=0;;i++){
  char id[512],name[256];ULONG status=0,problem=0;CONFIGRET cr;
  zero(&d,sizeof(d));d.cbSize=sizeof(d);if(!SetupDiEnumDeviceInfo(set,i,&d))break;
  if(!SetupDiGetDeviceInstanceIdA(set,&d,id,sizeof(id),NULL)||!has(id,"USB\\VID_0BDA"))continue;
  count++;zero(name,sizeof(name));
  if(!SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_FRIENDLYNAME,NULL,(BYTE*)name,sizeof(name),NULL))
   SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_DEVICEDESC,NULL,(BYTE*)name,sizeof(name),NULL);
  cr=CM_Get_DevNode_Status(&status,&problem,d.DevInst,0);
  add("DEVICE ");add(id);add("\r\nNAME ");add(name);add("\r\nPROBLEM ");number(problem);add(" STATUS ");number(status);add(" CM ");number(cr);add("\r\n");
  if(cr||problem||!(status&DN_STARTED)||!has(name,"Realtek RTL"))problems++;
 }SetupDiDestroyDeviceInfoList(set);}else problems++;
 if(WSAStartup(0x0202,&w)){add("FAIL WSAStartup\r\n");goto finish;}
 lib=LoadLibraryA("icmp.dll");if(!lib)lib=LoadLibraryA("iphlpapi.dll");
 open=lib?(OPEN)GetProcAddress(lib,"IcmpCreateFile"):NULL;close=lib?(CLOSE)GetProcAddress(lib,"IcmpCloseHandle"):NULL;echo=lib?(ECHO)GetProcAddress(lib,"IcmpSendEcho"):NULL;
 if(open&&close&&echo){h=open();if(h!=INVALID_HANDLE_VALUE){
  zero(&options,sizeof(options));options.Ttl=64;options.Flags=IP_FLAG_DF;
  for(i=0;i<sizeof(payload);i++)payload[i]=(BYTE)(i*31+7);
  for(i=0;i<4;i++){
   PICMP_ECHO_REPLY r=(PICMP_ECHO_REPLY)buffer;zero(buffer,sizeof(buffer));
   n=echo(h,inet_addr("10.0.2.2"),payload,sizeof(payload),&options,buffer,sizeof(buffer),3000);
   if(n&&r->Status==IP_SUCCESS&&r->Address==inet_addr("10.0.2.2")&&r->DataSize==sizeof(payload)){
    for(j=0;j<sizeof(payload)&&((BYTE*)r->Data)[j]==payload[j];j++);
    if(j==sizeof(payload))replies++;
   }
   Sleep(200);
  }close(h);
 }}
 add("USB_DEVICES ");number(count);add(" PROBLEMS ");number(problems);add(" ICMP_REPLIES ");number(replies);add(" PAYLOAD 1472\r\n");
 add(count==1&&!problems&&replies==4?"PASS\r\n":"FAIL\r\n");
 zero(&host,sizeof(host));host.sin_family=AF_INET;host.sin_port=htons(18200);host.sin_addr.s_addr=inet_addr("10.0.2.2");
 sock=socket(AF_INET,SOCK_DGRAM,0);if(sock!=INVALID_SOCKET){sendto(sock,report,used,0,(struct sockaddr*)&host,sizeof(host));closesocket(sock);}WSACleanup();
finish:
 WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),report,used,&written,NULL);
 file=CreateFileA("C:\\RLTTEST\\native.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file!=INVALID_HANDLE_VALUE){SetFilePointer(file,0,NULL,FILE_END);WriteFile(file,report,used,&written,NULL);CloseHandle(file);}
 ExitProcess(count==1&&!problems&&replies==4?0:1);
}
