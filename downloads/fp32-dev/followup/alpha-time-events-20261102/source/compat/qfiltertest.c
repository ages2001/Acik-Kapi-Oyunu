#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;static int fails,total;
static BOOL(WINAPI*peek)(LPMSG,HWND,UINT,UINT,UINT);
static DWORD(WINAPI*status)(UINT);
static void check(int ok,const char*s){DWORD n=0,w;total++;if(!ok)fails++;while(s[n])n++;WriteFile(logh,ok?"PASS ":"FAIL ",5,&w,0);WriteFile(logh,s,n,&w,0);WriteFile(logh,"\r\n",2,&w,0);}
static void drain(void){MSG m;int i;for(i=0;i<1000&&PeekMessageW(&m,0,0,0,PM_REMOVE);i++){TranslateMessage(&m);DispatchMessageW(&m);}}
void WINAPI entry(void){HMODULE lib=LoadLibraryA("C:\\FP10\\TRACE\\FPU.DLL");WNDCLASSW c;HWND a,b;MSG m;int i;POINT old,p;DWORD q,e;
 peek=(void*)GetProcAddress(lib,"PeekMessageW");status=(void*)GetProcAddress(lib,"GetQueueStatus");if(!peek||!status)ExitProcess(2);
 logh=CreateFileA("C:\\FP10\\QFILTER.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 for(i=0;i<sizeof(c);i++)((BYTE*)&c)[i]=0;c.hInstance=GetModuleHandleA(0);c.lpfnWndProc=DefWindowProcW;c.lpszClassName=L"QueueMaskProbe";RegisterClassW(&c);
 a=CreateWindowExW(0,c.lpszClassName,L"Input queue mask probe",WS_VISIBLE|WS_OVERLAPPEDWINDOW,20,20,240,180,0,0,c.hInstance,0);
 b=CreateWindowExW(0,c.lpszClassName,L"Window filter",WS_OVERLAPPEDWINDOW,300,20,200,180,0,0,c.hInstance,0);SetForegroundWindow(a);SetFocus(a);GetCursorPos(&old);p.x=80;p.y=70;ClientToScreen(a,&p);SetCursorPos(p.x,p.y);drain();
 PostMessageW(a,WM_APP+1,11,22);check(!peek(&m,a,0,0,0x04070000),"input mask excludes ordinary posted messages");
 PostMessageW(a,WM_KEYDOWN,VK_F9,0);check(!peek(&m,a,0,0,0x04070000),"posted keyboard ID alone is not device input");PeekMessageW(&m,a,WM_KEYDOWN,WM_KEYDOWN,PM_REMOVE);
 keybd_event(VK_F8,0,0,0);Sleep(100);SetLastError(0x12345678);q=status(0x407);e=GetLastError();
 check((q&0x10000)!=0&&e==0x12345678,"modern QS_INPUT reports native keyboard input without invalid flags");
 check(peek(&m,a,0,0,0x04070000)&&m.message==WM_KEYDOWN&&m.wParam==VK_F8,"input-only peek bypasses earlier posted message");
 check(peek(&m,a,0,0,0x04070000)&&m.message==WM_KEYDOWN,"input PM_NOREMOVE retains key");
 check(peek(&m,a,0,0,PM_NOREMOVE)&&m.message==WM_APP+1,"ordinary peek preserves native posted-message priority");
 check(!peek(&m,b,0,0,0x04070000),"input mask honors another HWND filter");
 check(!peek(&m,a,WM_LBUTTONDOWN,WM_LBUTTONUP,0x04070000),"input mask intersects explicit message range");
 check(peek(&m,a,0,0,0x04070001)&&m.message==WM_KEYDOWN,"input PM_REMOVE consumes key");
 check(!peek(&m,a,WM_KEYDOWN,WM_KEYDOWN,PM_NOREMOVE),"removed key is absent");
 check(peek(&m,a,0,0,PM_REMOVE)&&m.message==WM_APP+1&&m.wParam==11,"earlier posted message remains unchanged");
 keybd_event(VK_F8,0,KEYEVENTF_KEYUP,0);Sleep(30);drain();
 PostMessageW(a,WM_COMMAND,123,0);mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);Sleep(30);
 check(peek(&m,a,WM_LBUTTONDOWN,WM_LBUTTONDOWN,0x04070001)&&m.message==WM_LBUTTONDOWN,"mouse input selected within numeric filter");
 check(peek(&m,a,WM_COMMAND,WM_COMMAND,PM_REMOVE)&&m.wParam==123,"posted command inside broad input-ID span is preserved");
 mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);Sleep(30);drain();
 mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);Sleep(30);keybd_event(VK_F8,0,0,0);Sleep(30);
 check(peek(&m,a,0,0,0x04070001)&&m.message==WM_LBUTTONDOWN,"older mouse event precedes newer keyboard event");
 check(peek(&m,a,0,0,0x04070001)&&m.message==WM_KEYDOWN,"newer keyboard event remains next");
 mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);keybd_event(VK_F8,0,KEYEVENTF_KEYUP,0);Sleep(30);drain();
 keybd_event(VK_F8,0,0,0);Sleep(30);mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);Sleep(30);
 check(peek(&m,a,0,0,0x04070001)&&m.message==WM_KEYDOWN,"older keyboard event precedes newer mouse event");
 check(peek(&m,a,0,0,0x04070001)&&m.message==WM_LBUTTONDOWN,"newer mouse event remains next");
 mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);keybd_event(VK_F8,0,KEYEVENTF_KEYUP,0);Sleep(30);drain();
 PostQuitMessage(73);check(peek(&m,0,WM_APP,WM_APP,0x04070000)&&m.message==WM_QUIT&&m.wParam==73,"input mask preserves WM_QUIT outside numeric range");
 check(peek(&m,0,0,0,0x04070001)&&m.message==WM_QUIT&&m.wParam==73,"input PM_NOREMOVE retains WM_QUIT");
 SetLastError(0);q=status(0x80000000);check(q==0&&GetLastError()==ERROR_INVALID_FLAGS,"unknown queue flags still fail native validation");
 check(status(0x400)==0,"unavailable raw-input queue reports no events");
 DestroyWindow(b);DestroyWindow(a);SetCursorPos(old.x,old.y);check(total==21,"expected check count");CloseHandle(logh);ExitProcess(fails?1:0);
}
