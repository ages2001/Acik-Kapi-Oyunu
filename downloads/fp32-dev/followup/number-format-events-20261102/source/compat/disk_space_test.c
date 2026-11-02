/* Native integration probe for the linked shell adapter. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef FP10_DISK_TEST
#define DLL "C:\\FP32\\D10S.DLL"
#define LOG "C:\\FP32\\DSK10.LOG"
#else
#define DLL "C:\\FP32\\D32S.DLL"
#define LOG "C:\\FP32\\DSK32.LOG"
#endif
static HANDLE logh;static DWORD checks,failures;static BYTE block[65536];
static void number(const char*s,DWORD n){char b[192];DWORD w;wsprintfA(b,"%s%lu\r\n",s,n);WriteFile(logh,b,lstrlenA(b),&w,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;number(s,ok);}
void WINAPI entry(void){
 typedef BOOL(WINAPI *Query)(LPCWSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER);
 HMODULE dll;Query query;ULARGE_INTEGER available,total,free_bytes,other,used,before,after;
 DWORD sectors,bytes,free_units,all_units,n,e;ULONGLONG expected;BOOL ok;HANDLE f;
 logh=CreateFileA(LOG,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 dll=LoadLibraryA(DLL);check(dll!=0,"Linked DLL loaded ");query=dll?(Query)GetProcAddress(dll,"SHGetDiskFreeSpaceExW"):0;check(query!=0,"Export resolved ");if(!query)goto done;
 check(query(L"C:\\",&available,&total,&free_bytes),"Root query ");
 check(total.QuadPart&&available.QuadPart<=total.QuadPart&&available.QuadPart==free_bytes.QuadPart,"Legacy no-quota counters ");
 ok=GetDiskFreeSpaceW(L"C:\\",&sectors,&bytes,&free_units,&all_units);check(ok,"Independent native cluster query ");
 if(ok){expected=(ULONGLONG)all_units*sectors*bytes;check(total.QuadPart==expected,"Total matches native allocation units ");expected=(ULONGLONG)free_units*sectors*bytes;check(available.QuadPart<=expected+1048576&&expected<=available.QuadPart+1048576,"Free bytes match live native query ");}
 check(query(L"C:\\FP32",0,&other,0)&&other.QuadPart==total.QuadPart,"Existing subdirectory without trailing slash ");
 check(query(L"C:\\FP32\\",0,&other,0)&&other.QuadPart==total.QuadPart,"Existing subdirectory with trailing slash ");
 check(query(L"C:\\",0,0,0),"All optional outputs omitted ");
 check(query(L"C:\\",&other,0,0)&&other.QuadPart<=total.QuadPart,"Available output alone ");
 check(query(L"C:\\",0,0,&other)&&other.QuadPart<=total.QuadPart,"Free output alone ");
 check(SetCurrentDirectoryA("C:\\FP32"),"Select known current drive ");
 check(query(0,0,&other,0)&&other.QuadPart==total.QuadPart,"NULL uses current drive ");
 check(query(L".",0,&other,0)&&other.QuadPart==total.QuadPart,"Relative directory ");
 available.QuadPart=0x1122334455667788ULL;total.QuadPart=0x8877665544332211ULL;free_bytes.QuadPart=0x123456789abcdef0ULL;
 /* Use a legal 8.3 component: NT3.1 FAT rejects long names before lookup. */
 SetLastError(0);ok=query(L"C:\\NOSUCHZZ",&available,&total,&free_bytes);e=GetLastError();
 number("Missing directory native error ",e);
 check(!ok&&(e==ERROR_PATH_NOT_FOUND||e==ERROR_FILE_NOT_FOUND),"Missing directory fails ");
 check(available.QuadPart==0x1122334455667788ULL&&total.QuadPart==0x8877665544332211ULL&&free_bytes.QuadPart==0x123456789abcdef0ULL,"Failed query leaves all outputs unchanged ");
 SetLastError(0);ok=query(L"",0,0,0);check(!ok&&GetLastError()==ERROR_PATH_NOT_FOUND,"Empty path rejected ");
 query(L"C:\\",&before,0,0);
 f=CreateFileA("C:\\FP32\\DSKPROBE.TMP",GENERIC_WRITE,0,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
 check(f!=INVALID_HANDLE_VALUE,"Fresh regular file created ");
 if(f!=INVALID_HANDLE_VALUE){
  check(WriteFile(f,block,sizeof(block),&n,0)&&n==sizeof(block)&&FlushFileBuffers(f),"Real 64 KiB allocation ");CloseHandle(f);
  check(query(L"C:\\",&used,0,0)&&used.QuadPart+sizeof(block)<=before.QuadPart,"Allocated file reduces free space ");
  SetLastError(0);ok=query(L"C:\\FP32\\DSKPROBE.TMP",0,0,0);e=GetLastError();check(!ok&&e==ERROR_DIRECTORY,"Regular file is not a directory ");
  check(DeleteFileA("C:\\FP32\\DSKPROBE.TMP"),"Test file removed ");
  check(query(L"C:\\",&after,0,0)&&after.QuadPart>=used.QuadPart+sizeof(block),"Deletion releases allocation ");
 }
done:
 number("Checks ",checks);number("Failures ",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
