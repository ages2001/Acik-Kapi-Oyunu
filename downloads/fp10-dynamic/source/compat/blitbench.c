#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Test-only: compare identical GDI workloads across guest display modes. */
static HANDLE logh;
static void value(const char*s,DWORD v){DWORD n=0,w;char b[16],out[16];int i=0,k=0;while(s[n])n++;WriteFile(logh,s,n,&w,0);do{b[i++]='0'+v%10;v/=10;}while(v);while(i)out[k++]=b[--i];WriteFile(logh,out,k,&w,0);WriteFile(logh,"\r\n",2,&w,0);}
void WINAPI entry(void){HMODULE lib=LoadLibraryA("C:\\FP10\\TRACE\\FPG.DLL");HBITMAP(WINAPI*create)(HDC,const BITMAPINFO*,UINT,VOID**,HANDLE,DWORD);HGDIOBJ(WINAPI*select)(HDC,HGDIOBJ);BOOL(WINAPI*blit)(HDC,int,int,int,int,HDC,int,int,DWORD);BOOL(WINAPI*del)(HGDIOBJ);BOOL(WINAPI*deldc)(HDC);WNDCLASSA c;BITMAPINFO bi;HWND wnd;HDC screen,src,dst;HBITMAP a,b;void *pa,*pb;DWORD i,j,start;MSG m;RECT clip;
 create=(void*)GetProcAddress(lib,"CreateDIBSection");select=(void*)GetProcAddress(lib,"SelectObject");blit=(void*)GetProcAddress(lib,"BitBlt");del=(void*)GetProcAddress(lib,"DeleteObject");deldc=(void*)GetProcAddress(lib,"DeleteDC");if(!create||!select||!blit||!del||!deldc)ExitProcess(2);
 logh=CreateFileA("C:\\FP10\\BLIT.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 for(i=0;i<sizeof(c);i++)((BYTE*)&c)[i]=0;c.lpfnWndProc=DefWindowProcA;c.hInstance=GetModuleHandleA(0);c.lpszClassName="BlitBench";RegisterClassA(&c);wnd=CreateWindowA(c.lpszClassName,"GDI transfer benchmark",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,80,470,350,0,0,c.hInstance,0);UpdateWindow(wnd);while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}screen=GetDC(wnd);src=CreateCompatibleDC(screen);dst=CreateCompatibleDC(screen);
 for(i=0;i<sizeof(bi);i++)((BYTE*)&bi)[i]=0;bi.bmiHeader.biSize=40;bi.bmiHeader.biWidth=450;bi.bmiHeader.biHeight=-300;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;a=create(screen,&bi,DIB_RGB_COLORS,&pa,0,0);b=create(screen,&bi,DIB_RGB_COLORS,&pb,0,0);if(!a||!b)ExitProcess(3);select(src,a);select(dst,b);for(i=0;i<450*300;i++)((DWORD*)pa)[i]=((i%450)<<8)^(i/450);
 value("Native bits per pixel=",GetDeviceCaps(screen,BITSPIXEL));value("Iterations=",32);start=GetTickCount();for(j=0;j<32;j++){((DWORD*)pa)[0]=j;if(!blit(screen,0,0,450,300,src,0,0,SRCCOPY))ExitProcess(4);}GdiFlush();value("Full changed surface presentation ms=",GetTickCount()-start);value("Top-left color=",GetPixel(screen,0,0));value("Bottom-left color=",GetPixel(screen,0,299));
 value("Memory DC clip type=",GetClipBox(dst,&clip));start=GetTickCount();for(j=0;j<32;j++)if(!blit(dst,0,0,450,300,src,0,0,SRCCOPY)){value("RGB copy error=",GetLastError());CloseHandle(logh);ExitProcess(5);}value("Full RGB surface copy ms=",GetTickCount()-start);
 deldc(src);deldc(dst);del(a);del(b);ReleaseDC(wnd,screen);DestroyWindow(wnd);CloseHandle(logh);ExitProcess(0);
}
