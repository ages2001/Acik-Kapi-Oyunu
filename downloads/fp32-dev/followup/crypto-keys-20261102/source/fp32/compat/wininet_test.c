/* Real loopback HTTP through the original WinInet and the production proxy. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>
#include <wininet.h>
static HINTERNET(WINAPI *open_internet)(LPCSTR,DWORD,LPCSTR,LPCSTR,DWORD);
static HINTERNET(WINAPI *connect_internet)(HINTERNET,LPCSTR,INTERNET_PORT,LPCSTR,LPCSTR,DWORD,DWORD,DWORD_PTR);
static HINTERNET(WINAPI *open_request)(HINTERNET,LPCSTR,LPCSTR,LPCSTR,LPCSTR,LPCSTR*,DWORD,DWORD_PTR);
static BOOL(WINAPI *send_request)(HINTERNET,LPCSTR,DWORD,LPVOID,DWORD);
static BOOL(WINAPI *query_info)(HINTERNET,DWORD,LPVOID,LPDWORD,LPDWORD);
static BOOL(WINAPI *read_internet)(HINTERNET,LPVOID,DWORD,LPDWORD);
static BOOL(WINAPI *close_internet)(HINTERNET);
static SOCKET listener=INVALID_SOCKET;static volatile DWORD server_errors,server_requests;
static HANDLE logh;static DWORD checks,failures;
static const BYTE binary_reply[]={0,1,0xff,'F','P',0,'X'};
static const BYTE post_body[]={'1',0,0xff,'X'};
static void number(const char*s,DWORD v){char b[180];DWORD n;wsprintfA(b,"%s %lu\r\n",s,v);WriteFile(logh,b,lstrlenA(b),&n,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok){failures++;number("Last error",GetLastError());}number(s,ok);}
static BOOL equal(const BYTE*a,const BYTE*b,DWORD n){while(n--)if(*a++!=*b++)return FALSE;return TRUE;}
static BOOL contains(const char*s,const char*part){DWORD i,j;for(i=0;s[i];i++){for(j=0;part[j]&&s[i+j]==part[j];j++){}if(!part[j])return TRUE;}return FALSE;}
static BOOL readable(SOCKET s){fd_set set;struct timeval timeout;FD_ZERO(&set);FD_SET(s,&set);timeout.tv_sec=15;timeout.tv_usec=0;return select(0,&set,0,0,&timeout)==1;}
static BOOL transmit(SOCKET s,const BYTE*b,DWORD n){int sent;while(n){sent=send(s,(const char*)b,n,0);if(sent<=0)return FALSE;b+=sent;n-=sent;}return TRUE;}
static DWORD WINAPI server(void*unused){DWORD iteration,n,header,i,required;int got;SOCKET client;static char request[4096];const char*response;
 (void)unused;
 for(iteration=0;iteration<3;iteration++){
  if(!readable(listener)){server_errors++;break;}client=accept(listener,0,0);if(client==INVALID_SOCKET){server_errors++;break;}
  n=header=0;
  while(n<sizeof(request)-1&&!header){if(!readable(client))break;got=recv(client,request+n,sizeof(request)-1-n,0);if(got<=0)break;n+=got;request[n]=0;for(i=0;i+3<n;i++)if(request[i]=='\r'&&request[i+1]=='\n'&&request[i+2]=='\r'&&request[i+3]=='\n'){header=i+4;break;}}
  if(!header){server_errors++;closesocket(client);break;}
  required=header+(iteration==2?sizeof(post_body):0);
  while(n<required){if(!readable(client))break;got=recv(client,request+n,sizeof(request)-1-n,0);if(got<=0)break;n+=got;request[n]=0;}
  if(!contains(request,"X-FP32-Test: yes"))server_errors++;
  if(iteration==0){if(!contains(request,"GET /ok "))server_errors++;response="HTTP/1.0 200 OK\r\nContent-Length: 13\r\nContent-Type: text/plain\r\nX-FP32-Reply: native\r\nConnection: close\r\n\r\nFlash works!\n";}
  else if(iteration==1){if(!contains(request,"GET /missing "))server_errors++;response="HTTP/1.0 404 Not Found\r\nContent-Length: 5\r\nConnection: close\r\n\r\nnope!";}
  else{if(!contains(request,"POST /post ")||n<required||!equal((BYTE*)request+header,post_body,sizeof(post_body)))server_errors++;response="HTTP/1.0 201 Created\r\nContent-Length: 7\r\nConnection: close\r\n\r\n";}
  if(!transmit(client,(const BYTE*)response,lstrlenA(response)))server_errors++;
  if(iteration==2&&!transmit(client,binary_reply,sizeof(binary_reply)))server_errors++;
  shutdown(client,1);closesocket(client);server_requests++;
 }
 return server_errors;
}
static void exchange(HINTERNET connection,LPCSTR method,LPCSTR path,DWORD expected_status,const BYTE*body,DWORD body_bytes,const BYTE*reply,DWORD reply_bytes)
{
 LPCSTR accept_types[]={"*/*",0};HINTERNET request;BYTE data[64];DWORD n,status=0,index=0,total=0;BOOL ok;
 request=open_request(connection,method,path,"HTTP/1.0",0,accept_types,INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE,0);
 check(request!=0,"Open HTTP request");if(!request)return;
 check(send_request(request,"X-FP32-Test: yes\r\n",(DWORD)-1,(LPVOID)body,body_bytes),"Send HTTP request");
 n=sizeof(status);check(query_info(request,HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,&status,&n,&index)&&status==expected_status,"HTTP status matches server");
 n=sizeof(status);status=0;index=0;check(query_info(request,HTTP_QUERY_CONTENT_LENGTH|HTTP_QUERY_FLAG_NUMBER,&status,&n,&index)&&status==reply_bytes,"HTTP content length matches server");
 if(expected_status==200){lstrcpyA((char*)data,"X-FP32-Reply");n=sizeof(data);index=0;check(query_info(request,HTTP_QUERY_CUSTOM,data,&n,&index)&&!lstrcmpA((char*)data,"native"),"Custom response header");}
 ok=TRUE;for(;;){n=0;if(!read_internet(request,data+total,4,&n)){ok=FALSE;break;}if(!n)break;total+=n;if(total+4>sizeof(data)){ok=FALSE;break;}}
 check(ok&&total==reply_bytes&&equal(data,reply,total),"Streamed response bytes match exactly");
 n=123;check(read_internet(request,data,4,&n)&&n==0,"End of stream returns zero bytes");
 check(close_internet(request),"Close request handle");
}
#define LOAD(variable,name) variable=(void*)GetProcAddress(module,name);check(variable!=0,name);if(!variable)goto done
void WINAPI entry(void){HMODULE module,bridge;WSADATA sockets;struct sockaddr_in address;int address_length;HANDLE thread=0;DWORD tid,exitcode,n;HINTERNET session=0,connection=0;SYSTEMTIME time;char text[64];
 typedef int(WINAPI *Format)(LCID,DWORD,const SYSTEMTIME*,LPCSTR,LPSTR,int);Format date_format,time_format;
 logh=CreateFileA("C:\\FP32\\INET.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 module=LoadLibraryA("C:\\FP32\\F32I.DLL");check(module!=0,"Production Internet proxy loads");if(!module)goto done;
 LOAD(open_internet,"InternetOpenA");LOAD(connect_internet,"InternetConnectA");LOAD(open_request,"HttpOpenRequestA");LOAD(send_request,"HttpSendRequestA");LOAD(query_info,"HttpQueryInfoA");LOAD(read_internet,"InternetReadFile");LOAD(close_internet,"InternetCloseHandle");
 bridge=LoadLibraryA("C:\\FP32\\FPIK.DLL");date_format=bridge?(Format)GetProcAddress(bridge,"GetDateFormatA"):0;time_format=bridge?(Format)GetProcAddress(bridge,"GetTimeFormatA"):0;
 check(date_format&&time_format,"ANSI format adapters resolve");
 if(date_format&&time_format){for(n=0;n<sizeof(time);n++)((BYTE*)&time)[n]=0;time.wYear=2026;time.wMonth=11;time.wDay=2;time.wHour=13;time.wMinute=5;time.wSecond=9;
  check(date_format(0x409,0,&time,"yyyy'-'MM'-'dd",text,sizeof(text))==11&&!lstrcmpA(text,"2026-11-02"),"Native date format through ANSI adapter");
  {WCHAR native_text[64];number("Native time query length",GetTimeFormatW(0x409,0,&time,L"HH':'mm':'ss",0,0));number("Native time query error",GetLastError());number("Native time direct length",GetTimeFormatW(0x409,0,&time,L"HH':'mm':'ss",native_text,64));number("Native time direct error",GetLastError());}
  check(time_format(0x409,0,&time,"HH':'mm':'ss",text,sizeof(text))==9&&!lstrcmpA(text,"13:05:09"),"Native time format through ANSI adapter");
  check(date_format(0x409,0,&time,"yyyy'-'MM'-'dd",0,0)==11,"ANSI format size query");
  text[0]='Q';check(!date_format(0x409,0,&time,"yyyy'-'MM'-'dd",text,4)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER&&text[0]=='Q',"Short ANSI format output unchanged");
 }
 check(WSAStartup(0x101,&sockets)==0,"Native Winsock startup");
 listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listener==INVALID_SOCKET)number("Winsock socket error",WSAGetLastError());check(listener!=INVALID_SOCKET,"Create loopback listener");if(listener==INVALID_SOCKET)goto done;
 for(n=0;n<sizeof(address);n++)((BYTE*)&address)[n]=0;address.sin_family=AF_INET;address.sin_addr.s_addr=inet_addr("127.0.0.1");
 check(bind(listener,(struct sockaddr*)&address,sizeof(address))==0&&listen(listener,3)==0,"Bind and listen on loopback only");
 address_length=sizeof(address);check(getsockname(listener,(struct sockaddr*)&address,&address_length)==0,"Read assigned local port");
 thread=CreateThread(0,0,server,0,0,&tid);check(thread!=0,"Start independent native HTTP server");if(!thread)goto cleanup;
 session=open_internet("FP32 compatibility probe",INTERNET_OPEN_TYPE_DIRECT,0,0,0);check(session!=0,"Original InternetOpenA");if(!session)goto cleanup;
 connection=connect_internet(session,"127.0.0.1",ntohs(address.sin_port),0,0,INTERNET_SERVICE_HTTP,0,0);check(connection!=0,"Original InternetConnectA");if(!connection)goto cleanup;
 exchange(connection,"GET","/ok",200,0,0,(const BYTE*)"Flash works!\n",13);
 exchange(connection,"GET","/missing",404,0,0,(const BYTE*)"nope!",5);
 exchange(connection,"POST","/post",201,post_body,sizeof(post_body),binary_reply,sizeof(binary_reply));
 check(WaitForSingleObject(thread,20000)==WAIT_OBJECT_0,"Native server completes");check(GetExitCodeThread(thread,&exitcode)&&!exitcode&&!server_errors&&server_requests==3,"Server verified all requests and binary payload");
cleanup:
 if(connection)check(close_internet(connection),"Close connection handle");if(session)check(close_internet(session),"Close session handle");
 if(listener!=INVALID_SOCKET){closesocket(listener);listener=INVALID_SOCKET;}if(thread){WaitForSingleObject(thread,20000);CloseHandle(thread);}WSACleanup();
done:number("Checks",checks);number("Failures",failures);number("Server errors",server_errors);number("Server requests",server_requests);CloseHandle(logh);ExitProcess(failures?2:0);
}
