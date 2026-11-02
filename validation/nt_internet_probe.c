/* Winsock 1.1 DNS/TCP/HTTP runtime probe for NT3.1 and later. No CRT or TLS.
   The endpoint is a public Debian README; no credentials or guest data leave.
   This tests network transport, not compatibility with modern web browsers. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>
static unsigned length(const char*s){unsigned n=0;while(s[n])n++;return n;}
static void print(const char*s){DWORD done;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,length(s),&done,NULL);}
static void number(DWORD n){char b[12];int at=10;b[11]=0;do{b[at--]=(char)('0'+n%10);n/=10;}while(n);print(b+at+1);}
static void fail(const char*s){print("FAIL ");print(s);print(" WSA=");number(WSAGetLastError());print("\r\n");ExitProcess(1);}
static void resolver_settings(void){
 HKEY key;char data[256];DWORD type,n;LONG status;unsigned i;
 const char*names[]={"NameServer","DNSLookupOrder","Hostname","Domain"};
 status=RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",0,KEY_READ,&key);
 if(status){print("Resolver key error ");number(status);print("\r\n");return;}
 for(i=0;i<4;i++){
  n=sizeof(data);type=0;status=RegQueryValueExA(key,names[i],0,&type,(BYTE*)data,&n);
  print(names[i]);print(": ");
  if(status){print("registry error ");number(status);}
  else if(type==REG_DWORD&&n==4)number(*(DWORD*)data);
  else if(type==REG_SZ){data[255]=0;print(data);}
  else{print("type ");number(type);}
  print("\r\n");
 }
 RegCloseKey(key);
}
static int ready(SOCKET s,int writing){fd_set f,e;struct timeval t={15,0};FD_ZERO(&f);FD_ZERO(&e);FD_SET(s,&f);FD_SET(s,&e);return select(0,writing?NULL:&f,writing?&f:NULL,&e,&t)>0&&!FD_ISSET(s,&e);}
void mainCRTStartup(void){
 WSADATA w;struct hostent*h;SOCKET s;struct sockaddr_in address;u_long nonblocking=1;int n,offset=0,total=0,i,error=0,sz=sizeof(error);
 static char response[8192];
 const char*request="GET /debian/README HTTP/1.0\r\nHost: deb.debian.org\r\nConnection: close\r\n\r\n";
 int proxy=0;char*arg=GetCommandLineA();for(i=0;arg[i];i++)if(arg[i]=='/'&&arg[i+1]=='p'){proxy=1;break;}
 print("RLTKLANT Internet transport probe (Winsock 1.1)\r\n");
 resolver_settings();
 if(WSAStartup(0x0101,&w))fail("WSAStartup");
 if(proxy){
  address.sin_family=AF_INET;address.sin_port=htons(18080);address.sin_addr.s_addr=inet_addr("10.0.2.2");
  request="GET http://deb.debian.org/debian/README HTTP/1.0\r\nHost: deb.debian.org\r\nConnection: close\r\n\r\n";
  print("Proxy mode: DNS performed by proxy, not guest\r\n");
 }else{
 h=gethostbyname("deb.debian.org");if(!h||h->h_length!=4)fail("DNS");
 address.sin_family=AF_INET;address.sin_port=htons(80);
 for(i=0;i<4;i++)((char*)&address.sin_addr)[i]=h->h_addr_list[0][i];
 print("DNS deb.debian.org -> ");print(inet_ntoa(address.sin_addr));print("\r\n");
 }
 s=socket(AF_INET,SOCK_STREAM,0);if(s==INVALID_SOCKET)fail("socket");
 if(ioctlsocket(s,FIONBIO,&nonblocking))fail("nonblocking");
 n=connect(s,(struct sockaddr*)&address,sizeof(address));
 if(n&&WSAGetLastError()!=WSAEWOULDBLOCK)fail("connect");
 if(!ready(s,1)||getsockopt(s,SOL_SOCKET,SO_ERROR,(char*)&error,&sz)||error)fail("TCP connection");
 print(proxy?"TCP to configured HTTP proxy connected\r\n":"TCP port 80 connected\r\n");
 while(offset<(int)length(request)){
  if(!ready(s,1))fail("send timeout");n=send(s,request+offset,length(request)-offset,0);if(n<=0)fail("send");offset+=n;
 }
 while(total<(int)sizeof(response)-1){
  if(!ready(s,0))fail("receive timeout");n=recv(s,response+total,sizeof(response)-1-total,0);if(n<0)fail("recv");if(!n)break;total+=n;
 }
 response[total]=0;closesocket(s);WSACleanup();
 for(i=0;i<total&&response[i]!='\r'&&response[i]!='\n';i++);response[i]=0;
 print(response);print("\r\nReceived bytes: ");number(total);print("\r\n");
 if(i<12||response[9]!='2'||response[10]!='0'||response[11]!='0'){print("FAIL HTTP status\r\n");ExitProcess(2);}
 if(total<1000){print("FAIL incomplete README response\r\n");ExitProcess(3);}
 print(proxy?"PASS TCP + public HTTP 200 through proxy\r\n":"PASS DNS + outbound TCP + public HTTP 200 response\r\n");ExitProcess(0);
}
