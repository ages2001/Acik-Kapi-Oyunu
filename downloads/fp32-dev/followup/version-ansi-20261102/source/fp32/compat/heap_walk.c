/* EXPERIMENTAL: excluded from the normal build. Native allocations above
   roughly 1 MiB are not tracked by NT 3.1 heap segment lists. The current
   native regression reports two failures; do not ship this as HeapWalk. */
/* NT 3.1 heap iterator. Layout verified against native RtlSnapShotHeap and
   allocated blocks in the supplied NT 3.1 image; never use it on another OS.
   No allocations are made from the heap being enumerated. Callers that need
   a stable complete walk must hold HeapLock across their enumeration. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
extern BOOL WINAPI fp_HeapLock(HANDLE);
extern BOOL WINAPI fp_HeapUnlock(HANDLE);
static void clear_entry(PROCESS_HEAP_ENTRY *e){DWORD i;for(i=0;i<sizeof(*e);i++)((BYTE*)e)[i]=0;}
static BOOL select_entry(PROCESS_HEAP_ENTRY *out,PROCESS_HEAP_ENTRY *candidate,BOOL *next){
 if(*next){*out=*candidate;return TRUE;}
 if(out->lpData==candidate->lpData && out->iRegionIndex==candidate->iRegionIndex && out->wFlags==candidate->wFlags)*next=TRUE;
 return FALSE;
}
BOOL WINAPI fp_HeapWalk(HANDLE heap,LPPROCESS_HEAP_ENTRY out){
 typedef BOOL(WINAPI *Native)(HANDLE,LPPROCESS_HEAP_ENTRY);
 Native native=(Native)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"HeapWalk");
 PROCESS_HEAP_ENTRY e;DWORD index,error=ERROR_NO_MORE_ITEMS;BOOL next,ok=FALSE;BYTE *segment,*first,*end,*p;DWORD *range;
 if(native)return native(heap,out);
 if(!out){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 if((GetVersion()&0xffff)!=0x0a03){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
 if(!heap||IsBadReadPtr(heap,0x88)||*(DWORD*)((BYTE*)heap+0x10)!=0xeeffeeff){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
 if(!fp_HeapLock(heap))return FALSE;
 next=out->lpData==0;
 for(index=0;index<16;index++){
  DWORD pages,uncommitted,span,flags,bytes,steps=0;MEMORY_BASIC_INFORMATION mbi;
  segment=*(BYTE**)((BYTE*)heap+0x48+index*4);if(!segment)continue;
  if(IsBadReadPtr(segment,0x3c)){error=ERROR_INVALID_DATA;goto done;}
  first=*(BYTE**)(segment+0x28);end=*(BYTE**)(segment+0x2c);pages=*(DWORD*)(segment+0x24);uncommitted=*(DWORD*)(segment+0x30);range=*(DWORD**)(segment+0x38);
  if(!VirtualQuery(segment,&mbi,sizeof(mbi))||pages>0xfffff||uncommitted>pages||first>=end||(DWORD)end-(DWORD)mbi.AllocationBase!=(pages<<12)){error=ERROR_INVALID_DATA;goto done;}
  clear_entry(&e);e.lpData=mbi.AllocationBase;e.cbData=(DWORD)first-(DWORD)e.lpData;e.iRegionIndex=(BYTE)index;e.wFlags=PROCESS_HEAP_REGION;
  e.Region.dwCommittedSize=(pages-uncommitted)<<12;e.Region.dwUnCommittedSize=uncommitted<<12;e.Region.lpFirstBlock=first;e.Region.lpLastBlock=end;
  if(select_entry(out,&e,&next)){ok=TRUE;goto done;}
  p=first;
  while(p<end){
   if(++steps>0x100000||IsBadReadPtr(p,16)){error=ERROR_INVALID_DATA;goto done;}
   span=(DWORD)*(WORD*)p*16;flags=p[5];
   if(span<16||span>(DWORD)(end-p)){error=ERROR_INVALID_DATA;goto done;}
   /* Native end-of-committed-run fence is not an allocated user block. */
   if(!(span==16&&(flags&9)==9)){
    clear_entry(&e);e.lpData=p+16;e.iRegionIndex=(BYTE)index;
    if(flags&1){bytes=HeapSize(heap,0,p+16);if(bytes==(DWORD)-1||bytes>span-16||span-bytes>255){error=ERROR_INVALID_DATA;goto done;}e.wFlags=PROCESS_HEAP_ENTRY_BUSY;e.cbData=bytes;e.cbOverhead=(BYTE)(span-bytes);}
    else {e.cbData=span-16;e.cbOverhead=16;}
    if(select_entry(out,&e,&next)){ok=TRUE;goto done;}
   }
   p+=span;
   if(flags&8){
    if(!range){p=end;continue;}
    if(IsBadReadPtr(range,12)||range[1]<(DWORD)p||range[1]>=(DWORD)end||!range[2]||range[2]>(DWORD)end-range[1]){error=ERROR_INVALID_DATA;goto done;}
    clear_entry(&e);e.lpData=(LPVOID)range[1];e.cbData=range[2];e.iRegionIndex=(BYTE)index;e.wFlags=PROCESS_HEAP_UNCOMMITTED_RANGE;
    if(select_entry(out,&e,&next)){ok=TRUE;goto done;}
    p=(BYTE*)(range[1]+range[2]);range=(DWORD*)range[0];
   }
  }
 }
 if(!next)error=ERROR_INVALID_PARAMETER;
done:
 fp_HeapUnlock(heap);if(!ok)SetLastError(error);return ok;
}
