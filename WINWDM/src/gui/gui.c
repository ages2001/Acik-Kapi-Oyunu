#include "kit.h"

#define BTN_BACK 110
#define BTN_MAIN 111
#define BTN_CANCEL 112
#define BTN_PCI 113
#define BOX_TARGET 114
#define MENU_DRIVER_LOAD 120
#define MENU_DRIVER_UNLOAD 121
static HWND targetBox;
static int selectionAtOpen=CB_ERR;
static int activeTarget=-1;
static HINSTANCE app;
static HWND nextButton, backButton, cancelButton;
static int page;
static BOOL live, completed;
static HICON icon;
static HFONT heading;

#if defined(WK_INFO)
#define WINDOW_NAME "WINWDM Info"
#elif defined(WK_REMOVE)
#define WINDOW_NAME "Remove WINWDM"
#else
#define WINDOW_NAME "WINWDM Setup"
#endif

static HWND pciButton;
static int bodyHeight, characterWidth;

static BOOL handleTargetKey(const MSG *message)
{
    int saved;
    if (!targetBox || !IsWindowVisible(targetBox) || message->message!=WM_KEYDOWN ||
        (message->wParam!=VK_RETURN && message->wParam!=VK_ESCAPE)) return FALSE;
    if (selectionAtOpen==CB_ERR && !SendMessage(targetBox,CB_GETDROPPEDSTATE,0,0))
        return FALSE;
    saved=selectionAtOpen;
    selectionAtOpen=CB_ERR;
    SendMessage(targetBox,CB_SHOWDROPDOWN,FALSE,0);
    if (message->wParam==VK_ESCAPE && saved!=CB_ERR)
        SendMessage(targetBox,CB_SETCURSEL,saved,0);
    return TRUE;
}

static void panel(HDC dc,int x,int y,int width,int height,COLORREF color)
{
    RECT rect;
    HBRUSH brush=CreateSolidBrush(color);
    SetRect(&rect,x,y,x+width,y+height);
    FillRect(dc,&rect,brush);
    DeleteObject(brush);
}

static int paragraph(HDC dc,int left,int top,int right,const char *value,COLORREF color,BOOL draw)
{
    RECT rect;
    UINT flags=DT_LEFT|DT_WORDBREAK|DT_NOPREFIX;
    if (!*value) return top;
    SetRect(&rect,left,top,right,top);
    DrawText(dc,value,-1,&rect,flags|DT_CALCRECT);
    /* Windows 3.0 can narrow the measured rectangle by the raster font's
     * overhang. Reusing that width would wrap the last word a second time. */
    rect.right=right;
    ++rect.bottom;
    if (draw) {
        SetTextColor(dc,color);
        DrawText(dc,value,-1,&rect,flags);
    }
    return rect.bottom;
}

/* DrawText measures each paragraph with the same font, width and word wrapping
 * used for painting. The combo box and footer never share a text rectangle. */
