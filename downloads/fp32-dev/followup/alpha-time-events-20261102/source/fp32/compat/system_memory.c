/* GlobalMemoryStatusEx for legacy NT, using original full-width page counts.
   GlobalMemoryStatus truncates products to DWORD and has an older load metric. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef LONG(WINAPI *SystemQuery)(ULONG,PVOID,ULONG,PULONG);
typedef LONG(WINAPI *ProcessQuery)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef ULONG(WINAPI *DosError)(LONG);
static ULONGLONG smaller(ULONGLONG a,ULONGLONG b){return a<b?a:b;}
BOOL WINAPI fp_GlobalMemoryStatusEx(LPMEMORYSTATUSEX out)
{
    typedef BOOL(WINAPI *Native)(LPMEMORYSTATUSEX);
    Native native=(Native)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GlobalMemoryStatusEx");
    HMODULE nt;SystemQuery system;ProcessQuery process;DosError error;
    DWORD basic[11],performance[78],quota[8],vm[11],length,i,available_pages;
    LONG status;MEMORYSTATUSEX result;ULONGLONG system_commit,system_free,process_limit;
    if(native)return native(out);
    if(!out||out->dwLength!=sizeof(*out)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    nt=GetModuleHandleA("NTDLL.DLL");
    system=nt?(SystemQuery)GetProcAddress(nt,"NtQuerySystemInformation"):0;
    process=nt?(ProcessQuery)GetProcAddress(nt,"NtQueryInformationProcess"):0;
    error=nt?(DosError)GetProcAddress(nt,"RtlNtStatusToDosError"):0;
    if(!system||!process||!error){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
    /* NT3.1/3.51 require these exact legacy buffer sizes. The processor count
       in basic[10] is one byte; its other bytes are unspecified and unused. */
    length=0;status=system(0,basic,sizeof(basic),&length);
    if(status<0)goto native_error;
    if(length!=sizeof(basic))goto bad_length;
    length=0;status=system(2,performance,sizeof(performance),&length);
    if(status<0)goto native_error;
    if(length!=sizeof(performance))goto bad_length;
    length=0;status=process(GetCurrentProcess(),1,quota,sizeof(quota),&length);
    if(status<0)goto native_error;
    if(length!=sizeof(quota))goto bad_length;
    length=0;status=process(GetCurrentProcess(),3,vm,sizeof(vm),&length);
    if(status<0)goto native_error;
    if(length!=sizeof(vm))goto bad_length;
    if(!basic[2]||!basic[3]||basic[8]<basic[7]||performance[11]>basic[3]){
        SetLastError(ERROR_INVALID_DATA);return FALSE;
    }
    result.dwLength=sizeof(result);
    available_pages=performance[11];
    /* ceil(100 * used / total), without floating point or a 32-bit product. */
    for(i=0;i<100&&(ULONGLONG)i*basic[3]<(ULONGLONG)(basic[3]-available_pages)*100;i++);
    result.dwMemoryLoad=i;
    result.ullTotalPhys=(ULONGLONG)basic[3]*basic[2];
    result.ullAvailPhys=(ULONGLONG)available_pages*basic[2];
    system_commit=(ULONGLONG)performance[13]*basic[2];
    system_free=performance[12]<performance[13]?
        (ULONGLONG)(performance[13]-performance[12])*basic[2]:0;
    /* QUOTA_LIMITS and VM_COUNTERS report bytes. MAXULONG denotes an
       unlimited legacy process quota; system commit still limits allocation. */
    process_limit=quota[4]==0xffffffffu?~(ULONGLONG)0:quota[4];
    result.ullTotalPageFile=smaller(system_commit,process_limit);
    result.ullAvailPageFile=smaller(system_free,process_limit>vm[9]?process_limit-vm[9]:0);
    result.ullTotalVirtual=(ULONGLONG)basic[8]-basic[7]+1;
    result.ullAvailVirtual=result.ullTotalVirtual>vm[1]?result.ullTotalVirtual-vm[1]:0;
    result.ullAvailExtendedVirtual=0; /* Reserved by MEMORYSTATUSEX. */
    *out=result;
    return TRUE;
native_error:
    SetLastError(error(status));return FALSE;
bad_length:
    SetLastError(ERROR_BAD_LENGTH);return FALSE;
}
