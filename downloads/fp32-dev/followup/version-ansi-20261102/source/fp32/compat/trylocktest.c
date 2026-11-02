#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static BOOL (WINAPI *tryenter)(LPCRITICAL_SECTION);
static CRITICAL_SECTION lock;
static HANDLE logh,ready,release_worker;
static DWORD checks,failures,total;
static volatile LONG inside,violations;
static void value(const char*s,DWORD v){char b[128];DWORD i,n=0,w;while(*s)b[n++]=*s++;b[n++]=' ';for(i=0;i<8;i++)b[n++]="0123456789ABCDEF"[(v>>(28-i*4))&15];b[n++]='\r';b[n++]='\n';WriteFile(logh,b,n,&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok){failures++;value(s,GetLastError());}}
static DWORD WINAPI contender(LPVOID unused){BOOL acquired;LONG before=lock.LockCount;SetLastError(0x12345678);acquired=tryenter(&lock);if(acquired)LeaveCriticalSection(&lock);return !acquired&&lock.LockCount==before&&GetLastError()==0x12345678?67:1;}
static DWORD WINAPI waiter(LPVOID unused){SetEvent(ready);EnterCriticalSection(&lock);SetEvent(release_worker);LeaveCriticalSection(&lock);return 68;}
static DWORD WINAPI stress(LPVOID arg){DWORD i;for(i=0;i<2000;i++){if((DWORD)arg&1)EnterCriticalSection(&lock);else while(!tryenter(&lock))Sleep(0);if(InterlockedIncrement(&inside)!=1)InterlockedIncrement(&violations);total++;if((i&31)==0)Sleep(0);if(InterlockedDecrement(&inside)!=0)InterlockedIncrement(&violations);LeaveCriticalSection(&lock);}return 0;}
void WINAPI entry(void){HMODULE m;HANDLE threads[4],t;DWORD id,code,i,start;
 logh=CreateFileA("C:\\FP32\\TRYLOCK.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 m=LoadLibraryA("C:\\FP32\\F32K.DLL");tryenter=m?(void*)GetProcAddress(m,"TryEnterCriticalSection"):0;if(!tryenter){value("Load failed",GetLastError());ExitProcess(2);}
 InitializeCriticalSection(&lock);SetLastError(0x12345678);
 check(tryenter(&lock)&&GetLastError()==0x12345678,"Acquire free section");
 check(tryenter(&lock)&&lock.RecursionCount==2,"Recursive try acquire");
 LeaveCriticalSection(&lock);check(lock.RecursionCount==1,"Native recursive leave");
 t=CreateThread(0,0,contender,0,0,&id);check(t&&WaitForSingleObject(t,5000)==WAIT_OBJECT_0&&GetExitCodeThread(t,&code)&&code==67,"Contended try fails without queue mutation");if(t)CloseHandle(t);
 ready=CreateEventA(0,TRUE,FALSE,0);release_worker=CreateEventA(0,TRUE,FALSE,0);
 t=CreateThread(0,0,waiter,0,0,&id);check(t&&WaitForSingleObject(ready,5000)==WAIT_OBJECT_0,"Native waiter started");
 start=GetTickCount();while(lock.LockCount==0&&GetTickCount()-start<5000)Sleep(0);
 check(lock.LockCount==1&&WaitForSingleObject(release_worker,0)==WAIT_TIMEOUT,"Native waiter blocked behind try owner");
 LeaveCriticalSection(&lock);check(t&&WaitForSingleObject(t,5000)==WAIT_OBJECT_0&&GetExitCodeThread(t,&code)&&code==68,"Native waiter wakes after leave");if(t)CloseHandle(t);CloseHandle(ready);CloseHandle(release_worker);
 EnterCriticalSection(&lock);check(tryenter(&lock)&&lock.RecursionCount==2,"Try recursively acquires native owner");LeaveCriticalSection(&lock);LeaveCriticalSection(&lock);
 for(i=0;i<4;i++)threads[i]=CreateThread(0,0,stress,(LPVOID)i,0,&id);
 for(i=0;i<4;i++){check(threads[i]&&WaitForSingleObject(threads[i],60000)==WAIT_OBJECT_0,"Mixed worker finished");if(threads[i])CloseHandle(threads[i]);}
 check(!violations&&total==8000&&inside==0,"Mixed native and try owners exclude each other");
 check(lock.LockCount==-1&&lock.RecursionCount==0&&!lock.OwningThread,"Final native lock state free");
 value("Protected increments",total);value("Overlap violations",violations);value("Checks",checks);value("Failures",failures);
 DeleteCriticalSection(&lock);CloseHandle(logh);FreeLibrary(m);ExitProcess(failures?1:0);
}
