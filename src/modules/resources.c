#include <windows.h>
#include <toolhelp.h>
#include <conio.h>
#include <string.h>
#define MAXIMUM 12000
static HANDLE held[MAXIMUM];
static char buf[224];
static void emit(const char *s){ while(*s) outp(0xe9,*s++); }
static int percent(int flag){return GetFreeSystemResources(flag);}
static void pools(const char *stage){
    SYSHEAPINFO sh;
    sh.dwSize=sizeof(sh);
    if(SystemHeapInfo(&sh)){
        wsprintf(buf,"%s USER=%u GDI=%u system=%u user_selector=%04x gdi_selector=%04x\n",(LPSTR)stage,sh.wUserFreePercent,sh.wGDIFreePercent,percent(GFSR_SYSTEMRESOURCES),sh.hUserSegment,sh.hGDISegment);
        emit(buf);
    }
}
static void trial(const char *type,int menu) {
    unsigned n=0,i,restored=0;
    int kind=menu?GFSR_USERRESOURCES:GFSR_GDIRESOURCES;
    int initial=percent(kind),at_low=initial;
    HANDLE h;
    while(n<MAXIMUM) {
        if(menu) h=(HANDLE)CreatePopupMenu();
        else h=(HANDLE)CreateSolidBrush(RGB(n&255,(n>>8)&255,1));
        if(!h) break;
        held[n++]=h;
        if(!(n&7) && (at_low=percent(kind))<=10)break;
    }
    pools(type);
    for(i=n;i;i--) {
        if(menu?DestroyMenu((HMENU)held[i-1]):DeleteObject((HGDIOBJ)held[i-1]))restored++;
    }
    wsprintf(buf,"%s simultaneous=%u released=%u before_percent=%u low_percent=%u after_percent=%u\n",(LPSTR)type,n,restored,initial,at_low,percent(kind));emit(buf);
}

static int exercise(void) {
    HMENU main_menu=CreateMenu(), sub=CreatePopupMenu();
    LOGBRUSH lb;
    HDC screen=GetDC(0), dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,8,8);
    HGDIOBJ oldbmp=SelectObject(dc,bitmap),oldbrush;
    HBRUSH brush=CreateSolidBrush(RGB(255,255,255));
    HBRUSH black=CreateSolidBrush(RGB(0,0,0));
    RECT bounds={0,0,8,8};
    BOOL ok=main_menu && sub && dc && brush && black && oldbmp;
    if (ok) ok=AppendMenu(sub,MF_STRING,25,"Object")
        && AppendMenu(main_menu,MF_POPUP,(UINT)sub,"Menu") && GetSubMenu(main_menu,0)==sub
        && GetMenuItemID(sub,0)==25 && InsertMenu(sub,0,MF_BYPOSITION|MF_STRING,23,"First")
        && GetMenuItemCount(sub)==2 && DeleteMenu(sub,0,MF_BYPOSITION)
        && IsMenu(sub);
    if (ok) ok=GetObject(brush,sizeof(lb),&lb)==sizeof(lb) && lb.lbStyle==BS_SOLID && lb.lbColor==RGB(255,255,255);
    oldbrush=SelectObject(dc,brush);
    if (!oldbrush || !FillRect(dc,&bounds,brush) || GetPixel(dc,3,3)!=RGB(255,255,255)) ok=FALSE;
    if (!FillRect(dc,&bounds,black) || GetPixel(dc,3,3)!=RGB(0,0,0)) ok=FALSE;
    SelectObject(dc,oldbrush);
    if (!UnrealizeObject(brush) || !DeleteObject(brush) || !DeleteObject(black)) ok=FALSE;
    SelectObject(dc,oldbmp);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(0,screen);
    if (!DestroyMenu(main_menu) || IsMenu(sub)) ok=FALSE;
    return ok;
}

int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR cmd,int show){
    (void)h;(void)prev;(void)cmd;(void)show;
    emit("W31PROBE begin\n");pools("start");trial("menus",1);trial("brushes",0);pools("end");if (exercise()) emit("result=PASS menus and GDI drawing\n"); else emit("result=FAIL menus or GDI drawing\n");emit("W31PROBE end\n");return 0;
}
