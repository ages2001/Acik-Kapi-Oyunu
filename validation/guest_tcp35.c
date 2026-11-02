/* NT3.51 disposable Winsock 1.1 TCP payload/stream diagnostic.
 * Explicit PCI fixture route; not an Internet reachability claim. */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <winsock.h>
static unsigned char buffer[4096];
static HANDLE log_file;
static void out(const char*s){DWORD n;unsigned len=lstrlenA(s);WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,len,&n,0);if(log_file!=INVALID_HANDLE_VALUE)WriteFile(log_file,s,len,&n,0);}
static void number(unsigned n){char b[12];int i=10;b[11]=0;do{b[i--]=(char)('0'+n%10);n/=10;}while(n);out(b+i+1);}
static int ready(SOCKET s,int writing){fd_set f;struct timeval tv;FD_ZERO(&f);FD_SET(s,&f);tv.tv_sec=10;tv.tv_usec=0;return select(0,writing?0:&f,writing?&f:0,0,&tv)==1;}
static int transfer(SOCKET s,char*p,unsigned n,int writing){while(n){int done;if(!ready(s,writing))return 0;done=writing?send(s,p,n,0):recv(s,p,n,0);if(done<=0)return 0;p+=done;n-=(unsigned)done;}return 1;}
void Entry(void){WSADATA w;SOCKET s=INVALID_SOCKET;struct sockaddr_in peer,local;unsigned round,offset,i;int local_len=sizeof(local),ok=0;
 char header[8]={'R','T','L','T','C','P','1','6'},finish[8];
 log_file=CreateFileA("C:\\RLTTEST\\TCP35.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 if(WSAStartup(0x0101,&w)){out("FAIL WSAStartup\r\n");goto exit;}
 s=socket(AF_INET,SOCK_STREAM,0);if(s==INVALID_SOCKET)goto fail;
 peer.sin_family=AF_INET;peer.sin_port=htons(18203);peer.sin_addr.s_addr=inet_addr("10.0.3.2");
 local.sin_family=AF_INET;local.sin_port=0;local.sin_addr.s_addr=inet_addr("10.0.3.15");
 if(bind(s,(struct sockaddr*)&local,sizeof(local))||connect(s,(struct sockaddr*)&peer,sizeof(peer)))goto fail;
 if(getsockname(s,(struct sockaddr*)&local,&local_len)||local.sin_addr.s_addr!=inet_addr("10.0.3.15"))goto fail;
 out("TCP PCI 10.0.3.15 -> 10.0.3.2:18203\r\n");
 if(!transfer(s,header,sizeof(header),1))goto fail;
 for(round=0;round<16;round++){
  for(offset=0;offset<65536;offset+=sizeof(buffer)){
   for(i=0;i<sizeof(buffer);i++)buffer[i]=(unsigned char)((offset+i)*29+round*7+11);
   if(!transfer(s,(char*)buffer,sizeof(buffer),1))goto fail;
  }
  for(offset=0;offset<65536;offset+=sizeof(buffer)){
   if(!transfer(s,(char*)buffer,sizeof(buffer),0))goto fail;
   for(i=0;i<sizeof(buffer);i++)if(buffer[i]!=(unsigned char)((offset+i)*17+round*11+3))goto fail;
  }
  out("ROUND ");number(round+1);out(" TX 65536 RX 65536 MATCH\r\n");
 }
 if(!transfer(s,finish,sizeof(finish),0))goto fail;
 for(i=0;i<8;i++)if(finish[i]!="TCP_PASS"[i])goto fail;
 out("PASS TCP TX 1048576 RX 1048576 ALL BYTES MATCH\r\n");ok=1;goto done;
fail:out("FAIL TCP SOCKET_ERROR ");number(WSAGetLastError());out("\r\n");
done:if(s!=INVALID_SOCKET)closesocket(s);WSACleanup();
exit:if(log_file!=INVALID_HANDLE_VALUE)CloseHandle(log_file);ExitProcess(ok?0:1);
}