static void layout(HWND window,HDC dc,BOOL draw)
{
    RECT area,box;
    HFONT old;
    int pad=bodyHeight, gap=(bodyHeight+1)/2, buttonHeight=bodyHeight+10;
    int rail,left,right,footer,top,buttonWidth,buttonX,comboHeight;
    BOOL choose=targetBox && page==1 && !completed;
    const char *title,*line,*detail;
#ifdef WK_INFO
    char information[72];
    title="Driver information";
    if (live && activeTarget>=0)
        wsprintf(information,"Active WDM target %s",WkTargets[activeTarget].label);
    line=live ? (activeTarget>=0 ? information : "Driver is running.") : "Kernel driver unavailable.";
    detail=live ? "Kernel checks passed for bitmap, filter, deferred I/O and cancellation." :
        "Install the driver and restart Windows if prompted. Choose Refresh to check again.";
#elif defined(WK_REMOVE)
    title=completed ? "Removal complete" : "Remove WINWDM";
    line=completed ? "WINWDM has been unregistered." : "Remove the driver, Info and the WINWDM group.";
    detail=completed ? "Choose Finish to close this window." : "Close WINWDM Info before continuing.";
#else
    title=completed ? "Setup complete" : page ? "Choose a WDM target" : "WINWDM Setup";
    line=completed ? "The driver and WINWDM Info are installed." :
        page ? "" : "Install the kernel driver and WINWDM Info.";
    detail=completed ? "Open Info from the WINWDM group. Restart Windows first if prompted." :
        page ? "The 1.x targets use smaller startup checks. The 6.0 target runs the extended checks." :
        "Choose Next to select the target version.";
#endif
    GetClientRect(window,&area);
    rail=area.right/5; left=rail+pad; right=area.right-pad;
    footer=area.bottom-buttonHeight-gap*2-2;
    if (draw) {
        panel(dc,0,0,area.right,area.bottom,RGB(192,192,192));
        panel(dc,0,0,rail,footer,RGB(0,0,128));
        panel(dc,0,0,4,footer,RGB(0,128,128));
        DrawIcon(dc,(rail-GetSystemMetrics(SM_CXICON))/2,pad*2,icon);
        panel(dc,rail/4,footer-pad*3,rail/2,2,RGB(0,128,128));
        panel(dc,rail/4+gap,footer-pad*3+6,rail/2-gap,2,RGB(0,255,255));
        panel(dc,0,footer,area.right,1,RGB(128,128,128));
        panel(dc,0,footer+1,area.right,1,RGB(255,255,255));
        SetBkMode(dc,TRANSPARENT);
    }
    old=(HFONT)SelectObject(dc,heading ? heading : GetStockObject(SYSTEM_FONT));
    top=paragraph(dc,left,pad,right,title,RGB(0,0,128),draw)+pad;
    SelectObject(dc,GetStockObject(SYSTEM_FONT));
    top=paragraph(dc,left,top,right,line,RGB(0,0,0),draw);
    if (choose) {
        if (!draw) {
            MoveWindow(targetBox,left,top,right-left,bodyHeight*(WK_TARGET_COUNT+2)+12,TRUE);
            ShowWindow(targetBox,SW_SHOW);
        }
        GetWindowRect(targetBox,&box);
        comboHeight=box.bottom-box.top;
        if (comboHeight<bodyHeight || comboHeight>bodyHeight*3) comboHeight=bodyHeight+8;
        top+=comboHeight;
    } else if (!draw && targetBox) ShowWindow(targetBox,SW_HIDE);
    paragraph(dc,left,top+pad,right,detail,RGB(0,0,0),draw);
    SelectObject(dc,old);
    if (!draw) {
        buttonWidth=characterWidth*10+14;
        buttonX=right-buttonWidth;
        MoveWindow(cancelButton,buttonX,footer+2+gap,buttonWidth,buttonHeight,TRUE);
        buttonX-=buttonWidth+gap;
        MoveWindow(nextButton,buttonX,footer+2+gap,buttonWidth,buttonHeight,TRUE);
        if (backButton || pciButton)
            MoveWindow(backButton ? backButton : pciButton,buttonX-buttonWidth-gap,
                       footer+2+gap,buttonWidth,buttonHeight,TRUE);
    }
}

static void refresh(HWND w)
{
    HDC dc=GetDC(w);
    layout(w,dc,FALSE);
    ReleaseDC(w,dc);
    InvalidateRect(w,NULL,FALSE);
}

static void paint(HWND w)
{
    PAINTSTRUCT ps;
    HDC dc=BeginPaint(w,&ps);
    layout(w,dc,TRUE);
    EndPaint(w,&ps);
}

static void update(HWND w)
{
    SetWindowText(nextButton,completed ? "&Finish" : page ? "&Install" : "&Next >");
    EnableWindow(backButton,page==1 && !completed);
    EnableWindow(cancelButton,!completed);
    refresh(w);
}

