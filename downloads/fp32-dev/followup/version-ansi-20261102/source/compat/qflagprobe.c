#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Test-only native queue behavior for the exact 0x407 mask observed in the
   original Flash executable. Posted messages are deliberately ahead of input. */
static HANDLE logh;
static void hex(const char*s,DWORD v){DWORD n=0,w,i;char out[10];const char*d="0123456789ABCDEF";while(s[n])n++;WriteFile(logh,s,n,&w,0);for(i=0;i<8;i++)out[i]=d[(v>>(28-4*i))&15];out[8]='\r';out[9]='\n';WriteFile(logh,out,10,&w,0);}
void WINAPI entry(void){WNDCLASSW c;HWND w;MSG m;DWORD i,q,e;BOOL r;
 logh=CreateFileA("C:\\FP10\\QFLAGS.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 for(i=0;i<sizeof(c);i++)((BYTE*)&c)[i]=0;c.lpfnWndProc=DefWindowProcW;c.hInstance=GetModuleHandleW(0);c.lpszClassName=L"QueueFlagsProbe";RegisterClassW(&c);
 w=CreateWindowW(c.lpszClassName,L"Native queue flags probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,180,180,350,150,0,0,c.hInstance,0);SetForegroundWindow(w);SetFocus(w);UpdateWindow(w);
 while(PeekMessageW(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}PostMessageW(w,WM_APP+1,0,0);
 keybd_event(VK_F8,0,0,0);Sleep(150);
 SetLastError(0x12345678);q=GetQueueStatus(0x0007);e=GetLastError();hex("Legacy input status=",q);hex("Legacy status error=",e);
 SetLastError(0x12345678);q=GetQueueStatus(0x0407);e=GetLastError();hex("Modern input status=",q);hex("Modern status error=",e);
 SetLastError(0x12345678);r=PeekMessageW(&m,0,0,0,0x04070000);e=GetLastError();hex("Modern Peek result=",r);hex("Modern Peek error=",e);if(r)hex("Modern Peek message=",m.message);
 r=PeekMessageW(&m,0,WM_KEYFIRST,WM_KEYLAST,PM_NOREMOVE);hex("Explicit keyboard Peek result=",r);if(r)hex("Explicit keyboard message=",m.message);
 r=PeekMessageW(&m,0,0,0,PM_NOREMOVE);hex("Ordinary Peek result=",r);if(r)hex("Ordinary Peek message=",m.message);
 keybd_event(VK_F8,0,KEYEVENTF_KEYUP,0);while(PeekMessageW(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}DestroyWindow(w);CloseHandle(logh);ExitProcess(0);
}
