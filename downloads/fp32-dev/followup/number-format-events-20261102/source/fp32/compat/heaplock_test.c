/* Test the actual proxy lock against allocation on another native thread. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE heap,entered,finished;
static volatile LONG worker_error;
static DWORD checks,errors;
static char report[2048];
static void check(BOOL condition){checks++;if(!condition)errors++;}
static DWORD WINAPI worker(void*unused){BYTE*p;SetEvent(entered);p=HeapAlloc(heap,0,64);if(!p)worker_error=1;else{p[0]=0x5a;if(!HeapFree(heap,0,p))worker_error=2;}SetEvent(finished);return 0;}
void WINAPI entry(void){typedef BOOL (WINAPI *Lock)(HANDLE);HMODULE m=LoadLibraryA("D:\\F32K.DLL");Lock lock=m?(Lock)GetProcAddress(m,"HeapLock"):0,unlock=m?(Lock)GetProcAddress(m,"HeapUnlock"):0;HANDLE thread=0,file;DWORD tid,n;BYTE*p;
 check(lock&&unlock);if(lock&&unlock){heap=HeapCreate(0,4096,0);entered=CreateEventA(0,TRUE,FALSE,0);finished=CreateEventA(0,TRUE,FALSE,0);check(heap&&entered&&finished);if(heap&&entered&&finished){
 check(lock(heap));check(lock(heap));p=HeapAlloc(heap,0,64);check(p!=0);if(p)check(HeapFree(heap,0,p));
 thread=CreateThread(0,0,worker,0,0,&tid);check(thread!=0);if(thread){check(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);check(WaitForSingleObject(finished,100)==WAIT_TIMEOUT);}
 check(unlock(heap));if(thread)check(WaitForSingleObject(finished,100)==WAIT_TIMEOUT);
 check(unlock(heap));if(thread){check(WaitForSingleObject(finished,5000)==WAIT_OBJECT_0);check(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);check(!worker_error);CloseHandle(thread);}
 check(HeapDestroy(heap));CloseHandle(entered);CloseHandle(finished);}}
 wsprintfA(report,"Heap lock proxy test\r\nChecks: %lu\r\nErrors: %lu\r\nWorker error: %lu\r\n",checks,errors,(DWORD)worker_error);
 file=CreateFileA("C:\\FP32\\HEAPTEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(file!=INVALID_HANDLE_VALUE){WriteFile(file,report,lstrlenA(report),&n,0);CloseHandle(file);}ExitProcess(errors?1:0);
}
