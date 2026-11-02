#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef PROXY_PATH
#define PROXY_PATH "C:\\FP32\\F32K.DLL"
#endif
#ifndef PROBE_LOG
#define PROBE_LOG "C:\\FP32\\AFFINITY.LOG"
#endif
static HANDLE logh;static DWORD checks,failures;
static void value(const char*s,DWORD n){char b[192],d[12];DWORD i=0,j=0,w;while(*s)b[i++]=*s++;do{d[j++]='0'+n%10;n/=10;}while(n);while(j)b[i++]=d[--j];b[i++]='\r';b[i++]='\n';WriteFile(logh,b,i,&w,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;value(s,ok);}
static DWORD WINAPI worker(void *unused){(void)unused;return 0x42;}
static DWORD cpu_id(void){DWORD a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));return b>>24;}
static LONG object_attributes(HANDLE handle,DWORD *attributes)
{
    typedef LONG (WINAPI *Query)(HANDLE,ULONG,PVOID,ULONG,PULONG);
    Query query=(Query)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtQueryObject");
    DWORD data[14];LONG status;
    if(!query)return -1;
    /* NT 3.1 requires the exact 56-byte basic-information structure. */
    status=query(handle,0,data,sizeof(data),0);
    value("Object basic status ",status);
    if(status>=0){*attributes=data[0];value("Object handle attributes ",*attributes);}
    return status;
}
void WINAPI entry(void)
{
    typedef HANDLE (WINAPI *Open)(DWORD,BOOL,DWORD);
    typedef BOOL (WINAPI *Process)(HANDLE,PDWORD_PTR,PDWORD_PTR);
    typedef DWORD_PTR (WINAPI *Thread)(HANDLE,DWORD_PTR);
    HMODULE dll;Open open;Process process;Thread thread;
    DWORD_PTR pm=0,sm=0,old=0,previous=0,got,first;DWORD e,tid,exitcode,bit,seen=0,cpu;
    DWORD attributes;SYSTEM_INFO info;HANDLE handle,real,created;BOOL ok;LONG status;
    logh=CreateFileA(PROBE_LOG,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
    if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
    dll=LoadLibraryA(PROXY_PATH);check(dll!=0,"DLL loaded ");
    open=dll?(Open)GetProcAddress(dll,"OpenThread"):0;
    process=dll?(Process)GetProcAddress(dll,"GetProcessAffinityMask"):0;
#ifdef FP10_THREAD_PROBE
    /* FP10 does not import GetProcessAffinityMask. Use the separately tested
       FP32 implementation as the reference for process-mask validation. */
    {HMODULE reference=LoadLibraryA("C:\\FP32\\D32K.DLL");
     process=reference?(Process)GetProcAddress(reference,"GetProcessAffinityMask"):0;}
#endif
    thread=dll?(Thread)GetProcAddress(dll,"SetThreadAffinityMask"):0;
    check(open&&process&&thread,"All exports found ");if(!open||!process||!thread)goto done;
    GetSystemInfo(&info);value("Processor count ",info.dwNumberOfProcessors);
    check(process(GetCurrentProcess(),&pm,&sm),"Current process query ");
    value("Process mask ",pm);value("System mask ",sm);
    check(pm&&!(pm&~sm)&&sm==info.dwActiveProcessorMask,"Native system and process masks ");
    first=pm&(~pm+1);if(!first)goto done;
    pm=0xa5a5a5a5;sm=0x5a5a5a5a;SetLastError(0);
    ok=process((HANDLE)0x12345678,&pm,&sm);e=GetLastError();
    check(!ok&&e==ERROR_INVALID_HANDLE,"Invalid process handle ");
    check(pm==0xa5a5a5a5&&sm==0x5a5a5a5a,"Failed query leaves output untouched ");
    real=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,GetCurrentProcessId());
    check(real&&process(real,&pm,&sm),"Real process query handle ");if(real)CloseHandle(real);
    real=OpenProcess(SYNCHRONIZE,FALSE,GetCurrentProcessId());SetLastError(0);
    ok=real&&process(real,&pm,&sm);e=GetLastError();if(real)CloseHandle(real);
    check(real&&!ok&&e==ERROR_ACCESS_DENIED,"Process query rights enforced ");
    process(GetCurrentProcess(),&pm,&sm);
    old=thread(GetCurrentThread(),first);check(old!=0,"Pin current thread ");
    if(old){previous=first;
        for(bit=0;bit<32;bit++)if(pm&(1u<<bit)){
            got=thread(GetCurrentThread(),1u<<bit);
            check(got==previous,"Previous affinity returned ");previous=1u<<bit;
            cpu=cpu_id();value("Pinned APIC ID ",cpu);
            /* This test lab uses QEMU flat APIC IDs 0..N-1. */
            check(cpu==bit,"Execution moved to selected QEMU CPU ");if(cpu<32)seen|=1u<<cpu;
        }
        check(seen==pm,"All allowed QEMU CPUs exercised ");
        SetLastError(0);got=thread(GetCurrentThread(),0);e=GetLastError();
        check(!got&&e==ERROR_INVALID_PARAMETER,"Zero affinity rejected ");
        got=thread(GetCurrentThread(),previous);check(got==previous,"Rejected mask preserves affinity ");
        if(sm!=0xffffffffu){DWORD invalid=(~sm)&(sm+1);SetLastError(0);got=thread(GetCurrentThread(),invalid);e=GetLastError();check(!got&&e==ERROR_INVALID_PARAMETER,"Unavailable CPU rejected ");}
        check(thread(GetCurrentThread(),old)==previous,"Original affinity restored ");old=0;
    }
    SetLastError(0);got=thread((HANDLE)0x12345678,first);e=GetLastError();check(!got&&e==ERROR_INVALID_HANDLE,"Invalid thread handle ");
    SetLastError(0);handle=open(THREAD_QUERY_INFORMATION,FALSE,0xffffffffu);e=GetLastError();
    check(!handle&&e==ERROR_INVALID_PARAMETER,"Invalid thread ID ");if(handle)CloseHandle(handle);
    created=CreateThread(0,0,worker,0,CREATE_SUSPENDED,&tid);check(created!=0,"Suspended worker created ");
    if(created){
        handle=open(THREAD_QUERY_INFORMATION|THREAD_SET_INFORMATION|SYNCHRONIZE,TRUE,tid);
        check(handle!=0,"Open worker by thread ID ");
        if(handle){
            status=object_attributes(handle,&attributes);
            check(status>=0&&(attributes&2),"Inheritable handle attribute ");
            check(thread(handle,first)!=0,"Set affinity through opened handle ");
            CloseHandle(handle);
        }
        handle=open(THREAD_QUERY_INFORMATION,FALSE,tid);
        check(handle!=0,"Open query-only worker handle ");
        if(handle){
            status=object_attributes(handle,&attributes);
            check(status>=0&&!(attributes&2),"Non-inheritable handle attribute ");
            SetLastError(0);got=thread(handle,first);e=GetLastError();check(!got&&e==ERROR_ACCESS_DENIED,"Thread set rights enforced ");CloseHandle(handle);
        }
        ResumeThread(created);check(WaitForSingleObject(created,5000)==WAIT_OBJECT_0,"Worker completed ");
        check(GetExitCodeThread(created,&exitcode)&&exitcode==0x42,"Worker exit code preserved ");CloseHandle(created);
    }
done:
    if(old&&thread)thread(GetCurrentThread(),old);
    value("Checks ",checks);value("Failures ",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
