#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef TEST_CORE_DLL
#define TEST_CORE_DLL "C:\\FP32\\F32K.DLL"
#endif
typedef struct Node {SLIST_ENTRY entry;DWORD number;} Node;
static SLIST_HEADER list;static Node nodes[2048];static BYTE seen[2048];
static void(WINAPI*init)(PSLIST_HEADER);static PSLIST_ENTRY(WINAPI*push)(PSLIST_HEADER,PSLIST_ENTRY);
static PSLIST_ENTRY(WINAPI*pop)(PSLIST_HEADER);static PVOID(WINAPI*enc)(PVOID);static PVOID(WINAPI*dec)(PVOID);
static HANDLE logh;static DWORD failures;
static HANDLE(WINAPI*thread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
static void check(BOOL ok,const char*s){DWORD n=0,w;if(!ok)failures++;while(s[n])n++;WriteFile(logh,ok?"PASS ":"FAIL ",5,&w,0);WriteFile(logh,s,n,&w,0);WriteFile(logh,"\r\n",2,&w,0);}
static DWORD WINAPI worker(void*p){DWORD base=(DWORD)p,i;for(i=base;i<base+512;i++){nodes[i].number=i;push(&list,&nodes[i].entry);if(dec(enc((PVOID)i))!=(PVOID)i)return 1;}return 0;}
void WINAPI entry(void){HMODULE lib=LoadLibraryA(TEST_CORE_DLL);SLIST_ENTRY a,b;HANDLE threads[4];DWORD i,n,code;BOOL good=TRUE;Node*node;
 init=(void*)GetProcAddress(lib,"InitializeSListHead");push=(void*)GetProcAddress(lib,"InterlockedPushEntrySList");pop=(void*)GetProcAddress(lib,"InterlockedPopEntrySList");enc=(void*)GetProcAddress(lib,"EncodePointer");dec=(void*)GetProcAddress(lib,"DecodePointer");thread=(void*)GetProcAddress(lib,"CreateThread");if(!init||!push||!pop||!enc||!dec||!thread)ExitProcess(2);
 logh=CreateFileA("C:\\FP32\\SLIST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 init(&list);check(!pop(&list)&&list.Depth==0,"empty list initialized");
 check(!push(&list,&a)&&push(&list,&b)==&a&&list.Depth==2,"push returns previous head and updates depth");
 check(pop(&list)==&b&&pop(&list)==&a&&!pop(&list)&&list.Depth==0,"pop preserves LIFO and emptiness");
 for(i=0;i<4;i++){threads[i]=thread(0,0,worker,(void*)(i*512),0,0);if(!threads[i])ExitProcess(3);}
 for(i=0;i<4;i++){if(WaitForSingleObject(threads[i],10000)!=WAIT_OBJECT_0)good=FALSE;GetExitCodeThread(threads[i],&code);if(code)good=FALSE;CloseHandle(threads[i]);}
 check(good,"four producer threads and concurrent pointer round trips");check(list.Depth==2048,"all concurrent pushes counted");
 n=0;while((node=(Node*)pop(&list))!=0){if(node->number>=2048||seen[node->number])good=FALSE;else seen[node->number]=1;n++;if(n>2048){good=FALSE;break;}}
 check(good&&n==2048&&list.Depth==0,"all 2048 nodes returned exactly once");
 check(dec(enc(0))==0&&dec(enc((void*)0xffffffffu))==(void*)0xffffffffu,"null and full-width pointer round trips");
 CloseHandle(logh);ExitProcess(failures?1:0);
}
