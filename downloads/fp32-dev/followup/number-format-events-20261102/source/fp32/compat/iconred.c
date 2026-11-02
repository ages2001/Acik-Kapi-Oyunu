#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
static HICON icons[3];
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_PAINT){PAINTSTRUCT p;HDC dc=BeginPaint(w,&p);DrawIcon(dc,20,20,icons[0]);DrawIcon(dc,100,20,icons[1]);DrawIcon(dc,180,20,icons[2]);TextOutA(dc,10,65,"Launcher  Remove  Window  ",26);EndPaint(w,&p);return 0;}if(m==WM_TIMER)DestroyWindow(w);if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(w,m,a,b);}
void WINAPI entry(void){WNDCLASSA c;MSG m;HWND w;DWORD i;for(i=0;i<sizeof(c);i++)((BYTE*)&c)[i]=0;c.hInstance=GetModuleHandleA(0);c.lpszClassName="FP32IconProbe";c.lpfnWndProc=proc;c.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);c.hCursor=LoadCursorA(0,IDC_ARROW);RegisterClassA(&c);
 icons[0]=ExtractIconA(c.hInstance,"D:\\FP32.EXE",0);icons[1]=ExtractIconA(c.hInstance,"C:\\FP10\\UNINST.EXE",1);icons[2]=ExtractIconA(c.hInstance,"D:\\F32U.DLL",0);
 w=CreateWindowA(c.lpszClassName,"Native icon extraction",WS_OVERLAPPEDWINDOW|WS_VISIBLE,180,140,270,140,0,0,c.hInstance,0);SetTimer(w,1,60000,0);while(GetMessageA(&m,0,0,0)>0){TranslateMessage(&m);DispatchMessageA(&m);}for(i=0;i<3;i++)if((UINT_PTR)icons[i]>1)DestroyIcon(icons[i]);ExitProcess(0);
}
