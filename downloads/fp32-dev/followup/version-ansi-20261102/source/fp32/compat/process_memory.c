/* NT process counters for the missing legacy PSAPI export. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

typedef struct FpVmCounters {
    SIZE_T PeakVirtualSize,VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize,WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage,QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage,QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage,PeakPagefileUsage;
} FpVmCounters;

BOOL WINAPI fp_GetProcessMemoryInfo(HANDLE process,PPROCESS_MEMORY_COUNTERS out,DWORD bytes)
{
    typedef LONG(WINAPI *Query)(HANDLE,ULONG,PVOID,ULONG,PULONG);
    typedef ULONG(WINAPI *DosError)(LONG);
    HMODULE ntdll=GetModuleHandleA("NTDLL.DLL");
    Query query=ntdll?(Query)GetProcAddress(ntdll,"NtQueryInformationProcess"):0;
    DosError error=ntdll?(DosError)GetProcAddress(ntdll,"RtlNtStatusToDosError"):0;
    FpVmCounters native;
    PROCESS_MEMORY_COUNTERS_EX result;
    ULONG returned=0;LONG status;DWORD i,size;
    if(bytes<sizeof(PROCESS_MEMORY_COUNTERS)){SetLastError(ERROR_INSUFFICIENT_BUFFER);return FALSE;}
    if(!out){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    if(!query||!error){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
    status=query(process,3,&native,sizeof(native),&returned);
    if(status<0){SetLastError(error(status));return FALSE;}
    if(returned!=sizeof(native)){SetLastError(ERROR_BAD_LENGTH);return FALSE;}
    size=bytes>=sizeof(result)?sizeof(result):sizeof(PROCESS_MEMORY_COUNTERS);
    result.cb=size;
    result.PageFaultCount=native.PageFaultCount;
    result.PeakWorkingSetSize=native.PeakWorkingSetSize;
    result.WorkingSetSize=native.WorkingSetSize;
    result.QuotaPeakPagedPoolUsage=native.QuotaPeakPagedPoolUsage;
    result.QuotaPagedPoolUsage=native.QuotaPagedPoolUsage;
    result.QuotaPeakNonPagedPoolUsage=native.QuotaPeakNonPagedPoolUsage;
    result.QuotaNonPagedPoolUsage=native.QuotaNonPagedPoolUsage;
    result.PagefileUsage=native.PagefileUsage;
    result.PeakPagefileUsage=native.PeakPagefileUsage;
    /* On the tested legacy NT kernels, PagefileUsage is private committed
       bytes, not resident bytes or bytes currently swapped to a disk file. */
    result.PrivateUsage=native.PagefileUsage;
    for(i=0;i<size;i++)((BYTE*)out)[i]=((BYTE*)&result)[i];
    return TRUE;
}
