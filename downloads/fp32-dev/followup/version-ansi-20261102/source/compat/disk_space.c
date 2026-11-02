/* Legacy NT disk capacity without truncating 64-bit allocation-unit counts.
   Later systems retain their native quota-aware GetDiskFreeSpaceExW. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef struct { USHORT length,maximum; PWSTR buffer; } FpUnicode;
typedef struct { ULONG length; HANDLE root; FpUnicode *name; ULONG attributes;
    PVOID security,qos; } FpObjectAttributes;
typedef struct { LONG status; ULONG information; } FpIoStatus;
typedef struct { LARGE_INTEGER total,available; ULONG sectors,bytes; } FpFsSize;

BOOL WINAPI fp_SHGetDiskFreeSpaceExW(LPCWSTR directory,PULARGE_INTEGER available,
                                    PULARGE_INTEGER total,PULARGE_INTEGER free_bytes)
{
    typedef BOOL(WINAPI *Native)(LPCWSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER);
    typedef BOOLEAN(WINAPI *Path)(PCWSTR,FpUnicode*,PWSTR*,PVOID);
    typedef VOID(WINAPI *FreeString)(FpUnicode*);
    typedef LONG(WINAPI *Open)(PHANDLE,ACCESS_MASK,FpObjectAttributes*,FpIoStatus*,ULONG,ULONG);
    typedef LONG(WINAPI *Query)(HANDLE,FpIoStatus*,PVOID,ULONG,ULONG);
    typedef ULONG(WINAPI *Error)(LONG);
    Native native=(Native)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GetDiskFreeSpaceExW");
    HMODULE nt;Path path;FreeString release;Open open;Query query;Error error;
    WCHAR current[MAX_PATH];DWORD length,i;FpUnicode name={0,0,0};
    FpObjectAttributes attributes;FpIoStatus io;FpFsSize size;HANDLE handle;
    LONG status;ULONGLONG all,remaining;
    if(native)return native(directory,available,total,free_bytes);
    nt=GetModuleHandleA("NTDLL.DLL");
    if(!nt){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
    path=(Path)GetProcAddress(nt,"RtlDosPathNameToNtPathName_U");
    release=(FreeString)GetProcAddress(nt,"RtlFreeUnicodeString");
    open=(Open)GetProcAddress(nt,"NtOpenFile");
    query=(Query)GetProcAddress(nt,"NtQueryVolumeInformationFile");
    error=(Error)GetProcAddress(nt,"RtlNtStatusToDosError");
    if(!path||!release||!open||!query||!error){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
    if(!directory){
        length=GetCurrentDirectoryW(MAX_PATH,current);
        if(!length)return FALSE;
        if(length>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}
        /* NULL means the current drive's root, not a possibly removed cwd. */
        if(current[0]&&current[1]==':')current[3]=0;
        else if(current[0]=='\\'&&current[1]=='\\'){
            for(i=2;current[i]&&current[i]!='\\';i++);
            if(current[i])for(i++;current[i]&&current[i]!='\\';i++);
            if(current[i])current[i+1]=0;
        }
        directory=current;
    }
    if(!*directory){SetLastError(ERROR_PATH_NOT_FOUND);return FALSE;}
    if(!path(directory,&name,0,0)){SetLastError(ERROR_PATH_NOT_FOUND);return FALSE;}
    attributes.length=sizeof(attributes);attributes.root=0;attributes.name=&name;
    attributes.attributes=0x40; /* OBJ_CASE_INSENSITIVE */
    attributes.security=0;attributes.qos=0;
    status=open(&handle,SYNCHRONIZE|FILE_LIST_DIRECTORY,&attributes,&io,
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                0x21); /* FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT */
    release(&name);
    if(status<0){SetLastError(error(status));return FALSE;}
    size.total.QuadPart=size.available.QuadPart=0;size.sectors=size.bytes=0;
    status=query(handle,&io,&size,sizeof(size),3); /* FileFsSizeInformation */
    CloseHandle(handle);
    if(status<0){SetLastError(error(status));return FALSE;}
    if(!size.sectors||!size.bytes||size.total.QuadPart<0||size.available.QuadPart<0||
       size.available.QuadPart>size.total.QuadPart){SetLastError(ERROR_INVALID_DATA);return FALSE;}
    all=(ULONGLONG)size.total.QuadPart*size.sectors*size.bytes;
    remaining=(ULONGLONG)size.available.QuadPart*size.sectors*size.bytes;
    /* NT3.1/3.51 have no per-user volume quota. Native later-OS calls above
       retain the distinction between caller-available and volume-free space. */
    if(available)available->QuadPart=remaining;
    if(total)total->QuadPart=all;
    if(free_bytes)free_bytes->QuadPart=remaining;
    return TRUE;
}
