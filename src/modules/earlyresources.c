#include <windows.h>
#include <conio.h>
#define MAX 6000
static HANDLE objects[MAX];
static void say(const char *s){while(*s)outp(0xe9,*s++);}
static void number(unsigned n){char b[6];int i=0;do{b[i++]='0'+n%10;n/=10;}while(n);while(i)outp(0xe9,b[--i]);}
int PASCAL WinMain(HINSTANCE h,HINSTANCE p,LPSTR cmd,int show){
    unsigned i,n,r;int kind,ok=1;LOGBRUSH lb;
    HMENU parent,sub;
    (void)h;(void)p;(void)cmd;(void)show;
    say("W31PROBE begin\n");
    for(kind=0;kind<2;kind++){
        for(n=0;n<MAX;n++){
            HANDLE o=kind?(HANDLE)CreateSolidBrush(RGB(n&255,n>>8,1)):(HANDLE)CreateMenu();
            if(!o)break;
            objects[n]=o;
        }
        r=0;
        for(i=n;i;i--)if(kind?DeleteObject(objects[i-1]):DestroyMenu(objects[i-1]))r++;
        say(kind?"brushes=":"menus=");number(n);say(" released=");number(r);say("\n");
        if(r!=n||!r)ok=0;
    }
    parent=CreateMenu();sub=CreateMenu();
    if(!parent||!sub)ok=0;
#ifdef WIN3
    for(i=0;i<35;i++)if(!AppendMenu(sub,MF_STRING,100+i,"Item"))ok=0;
    if(!AppendMenu(parent,MF_POPUP,(UINT)sub,"Submenu")||GetSubMenu(parent,0)!=sub||GetMenuItemCount(sub)!=35||GetMenuItemID(sub,34)!=134)ok=0;
#else
    for(i=0;i<35;i++)if(!ChangeMenu(sub,0,"Item",100+i,MF_APPEND|MF_STRING))ok=0;
    if(!ChangeMenu(parent,0,"Submenu",(UINT)sub,MF_APPEND|MF_POPUP)||GetSubMenu(parent,0)!=sub)ok=0;
#endif
    if(!DestroyMenu(parent))ok=0;
    {HBRUSH b=CreateSolidBrush(RGB(37,55,11));if(!b||!GetObject(b,sizeof(lb),&lb)||lb.lbColor!=RGB(37,55,11)||!DeleteObject(b))ok=0;}
    say(ok?"result=PASS resource allocation and reuse\n":"result=FAIL resources\n");
    say("W31PROBE end\n");return !ok;
}
