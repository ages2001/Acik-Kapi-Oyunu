#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHILDREN 7
#ifndef MIB
#define MIB 500
#endif
#define PAGES (MIB * 256UL)
typedef struct { LONG ready[CHILDREN]; LONG done[CHILDREN]; } State;
static HANDLE note, serial;
static void output(const char *s) {
    DWORD n;
    if(note!=INVALID_HANDLE_VALUE) { WriteFile(note,s,strlen(s),&n,0); FlushFileBuffers(note); }
    if(serial!=INVALID_HANDLE_VALUE) WriteFile(serial,s,strlen(s),&n,0);
}
static int child(unsigned k) {
    HANDLE m=OpenFileMapping(FILE_MAP_WRITE,FALSE,"Ram9xMap");
    HANDLE go=OpenEvent(SYNCHRONIZE,FALSE,"Ram9xRelease");
    State *state; DWORD i,tag; volatile DWORD *v;
    if(!m||!go||k>=CHILDREN) return 1;
    state=MapViewOfFile(m,FILE_MAP_WRITE,0,0,sizeof(State)); if(!state)return 1;
    v=VirtualAlloc(NULL,MIB*1048576UL,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!v){state->ready[k]=-1;return 1;}
    for(i=0;i<PAGES;i++){tag=0x31000000UL+k*PAGES+i;v[i*1024]=tag;v[i*1024+1023]=~tag;}
    InterlockedExchange(&state->ready[k],1);
    if(WaitForSingleObject(go,300000)!=WAIT_OBJECT_0)return 1;
    for(i=PAGES;i;i--){tag=0x31000000UL+k*PAGES+i-1;if(v[(i-1)*1024]!=tag||v[(i-1)*1024+1023]!=~tag){state->done[k]=-1;return 1;}}
    InterlockedExchange(&state->done[k],VirtualFree((LPVOID)v,0,MEM_RELEASE)?1:-1);
    return 0;
}
int WINAPI WinMain(HINSTANCE h,HINSTANCE p,LPSTR cmd,int show){
    HANDLE m,go,proc[CHILDREN];State *s;MEMORYSTATUS ms;OSVERSIONINFO os;char b[240],args[60];
    unsigned i,count=0;int ok=1,held=0;DWORD start;
    if(cmd[0]>='0'&&cmd[0]<='6')return child((unsigned)(cmd[0]-'0'));
    note=CreateFile("C:\\RAM9X.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
    serial=CreateFile("COM1",GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);
    output("W9X begin\r\n");
    os.dwOSVersionInfoSize=sizeof(os); GetVersionEx(&os);
    ms.dwLength=sizeof(ms);GlobalMemoryStatus(&ms);
    wsprintf(b,"Windows %lu.%lu build %lu reported physical bytes=%lu free=%lu\r\n",os.dwMajorVersion,os.dwMinorVersion,os.dwBuildNumber&65535,ms.dwTotalPhys,ms.dwAvailPhys);output(b);
    m=CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(State),"Ram9xMap");
    go=CreateEvent(NULL,TRUE,FALSE,"Ram9xRelease");
    if(!m||!go) return 2;
    s=MapViewOfFile(m,FILE_MAP_WRITE,0,0,sizeof(State));if(!s) return 2;ZeroMemory(s,sizeof(*s));
    for(i=0;i<CHILDREN;i++){
        STARTUPINFO si;PROCESS_INFORMATION pi;
        ZeroMemory(&si,sizeof(si));si.cb=sizeof(si);
        wsprintf(args,"C:\\RAM9X.EXE %u",i);
        if(!CreateProcess(NULL,args,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)){ok=0;break;}
        proc[count++]=pi.hProcess;CloseHandle(pi.hThread);
    }
    start=GetTickCount();
    while(ok && GetTickCount()-start<180000){
        for(i=0;i<count;i++){if(s->ready[i]<0){ok=0;break;}if(!s->ready[i])break;}
        if(i==count){held=1;break;}Sleep(50);
    }
    GlobalMemoryStatus(&ms);
    wsprintf(b,"Simultaneously resident target MiB=%u workers_ready=%d free bytes=%lu\r\n",CHILDREN*MIB,held,ms.dwAvailPhys);output(b);
    for(i=0;i<count;i++){wsprintf(b,"worker %u state %ld\r\n",i,s->ready[i]);output(b);}
    SetEvent(go);
    for(i=0;i<count;i++){if(WaitForSingleObject(proc[i],180000)!=WAIT_OBJECT_0||s->done[i]!=1)ok=0;CloseHandle(proc[i]);}
    output(ok&&held?"result=PASS every 4 KiB page verified\r\n":"result=FAIL\r\n");output("W9X end\r\n");
    if(serial!=INVALID_HANDLE_VALUE)CloseHandle(serial);if(note!=INVALID_HANDLE_VALUE)CloseHandle(note);
    return ok&&held?0:1;
}