LRESULT CALLBACK WK_EXPORT WkWindowProc(HWND w,UINT msg,WPARAM wp,LPARAM lp)
{
    RECT r;
    switch (msg) {
    case WM_CREATE:
        GetClientRect(w,&r);
#ifdef WK_INFO
#ifdef _WIN32
        if (WkNtHost()) {
            HMENU bar = CreateMenu(), drivers = CreatePopupMenu();
            if (!bar || !drivers ||
                !AppendMenuA(drivers, MF_STRING, MENU_DRIVER_LOAD, "&Load SYS...") ||
                !AppendMenuA(drivers, MF_STRING, MENU_DRIVER_UNLOAD, "&Unload SYS...") ||
                !AppendMenuA(bar, MF_POPUP, (UINT_PTR)drivers, "&Drivers")) {
                if (drivers) DestroyMenu(drivers);
                if (bar) DestroyMenu(bar);
                return -1;
            }
            if (!SetMenu(w, bar)) {
                DestroyMenu(bar);
                return -1;
            }
        }
#endif
        pciButton=CreateWindow("BUTTON","&PCI test",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
                     108,r.bottom-32,78,23,w,(HMENU)BTN_PCI,app,NULL);
#endif
#ifndef WK_INFO
#ifndef WK_REMOVE
        {
            int i;
            targetBox=CreateWindow("COMBOBOX","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
                                   98,89,210,112,w,(HMENU)BOX_TARGET,app,NULL);
            if (!targetBox) return -1;
            for (i=0;i<WK_TARGET_COUNT;++i)
                SendMessage(targetBox,CB_ADDSTRING,0,(LPARAM)(LPCSTR)WkTargets[i].label);
            SendMessage(targetBox,CB_SETCURSEL,WK_INDEX_600,0);
        }
        backButton=CreateWindow("BUTTON","< &Back",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
                               116,r.bottom-32,75,23,w,(HMENU)BTN_BACK,app,NULL);
        EnableWindow(backButton,FALSE);
#endif
#endif
        nextButton=CreateWindow("BUTTON",
#ifdef WK_INFO
            "&Refresh",
#elif defined(WK_REMOVE)
            "&Remove",
#else
            "&Next >",
#endif
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,196,r.bottom-32,82,23,w,(HMENU)BTN_MAIN,app,NULL);
        cancelButton=CreateWindow("BUTTON",
#ifdef WK_INFO
            "&Close",
#else
            "Cancel",
#endif
            WS_CHILD|WS_VISIBLE|WS_TABSTOP,286,r.bottom-32,78,23,w,(HMENU)BTN_CANCEL,app,NULL);
        refresh(w);
        SetFocus(nextButton);
        return 0;
    case WM_SIZE: if (nextButton && cancelButton) refresh(w); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: paint(w); return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case BOX_TARGET:
            {
#ifdef _WIN32
                UINT notice=HIWORD(wp);
#else
                UINT notice=HIWORD(lp);
#endif
                /* CBN_DROPDOWN is available on Windows 3.0, which lacks
                 * both CB_GETDROPPEDSTATE and the CBN_CLOSEUP notification. */
                if (notice==CBN_DROPDOWN)
                    selectionAtOpen=(int)SendMessage(targetBox,CB_GETCURSEL,0,0);
                else if (notice==CBN_CLOSEUP || notice==CBN_KILLFOCUS)
                    selectionAtOpen=CB_ERR;
            }
            return 0;
#ifdef WK_INFO
#ifdef _WIN32
        case MENU_DRIVER_LOAD: case MENU_DRIVER_UNLOAD:
            WkChooseVendor(w, LOWORD(wp) == MENU_DRIVER_LOAD);
            return 0;
#endif
        case BTN_PCI:
            if (WkPciCheck())
                MessageBox(w,"The QEMU PCI device counted two writes. Start, stop and D3/D0 passed. Pending I/O completed before removal.",
                           "WINWDM PCI test",MB_OK);
            else
                MessageBox(w,"The PCI test did not complete. This test requires QEMU with its pci-testdev device.",
                           "WINWDM PCI test",MB_OK);
            return 0;
#endif
        case BTN_CANCEL: DestroyWindow(w); return 0;
        case BTN_BACK: page=0; update(w); return 0;
        case BTN_MAIN:
#ifdef WK_INFO
            live=WkKernelReady();
            activeTarget=live ? WkActiveTarget() : -1;
            refresh(w);
#else
            if (completed) { DestroyWindow(w); return 0; }
#ifdef WK_REMOVE
            if (WkUninstall(w)) {
                completed=TRUE; BringWindowToTop(w); SetActiveWindow(w); SetFocus(nextButton);
                SetWindowText(nextButton,"&Finish");
                EnableWindow(cancelButton,FALSE);
                refresh(w);
            }
#else
            if (!page) ++page;
            else if (WkInstall(w,(unsigned)SendMessage(targetBox,CB_GETCURSEL,0,0))) { completed=TRUE; BringWindowToTop(w); SetActiveWindow(w); SetFocus(nextButton); }
            update(w);
#endif
#endif
            return 0;
        }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
#ifndef WK_INFO
    case 0x3e1: case 0x3e4: return WkDdeMessage(w,msg,wp,lp);
#endif
    }
    return DefWindowProc(w,msg,wp,lp);
}

