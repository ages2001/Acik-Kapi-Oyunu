/* Interactive Win16 and native Win32/AMD64 viewer for device-rendered pixels.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include "demo_scene.h"
#ifndef _WIN32
#define CALLBACK_WND __export FAR PASCAL
typedef int (FAR PASCAL *DrawTriangles)(const WxpDraw FAR *, WxpFrame FAR *);
#else
#define CALLBACK_WND CALLBACK
#endif
static WxpFrame frame;
static BITMAPINFO bitmap;
static unsigned char bgr[WXP_SIDE*WXP_SIDE*3];
static char message[160];

static int draw_scene(HINSTANCE instance)
{
#ifdef _WIN32
    DWORD count = 0;
    HANDLE h = CreateFileA("\\\\.\\WxpGpu",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                           NULL,OPEN_EXISTING,0,NULL);
    BOOL ok;
    if (h == INVALID_HANDLE_VALUE) { lstrcpy(message,"Start the WINXPDM 3D driver and reopen this window."); return 0; }
    ok = DeviceIoControl(h,WXP_GPU_IOCTL,(void *)&scene,sizeof(scene),&frame,sizeof(frame),&count,NULL);
    CloseHandle(h);
    if (!ok || count != sizeof(frame)) { lstrcpy(message,"Windows could not complete the 3D request."); return 0; }
#else
    char path[260], *name;
    HINSTANCE library;
    DrawTriangles draw;
    if (!GetModuleFileName(instance,path,sizeof(path))) { lstrcpy(message,"Cannot locate the 3D viewer."); return 0; }
    name = path + lstrlen(path);
    while (name > path && name[-1] != '\\' && name[-1] != ':') --name;
    if ((unsigned)(name-path)+11 > sizeof(path)) return 0;
    lstrcpy(name,"WXPGPU.DRV");
    library = LoadLibrary(path);
    if ((UINT)library < 32) { lstrcpy(message,"Open the viewer from the folder containing WXPGPU.DRV."); return 0; }
    draw = (DrawTriangles)GetProcAddress(library,"WXPDRAWTRIANGLES");
    if (!draw) { FreeLibrary(library); lstrcpy(message,"The driver entry point is missing."); return 0; }
    frame.status = draw(&scene,&frame);
    FreeLibrary(library);
#endif
    if (frame.status) {
        if (frame.status == WXP_NO_DEVICE) lstrcpy(message,"No VirtIO GPU was found on the PCI bus.");
        else if (frame.status == WXP_BUSY) lstrcpy(message,"Another driver is using the VirtIO GPU.");
        else wsprintf(message,"The GPU returned status %u at command 0x%04x.",(unsigned)frame.status,(unsigned)frame.request);
        return 0;
    }
    wsprintf(message,"%lu completed GPU commands.  %lu configured outputs.",frame.completed,frame.outputs);
    return 1;
}
LRESULT CALLBACK_WND view(HWND window,UINT msg,WPARAM wp,LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window,&paint);
        RECT r; int width;
        GetClientRect(window,&r);
        width = r.bottom-48;
        if (width > r.right-16) width = r.right-16;
        if (width > 0 && frame.completed && !frame.status)
            StretchDIBits(dc,(r.right-width)/2,8,width,width,0,0,WXP_SIDE,WXP_SIDE,bgr,&bitmap,DIB_RGB_COLORS,SRCCOPY);
        r.left += 6; r.right -= 6; r.top = r.bottom-34;
        DrawText(dc,message,-1,&r,DT_CENTER|DT_WORDBREAK);
        EndPaint(window,&paint); return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(window,msg,wp,lp);
}

static int application(HINSTANCE instance,HINSTANCE previous,int show)
{
    WNDCLASS wc = {0};
    HWND window;
    unsigned i;
    MSG msg;
    if (draw_scene(instance)) {
        for (i = 0; i < WXP_SIDE*WXP_SIDE; ++i) {
            uint32_t c = frame.pixels[i];
            bgr[i*3] = (unsigned char)(c >> 16); bgr[i*3+1] = (unsigned char)(c >> 8);
            bgr[i*3+2] = (unsigned char)c;
        }
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = WXP_SIDE; bitmap.bmiHeader.biHeight = WXP_SIDE;
        bitmap.bmiHeader.biPlanes = 1; bitmap.bmiHeader.biBitCount = 24;
    }
    if (!previous) {
        wc.hInstance = instance; wc.lpfnWndProc = view;
        wc.hCursor = LoadCursor(NULL,IDC_ARROW); wc.hIcon = LoadIcon(instance,MAKEINTRESOURCE(1));
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        wc.lpszClassName = "WxpGpuView";
        if (!RegisterClass(&wc)) return 1;
    }
    window = CreateWindow("WxpGpuView","WINXPDM 3D",WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT,CW_USEDEFAULT,360,310,NULL,NULL,instance,NULL);
    if (!window) return 1;
    ShowWindow(window,show); UpdateWindow(window);
    while (GetMessage(&msg,NULL,0,0)>0) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
#ifdef _WIN32
void WINAPI WinMainEntry(void) { ExitProcess(application(GetModuleHandle(NULL),NULL,SW_SHOWNORMAL)); }
#else
int PASCAL WinMain(HINSTANCE self,HINSTANCE prev,LPSTR args,int show)
{ (void)args; return application(self,prev,show); }
#endif
