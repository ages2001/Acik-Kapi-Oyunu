#include <windows.h>
#include <toolhelp.h>
#include <conio.h>
#include <string.h>
typedef struct { DWORD size,reserved,committed,free,largest,handles; } Info;
typedef WORD (FAR PASCAL *GetSeg)(DWORD);
typedef BOOL (FAR PASCAL *GetInfo)(Info FAR *,WORD);
static HFILE note;static char line[172];static HMENU menus[6500];
static void put(const char *s){_lwrite(note,s,lstrlen(s));while(*s)outp(0xe9,*s++);}
static void dump(const char *name,WORD dg,WORD ptr){
    GLOBALENTRY e; HGLOBAL h;char FAR *data;DWORD heap;WORD segment;Info info;HMODULE k=GetModuleHandle("KERNEL");
    GetSeg getseg=(GetSeg)GetProcAddress(k,MAKEINTRESOURCE(229));GetInfo getinfo=(GetInfo)GetProcAddress(k,MAKEINTRESOURCE(444));
    e.dwSize=sizeof(e);if(!GlobalEntryModule(&e,GetModuleHandle(name),dg)){put("module entry unavailable\r\n");return;}
    h=e.hBlock;data=GlobalLock(h);if(!data)return;
    heap=*(DWORD FAR *)(data+ptr);GlobalUnlock(h);
    if(!heap||!getseg||!getinfo){put("heap address unavailable\r\n");return;}
    segment=getseg(heap);info.size=sizeof(info);
    if(!getinfo(&info,segment)){put("heap statistics unavailable\r\n");return;}
    wsprintf(line,"%s reserved=%lu committed=%lu free=%lu handles_free=%lu\r\n",(LPSTR)name,info.reserved,info.committed,info.free,info.handles);put(line);
}
int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR cmd,int show){
    WORD v=LOWORD(GetVersion());unsigned n=0,i,j;DWORD items=0;int ok=1;
    (void)h;(void)prev;(void)cmd;(void)show;
    note=_lcreat("C:\\HEAP9X.LOG",0);put("W31PROBE begin\r\n");
    /* Win9x's Win16 GetVersion may retain Win3 compatibility. The build tag is
       passed by the small per-edition launch command stored in SYSTEM.INI. */
#ifdef WINME_HEAP
    dump("USER",35,0x29a);dump("GDI",47,0x41e);
#elif defined(WIN95_HEAP)
    dump("USER",32,0x1d6);dump("GDI",45,0x138);
#else
    dump("USER",34,0x27a);dump("GDI",47,0x422);
#endif
    for(n=0;n<6500;n++){
        HMENU m=CreatePopupMenu();
        if(!m)break;
        menus[n]=m;
        for(j=0;j<32;j++){
            if(!AppendMenu(m,MF_STRING,j+1,"A menu entry to measure the window-manager heap"))break;
            items++;
        }
        if(j!=32){n++;break;}
    }
    for(i=n;i;i--)if(!DestroyMenu(menus[i-1]))ok=0;
    wsprintf(line,"menus=%u items=%lu all_released=%d\r\n",n,(DWORD)items,ok);put(line);
    put("W31PROBE end\r\n");_lclose(note);return !ok;
}
