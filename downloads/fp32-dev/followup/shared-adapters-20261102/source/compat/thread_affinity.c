/* Legacy NT thread handles and processor affinity. Never invent a CPU mask. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef LONG (WINAPI *QueryInfo)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef LONG (WINAPI *SetInfo)(HANDLE,ULONG,PVOID,ULONG);
typedef ULONG (WINAPI *DosError)(LONG);
typedef struct { LONG status; PVOID peb; ULONG affinity; LONG priority;
    ULONG pid,parent; } FpProcessBasic;
typedef struct { LONG status; PVOID teb; HANDLE process,thread;
    ULONG affinity; LONG priority,base_priority; } FpThreadBasic;
typedef struct { ULONG length; HANDLE root; PVOID name; ULONG attributes;
    PVOID security,qos; } FpObjectAttributes;
typedef struct { HANDLE process,thread; } FpClientId;

static FARPROC native_kernel(const char *name)
{ return GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),name); }
static FARPROC native_nt(const char *name)
{ HMODULE dll=GetModuleHandleA("NTDLL.DLL");return dll?GetProcAddress(dll,name):0; }
static void nt_error(LONG status)
{ DosError error=(DosError)native_nt("RtlNtStatusToDosError");
  SetLastError(error?error(status):ERROR_CALL_NOT_IMPLEMENTED); }

HANDLE WINAPI fp_OpenThread(DWORD access,BOOL inherit,DWORD id)
{
    typedef HANDLE (WINAPI *Direct)(DWORD,BOOL,DWORD);
    typedef LONG (WINAPI *Open)(PHANDLE,ACCESS_MASK,FpObjectAttributes*,FpClientId*);
    Direct direct=(Direct)native_kernel("OpenThread");
    Open open;FpObjectAttributes attributes;FpClientId client;HANDLE thread=0;LONG status;
    if(direct)return direct(access,inherit,id);
    open=(Open)native_nt("NtOpenThread");
    if(!open){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return 0;}
    attributes.length=sizeof(attributes);attributes.root=0;attributes.name=0;
    attributes.attributes=inherit?2:0; /* OBJ_INHERIT */
    attributes.security=0;attributes.qos=0;
    client.process=0;client.thread=(HANDLE)id;
    status=open(&thread,access,&attributes,&client);
    if(status<0){nt_error(status);return 0;}
    return thread;
}

BOOL WINAPI fp_GetProcessAffinityMask(HANDLE process,PDWORD_PTR process_mask,PDWORD_PTR system_mask)
{
    typedef BOOL (WINAPI *Direct)(HANDLE,PDWORD_PTR,PDWORD_PTR);
    Direct direct=(Direct)native_kernel("GetProcessAffinityMask");
    QueryInfo query;FpProcessBasic basic;SYSTEM_INFO system;LONG status;ULONG returned=0;
    if(direct)return direct(process,process_mask,system_mask);
    if(!process_mask||!system_mask){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    query=(QueryInfo)native_nt("NtQueryInformationProcess");
    if(!query){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
    status=query(process,0,&basic,sizeof(basic),&returned);
    if(status<0){nt_error(status);return FALSE;}
    if(returned!=sizeof(basic)){SetLastError(ERROR_BAD_LENGTH);return FALSE;}
    GetSystemInfo(&system);
    *process_mask=basic.affinity;*system_mask=system.dwActiveProcessorMask;
    return TRUE;
}

DWORD_PTR WINAPI fp_SetThreadAffinityMask(HANDLE thread,DWORD_PTR mask)
{
    typedef DWORD_PTR (WINAPI *Direct)(HANDLE,DWORD_PTR);
    Direct direct=(Direct)native_kernel("SetThreadAffinityMask");
    QueryInfo query;SetInfo set;FpThreadBasic basic;LONG status;ULONG returned=0;
    if(direct)return direct(thread,mask);
    query=(QueryInfo)native_nt("NtQueryInformationThread");
    set=(SetInfo)native_nt("NtSetInformationThread");
    if(!query||!set){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return 0;}
    status=query(thread,0,&basic,sizeof(basic),&returned);
    if(status<0){nt_error(status);return 0;}
    if(returned!=sizeof(basic)){SetLastError(ERROR_BAD_LENGTH);return 0;}
    status=set(thread,4,&mask,sizeof(mask));
    if(status<0){nt_error(status);return 0;}
    return basic.affinity;
}

/* NT3.1 writes the thread ID even when a modern caller passes NULL. */
HANDLE WINAPI fp_CreateThread(LPSECURITY_ATTRIBUTES attributes,SIZE_T stack,
    LPTHREAD_START_ROUTINE start,LPVOID argument,DWORD flags,LPDWORD id)
{
    DWORD local_id;
    return CreateThread(attributes,stack,start,argument,flags,id?id:&local_id);
}
