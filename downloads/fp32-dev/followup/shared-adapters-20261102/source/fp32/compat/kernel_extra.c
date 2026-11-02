#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Process-local synchronization for SLists on early NT. All accesses through
   these exports share the lock; callers must obey the normal SList lifetime
   rules (initialization before publication, no concurrent node destruction). */
static volatile LONG list_lock;
static void lock_list(void){while(InterlockedExchange(&list_lock,1))Sleep(0);}
static void unlock_list(void){InterlockedExchange(&list_lock,0);}
void WINAPI fp_InitializeSListHead(PSLIST_HEADER h){h->Next.Next=0;h->Depth=0;h->Sequence=0;}
PSLIST_ENTRY WINAPI fp_InterlockedPushEntrySList(PSLIST_HEADER h,PSLIST_ENTRY e){PSLIST_ENTRY old;lock_list();old=h->Next.Next;e->Next=old;h->Next.Next=e;h->Depth++;h->Sequence++;unlock_list();return old;}
PSLIST_ENTRY WINAPI fp_InterlockedPopEntrySList(PSLIST_HEADER h){PSLIST_ENTRY e;lock_list();e=h->Next.Next;if(e){h->Next.Next=e->Next;h->Depth--;h->Sequence++;}unlock_list();return e;}

extern LONG WINAPI fp_InterlockedCompareExchange(volatile LONG*,LONG,LONG);
static volatile LONG pointer_cookie;
static DWORD cookie(void){DWORD value=(DWORD)pointer_cookie;LARGE_INTEGER q;if(!value){q.QuadPart=0;QueryPerformanceCounter(&q);value=q.LowPart^q.HighPart^GetTickCount()^GetCurrentProcessId()^(DWORD)&pointer_cookie;value|=0xa0000000u;fp_InterlockedCompareExchange(&pointer_cookie,(LONG)value,0);value=(DWORD)pointer_cookie;}return value;}
PVOID WINAPI fp_EncodePointer(PVOID p){return (PVOID)((DWORD)p^cookie());}
PVOID WINAPI fp_DecodePointer(PVOID p){return (PVOID)((DWORD)p^cookie());}

/* CreateThread compatibility is shared in compat/thread_affinity.c. */

/* NT 3.1 exposes void-returning heap locks in NTDLL. Their return register
   is not the BOOLEAN contract used by newer NTDLL versions. */
static BOOL heap_lock_operation(HANDLE heap,LPCSTR name){
    typedef VOID (WINAPI *Native)(HANDLE);
    typedef BYTE (WINAPI *Validate)(HANDLE);
    typedef BOOL (WINAPI *Win32)(HANDLE);
    Win32 direct=(Win32)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),!lstrcmpA(name,"RtlLockHeap")?"HeapLock":"HeapUnlock");
    Native native=(Native)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),name);
    Validate validate=(Validate)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"RtlValidateHeap");
    if(direct)return direct(heap);
    if((GetVersion()&0xffff)!=0x0a03||!native||!validate){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
    if(!validate(heap)){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
    native(heap);return TRUE;
}
BOOL WINAPI fp_HeapLock(HANDLE heap){return heap_lock_operation(heap,"RtlLockHeap");}
BOOL WINAPI fp_HeapUnlock(HANDLE heap){return heap_lock_operation(heap,"RtlUnlockHeap");}

/* A 32-bit legacy OS has no WOW64 system directory. Preserve the documented
   absence result, while delegating to the native API when it is available. */
UINT WINAPI fp_GetSystemWow64DirectoryW(LPWSTR buffer,UINT capacity)
{
    typedef UINT (WINAPI *Native)(LPWSTR,UINT);
    Native native=(Native)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GetSystemWow64DirectoryW");
    if(native)return native(buffer,capacity);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

/* NT 3.1's native Enter/Leave use LockCount=-1 when free, count recursive
   acquisitions and waiters, and store the thread ID in OwningThread. Claim
   the free state atomically so native waiters use the same semaphore protocol.
   Failure must not increment LockCount: there is no waiter to wake. */
BOOL WINAPI fp_TryEnterCriticalSection(LPCRITICAL_SECTION section)
{
    if(fp_InterlockedCompareExchange(&section->LockCount,0,-1)==-1){
        section->OwningThread=(HANDLE)GetCurrentThreadId();
        section->RecursionCount=1;
        return TRUE;
    }
    if(section->OwningThread==(HANDLE)GetCurrentThreadId()){
        EnterCriticalSection(section);
        return TRUE;
    }
    return FALSE;
}
