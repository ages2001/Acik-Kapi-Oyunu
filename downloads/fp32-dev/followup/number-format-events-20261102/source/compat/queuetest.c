#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Native NT 3.1 API contract checks for the diagnostic message-pump bridge. */
static HANDLE logh;static int fails;
static BOOL(WINAPI*peek)(LPMSG,HWND,UINT,UINT,UINT);
static BOOL(WINAPI*get)(LPMSG,HWND,UINT,UINT);
static void check(int ok,const char*s){DWORD n=0,w;if(!ok)fails++;while(s[n])n++;WriteFile(logh,ok?"PASS ":"FAIL ",5,&w,0);WriteFile(logh,s,n,&w,0);WriteFile(logh,"\r\n",2,&w,0);}
static void drain(void){MSG m;int i;for(i=0;i<1000&&PeekMessageW(&m,0,0,0,PM_REMOVE);i++){TranslateMessage(&m);DispatchMessageW(&m);}}
void WINAPI entry(void){HMODULE lib=LoadLibraryA("C:\\FP10\\TRACE\\FPU.DLL");WNDCLASSW c;HWND a,b;MSG m;int i;POINT old,p;
 peek=(void*)GetProcAddress(lib,"PeekMessageW");get=(void*)GetProcAddress(lib,"GetMessageW");if(!peek||!get)ExitProcess(2);
 logh=CreateFileA("C:\\FP10\\QUEUE.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 for(i=0;i<sizeof(c);i++)((BYTE*)&c)[i]=0;c.hInstance=GetModuleHandleA(0);c.lpfnWndProc=DefWindowProcW;c.lpszClassName=L"QueueProbe";RegisterClassW(&c);
 a=CreateWindowExW(0,c.lpszClassName,L"Input queue probe",WS_VISIBLE|WS_OVERLAPPEDWINDOW,20,20,240,180,0,0,c.hInstance,0);
 b=CreateWindowExW(0,c.lpszClassName,L"Filter probe",WS_OVERLAPPEDWINDOW,300,20,200,180,0,0,c.hInstance,0);SetForegroundWindow(a);drain();
 PostMessageW(a,WM_APP+1,11,22);check(peek(&m,a,WM_APP+1,WM_APP+1,PM_NOREMOVE)&&m.wParam==11,"explicit filter finds posted message");
 check(peek(&m,a,WM_APP+1,WM_APP+1,PM_NOREMOVE)&&m.lParam==22,"PM_NOREMOVE retains message");
 check(peek(&m,a,WM_APP+1,WM_APP+1,PM_REMOVE)&&m.wParam==11,"PM_REMOVE retrieves message");
 check(!peek(&m,a,WM_APP+1,WM_APP+1,PM_REMOVE),"removed message is absent");
 PostMessageW(b,WM_APP+2,33,44);check(!peek(&m,a,WM_APP+2,WM_APP+2,PM_REMOVE),"HWND filter excludes other window");
 check(peek(&m,b,WM_APP+2,WM_APP+2,PM_REMOVE)&&m.wParam==33,"other window message remains queued");
 drain();GetCursorPos(&old);p.x=80;p.y=70;ClientToScreen(a,&p);SetCursorPos(p.x,p.y);drain();
 PostMessageW(a,WM_APP+3,55,66);mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);
 check(peek(&m,a,WM_APP+3,WM_APP+3,PM_REMOVE)&&m.wParam==55,"explicit posted-message filter is honored with input pending");
 check(peek(&m,a,WM_LBUTTONDOWN,WM_LBUTTONDOWN,PM_REMOVE)&&m.message==WM_LBUTTONDOWN,"native mouse input remains available");drain();
 PostMessageW(a,WM_APP+4,77,88);check(get(&m,a,WM_APP+4,WM_APP+4)>0&&m.wParam==77,"GetMessage explicit filter preserves payload");drain();
 PostQuitMessage(73);check(get(&m,0,0,0)==0&&m.message==WM_QUIT&&m.wParam==73,"GetMessage returns zero for WM_QUIT and preserves exit code");
 PostQuitMessage(74);check(peek(&m,0,WM_KEYFIRST,WM_KEYLAST,PM_NOREMOVE)&&m.message==WM_QUIT,"PeekMessage returns WM_QUIT regardless of filter");
 check(peek(&m,0,0,0,PM_REMOVE)&&m.message==WM_QUIT&&m.wParam==74,"WM_QUIT survives PM_NOREMOVE");
 DestroyWindow(b);DestroyWindow(a);SetCursorPos(old.x,old.y);CloseHandle(logh);ExitProcess(fails?1:0);
}
