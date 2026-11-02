#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static DWORD checks,errors,used;static char logtext[8192];
static void check(BOOL ok,LPCSTR name){checks++;if(!ok){errors++;used+=wsprintfA(logtext+used,"FAIL: %s (error %lu)\r\n",name,GetLastError());}}
void WINAPI entry(void){
 typedef BOOL(WINAPI*Walk)(HANDLE,LPPROCESS_HEAP_ENTRY);typedef BOOL(WINAPI*Lock)(HANDLE);
 HMODULE m=LoadLibraryA("C:\\FP32\\F32K.DLL");Walk walk=m?(Walk)GetProcAddress(m,"HeapWalk"):0;Lock lock=m?(Lock)GetProcAddress(m,"HeapLock"):0,unlock=m?(Lock)GetProcAddress(m,"HeapUnlock"):0;
 HANDLE heap,out;DWORD i,j,n,counts[132],sizes[132],round,regions,freeblocks,ucr,busy,tick;LPVOID blocks[132];PROCESS_HEAP_ENTRY e;
 check(walk&&lock&&unlock,"exports");if(!walk||!lock||!unlock)goto done;
 heap=HeapCreate(0,4096,0);check(heap!=0,"create heap");if(!heap)goto done;
 for(i=0;i<132;i++){sizes[i]=i<128?1+i*31:(i==128?300000:(i==129?2000000:(i==130?37:0)));blocks[i]=HeapAlloc(heap,0,sizes[i]);check(blocks[i]!=0,"allocation");}
 for(i=1;i<128;i+=2){check(HeapFree(heap,0,blocks[i]),"free alternating blocks");blocks[i]=0;}
 blocks[130]=HeapReAlloc(heap,0,blocks[130],60000);sizes[130]=60000;check(blocks[130]!=0,"reallocate");
 tick=GetTickCount();
 for(round=0;round<2;round++){
  for(i=0;i<132;i++)counts[i]=0;for(i=0;i<sizeof(e);i++)((BYTE*)&e)[i]=0;
  regions=freeblocks=ucr=busy=n=0;check(lock(heap),"external enumeration lock");
  while(walk(heap,&e)){
   if(++n>4096){check(FALSE,"bounded enumeration");break;}
   if(e.wFlags&PROCESS_HEAP_REGION){regions++;check(e.Region.dwCommittedSize+e.Region.dwUnCommittedSize>0,"region sizes");continue;}
   if(e.wFlags&PROCESS_HEAP_UNCOMMITTED_RANGE){ucr++;check(e.cbData!=0,"uncommitted range");continue;}
   if(!(e.wFlags&PROCESS_HEAP_ENTRY_BUSY)){freeblocks++;continue;}
   busy++;
   for(j=0;j<132;j++)if(blocks[j]&&blocks[j]==e.lpData){counts[j]++;check(e.cbData==HeapSize(heap,0,blocks[j])&&e.cbData>=sizes[j],"busy block size");break;}
   check(j!=132,"only live test allocations");
  }
  check(GetLastError()==ERROR_NO_MORE_ITEMS,"end of enumeration");check(unlock(heap),"external enumeration unlock");
  check(regions>=2&&freeblocks>0&&ucr>0,"regions, free blocks and uncommitted ranges");
  for(i=0;i<132;i++)check(counts[i]==(blocks[i]?1:0),"live allocation visited exactly once");
  used+=wsprintfA(logtext+used,"Round %lu: entries=%lu regions=%lu busy=%lu free=%lu uncommitted=%lu\r\n",round,n,regions,busy,freeblocks,ucr);
 }
 used+=wsprintfA(logtext+used,"Two walks elapsed: %lu ms\r\n",GetTickCount()-tick);
 e.lpData=(void*)1;e.iRegionIndex=0;e.wFlags=0;check(!walk(heap,&e)&&GetLastError()==ERROR_INVALID_PARAMETER,"unknown cursor rejected");
 check(!walk(heap,0)&&GetLastError()==ERROR_INVALID_PARAMETER,"NULL output rejected");
 e.lpData=0;check(!walk(0,&e)&&GetLastError()==ERROR_INVALID_HANDLE,"NULL heap rejected");
 for(i=0;i<132;i++)if(blocks[i])HeapFree(heap,0,blocks[i]);check(HeapDestroy(heap),"destroy heap");
 done:used+=wsprintfA(logtext+used,"Checks: %lu\r\nErrors: %lu\r\n",checks,errors);out=CreateFileA("C:\\FP32\\HEAPWALK.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(out!=INVALID_HANDLE_VALUE){WriteFile(out,logtext,used,&n,0);CloseHandle(out);}ExitProcess(errors?1:0);
}