static int run(HINSTANCE instance, HINSTANCE previous, int show)
{
    WNDCLASS c; RECT r; HWND w; MSG msg;
    HDC desktop;
    TEXTMETRIC metrics;
    HFONT prior;
    int width,height,screenX,screenY;
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;
    app=instance;
    icon=LoadIcon(app,MAKEINTRESOURCE(1));
    desktop=GetDC(NULL);
    prior=(HFONT)SelectObject(desktop,GetStockObject(SYSTEM_FONT));
    GetTextMetrics(desktop,&metrics);
    bodyHeight=metrics.tmHeight+metrics.tmExternalLeading;
    if (bodyHeight<12) bodyHeight=12;
    characterWidth=metrics.tmAveCharWidth;
    if (characterWidth<6) characterWidth=6;
    heading=CreateFont(-MulDiv(14,GetDeviceCaps(desktop,LOGPIXELSY),72),0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,
          OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Helv");
    SelectObject(desktop,prior); ReleaseDC(NULL,desktop);
    if (!previous) {
        c.style=0; c.lpfnWndProc=WkWindowProc; c.cbClsExtra=0; c.cbWndExtra=0;
        c.hInstance=app; c.hIcon=icon; c.hCursor=LoadCursor(NULL,IDC_ARROW);
        c.hbrBackground=(HBRUSH)GetStockObject(LTGRAY_BRUSH);
        c.lpszMenuName=NULL; c.lpszClassName=WINDOW_NAME;
        if (!RegisterClass(&c)) return 1;
    }
#ifdef WK_INFO
    live=WkKernelReady();
    activeTarget=live ? WkActiveTarget() : -1;
#endif
    screenX=GetSystemMetrics(SM_CXSCREEN); screenY=GetSystemMetrics(SM_CYSCREEN);
    width=characterWidth*56; if (width<416) width=416;
    if (width>screenX-24) width=screenX-24;
    height=bodyHeight*14+20; if (height<216) height=216;
    if (height>screenY-GetSystemMetrics(SM_CYCAPTION)-24)
        height=screenY-GetSystemMetrics(SM_CYCAPTION)-24;
    SetRect(&r,0,0,width,height);
#if defined(WK_INFO) && defined(_WIN32)
    AdjustWindowRect(&r,style,WkNtHost());
#else
    AdjustWindowRect(&r,style,FALSE);
#endif
    w=CreateWindow(WINDOW_NAME,WINDOW_NAME,style,
        (GetSystemMetrics(SM_CXSCREEN)-(r.right-r.left))/2,
        (GetSystemMetrics(SM_CYSCREEN)-(r.bottom-r.top))/2,
        r.right-r.left,r.bottom-r.top,NULL,NULL,app,NULL);
    if (!w) return 1;
    ShowWindow(w,show); UpdateWindow(w);
    while (GetMessage(&msg,NULL,0,0)>0) {
        if (handleTargetKey(&msg)) continue;
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE) {
            PostMessage(w,WM_CLOSE,0,0); continue;
        }
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN) {
            HWND focus=GetFocus();
            PostMessage(w,WM_COMMAND,focus && focus!=targetBox ? GetDlgCtrlID(focus) : BTN_MAIN,0); continue;
        }
        if (!IsDialogMessage(w,&msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    }
    DeleteObject(heading);
    return (int)msg.wParam;
}
#ifdef WK_REMOVE
extern int WkRemoveLauncher(HINSTANCE,LPSTR);
extern void WkCleanLauncher(HINSTANCE);
#endif
int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR cmd,int show)
{
    int result;
#ifdef WK_REMOVE
    result=WkRemoveLauncher(h,cmd);
    if (result>=0) return result;
#elif defined(WK_INFO) && defined(_WIN32)
    if (cmd && *cmd) return (int)WkVendorCommand(cmd);
#else
    (void)cmd;
#endif
    result=run(h,prev,show);
#ifdef WK_REMOVE
    WkCleanLauncher(h);
#endif
    return result;
}
