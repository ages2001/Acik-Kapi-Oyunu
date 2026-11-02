/* Guest-side full-payload checks for three independent legacy NT interfaces.
 * Diagnostic fixture only; reports synthetic results through the PCI subnet.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>
#include <ipexport.h>
static char report[4096],token[49];static unsigned used;
static BYTE payload[1472];
static union {DWORD align;BYTE bytes[4096];} reply;
static void zero(void*p,unsigned n){volatile BYTE*b=p;while(n--)*b++=0;}
static void add(const char*s){while(*s&&used+1<sizeof(report))report[used++]=*s++;report[used]=0;}
static void number(DWORD n){char b[12];int at=10;b[11]=0;do{b[at--]='0'+n%10;n/=10;}while(n);add(b+at+1);}
typedef HANDLE (WINAPI*OPEN)(void);
typedef BOOL (WINAPI*CLOSE)(HANDLE);
typedef DWORD (WINAPI*ECHO)(HANDLE,IPAddr,LPVOID,WORD,PIP_OPTION_INFORMATION,LPVOID,DWORD,DWORD);
void Entry(void)
{
 char*line=GetCommandLineA();unsigned mask=0,i,j,path,total=0,wanted=0;DWORD seed=5381,written;
 const char*destinations[]={"10.0.2.2","10.0.3.2","10.0.4.2"};
 WSADATA w;SOCKET sock;struct sockaddr_in host;HMODULE lib;HANDLE h,file;
 OPEN open;CLOSE close;ECHO echo;IP_OPTION_INFORMATION options;int network=0;
 if(*line=='"'){line++;while(*line&&*line!='"')line++;if(*line)line++;}else while(*line&&*line!=' ')line++;
 while(*line==' ')line++;
 for(i=0;i<sizeof(token)-1&&line[i]&&line[i]!=' ';i++){
  char c=line[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))ExitProcess(2);
  token[i]=c;seed=seed*33+(BYTE)c;
 }
 if(!i||line[i]!=' ')ExitProcess(2);line+=i;while(*line==' ')line++;
 if(line[0]<'1'||line[0]>'7'||line[1])ExitProcess(2);mask=(unsigned)(line[0]-'0');
 add("TOKEN ");add(token);add("\r\nMASK ");number(mask);add("\r\n");
 for(path=0;path<3;path++)if(mask&(1U<<path))wanted+=4;
 if(WSAStartup(0x0101,&w)){add("WSAStartup FAIL\r\n");goto finish;}network=1;
 lib=LoadLibraryA("icmp.dll");
 open=lib?(OPEN)GetProcAddress(lib,"IcmpCreateFile"):NULL;
 close=lib?(CLOSE)GetProcAddress(lib,"IcmpCloseHandle"):NULL;
 echo=lib?(ECHO)GetProcAddress(lib,"IcmpSendEcho"):NULL;
 if(open&&close&&echo){h=open();if(h!=INVALID_HANDLE_VALUE){
  zero(&options,sizeof(options));options.Ttl=64;options.Flags=IP_FLAG_DF;
  for(path=0;path<3;path++)if(mask&(1U<<path)){
   IPAddr address=inet_addr(destinations[path]);unsigned matched=0;
   for(i=0;i<4;i++){
    PICMP_ECHO_REPLY r=(PICMP_ECHO_REPLY)reply.bytes;DWORD count;ULONG_PTR data;
    for(j=0;j<sizeof(payload);j++)payload[j]=(BYTE)((seed>>((j&3)*8))+j*31+i*17+path*53);
    zero(reply.bytes,sizeof(reply.bytes));
    count=echo(h,address,payload,sizeof(payload),&options,reply.bytes,sizeof(reply.bytes),2000);
    data=(ULONG_PTR)r->Data;
    if(count&&r->Status==IP_SUCCESS&&r->Address==address&&r->DataSize==sizeof(payload)&&
       data>=(ULONG_PTR)reply.bytes&&data<=(ULONG_PTR)reply.bytes+sizeof(reply.bytes)-sizeof(payload)){
     for(j=0;j<sizeof(payload)&&((BYTE*)r->Data)[j]==payload[j];j++);
     if(j==sizeof(payload)){matched++;total++;}
    }
    Sleep(100);
   }
   add("PATH ");add(destinations[path]);add(" REPLIES ");number(matched);add(" PAYLOAD 1472\r\n");
  }close(h);
 }}else add("ICMP_API FAIL\r\n");
 add("TOTAL ");number(total);add(" EXPECTED ");number(wanted);add("\r\n");
 add(total==wanted?"PASS\r\n":"FAIL\r\n");
 zero(&host,sizeof(host));host.sin_family=AF_INET;host.sin_port=htons(18202);host.sin_addr.s_addr=inet_addr("10.0.3.2");
 sock=socket(AF_INET,SOCK_DGRAM,0);if(sock!=INVALID_SOCKET){sendto(sock,report,used,0,(struct sockaddr*)&host,sizeof(host));closesocket(sock);}
finish:
 if(network)WSACleanup();
 WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),report,used,&written,NULL);
 file=CreateFileA("C:\\RLTTEST\\LNET.LOG",GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file!=INVALID_HANDLE_VALUE){SetFilePointer(file,0,NULL,FILE_END);WriteFile(file,report,used,&written,NULL);CloseHandle(file);}
 ExitProcess(network&&total==wanted?0:1);
}
