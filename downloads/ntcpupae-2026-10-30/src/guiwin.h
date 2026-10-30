/* guiwin.h - minimal CRT-free Win32 GUI runtime, NT 3.1 compatible.
 * Imports only kernel32 + user32 + advapi32 (all present since NT 3.1).
 * Uses GetVersion (not GetVersionEx) so it runs on NT 3.1/3.5 too. */
#ifndef GUIWIN_H
#define GUIWIN_H

typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned int   DWORD;
typedef int            BOOL;
typedef unsigned int   UINT;
typedef long           LONG;
typedef void*          LPVOID;
typedef const char*    LPCSTR;
typedef char*          LPSTR;
typedef void*          HANDLE;
typedef HANDLE         HWND;
typedef HANDLE         HINSTANCE;
typedef HANDLE         HMENU;
typedef HANDLE         HICON;
typedef HANDLE         HCURSOR;
typedef HANDLE         HBRUSH;
typedef HANDLE         HKEY;
typedef UINT           WPARAM;
typedef LONG           LPARAM;
typedef LONG           LRESULT;
typedef LRESULT (__stdcall *WNDPROC)(HWND,UINT,WPARAM,LPARAM);

#define NULL ((void*)0)
#define WINAPI __stdcall
#define CALLBACK __stdcall

#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_VISIBLE   0x10000000
#define WS_CHILD     0x40000000
#define WS_BORDER    0x00800000
#define WS_VSCROLL   0x00200000
#define WS_TABSTOP   0x00010000
#define BS_PUSHBUTTON 0x00000000
#define SS_LEFT      0x00000000
#define LBS_NOTIFY   0x00000001
#define LBS_USETABSTOPS 0x00000080
#define ES_MULTILINE 0x0004
#define ES_READONLY  0x0800
#define ES_AUTOVSCROLL 0x0040
#define WM_CREATE    0x0001
#define WM_DESTROY   0x0002
#define WM_COMMAND   0x0111
#define WM_CLOSE     0x0010
#define SW_SHOW      5
#define SW_SHOWNORMAL 1
#define CW_USEDEFAULT ((int)0x80000000)
#define IDC_ARROW    ((LPCSTR)32512)
#define IDI_APPLICATION ((LPCSTR)32512)
#define COLOR_BTNFACE 15
#define CS_HREDRAW 2
#define CS_VREDRAW 1
#define WHITE_BRUSH 0
#define LB_ADDSTRING 0x0180
#define LB_RESETCONTENT 0x0184
#define LB_GETCURSEL 0x0188
#define LB_SETCURSEL 0x0186
#define LB_GETITEMDATA 0x0199
#define LBS_DISABLENOSCROLL 0x1000
#define LBN_SELCHANGE 1
#define WM_CTLCOLORBTN 0x0135
#define WM_CTLCOLORLISTBOX 0x0134
#define WM_SETFONT 0x0030
#define MB_OK 0
#define MB_ICONINFORMATION 0x40
#define MB_ICONERROR 0x10
#define MB_YESNO 0x04
#define IDYES 6
#define HWND_DESKTOP ((HWND)0)

#define HKEY_LOCAL_MACHINE ((HKEY)0x80000002)
#define KEY_READ 0x20019
#define KEY_SET_VALUE 0x0002
#define KEY_ALL_ACCESS 0xF003F
#define REG_SZ 1
#define REG_DWORD 4
#define ERROR_SUCCESS 0

#define GENERIC_READ  0x80000000
#define GENERIC_WRITE 0x40000000
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define CREATE_ALWAYS 2
#define FILE_END 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define FILE_BEGIN 0
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 4
#define FILE_ATTRIBUTE_HIDDEN 2
#define FILE_ATTRIBUTE_SYSTEM 4
#define FILE_ATTRIBUTE_READONLY 1

typedef struct { DWORD style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra;
    HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
    LPCSTR lpszMenuName; LPCSTR lpszClassName; } WNDCLASSA;
typedef struct { LONG x,y; } POINT;
typedef struct { LONG left,top,right,bottom; } RECT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG;

typedef HANDLE HDC;
typedef HANDLE HFONT;
typedef HANDLE HPEN;
typedef HANDLE HGDIOBJ;
typedef DWORD  COLORREF;
typedef struct { UINT CtlType; UINT CtlID; UINT itemID; UINT itemAction; UINT itemState;
    HWND hwndItem; HDC hDC; RECT rcItem; DWORD itemData; } DRAWITEMSTRUCT;
#define RGB(r,g,b) ((COLORREF)((BYTE)(r)|((BYTE)(g)<<8)|((DWORD)(BYTE)(b)<<16)))

/* window / message / control constants (GUI additions) */
#define WM_SETICON 0x0080
#define WM_CTLCOLORSTATIC 0x0138
#define WM_DRAWITEM 0x002B
#define ICON_SMALL 0
#define ICON_BIG 1
#define BS_GROUPBOX 0x07
#define BS_DEFPUSHBUTTON 0x01
#define LBS_OWNERDRAWFIXED 0x0010
#define LB_SETITEMHEIGHT 0x01A0
#define TRANSPARENT 1
#define FW_NORMAL 400
#define FW_BOLD 700
#define DEFAULT_CHARSET 1
#define VARIABLE_PITCH 2
#define FF_SWISS 0x20
#define COLOR_WINDOW 5

/* kernel32 */
__declspec(dllimport) HANDLE WINAPI GetModuleHandleA(LPCSTR);
__declspec(dllimport) LPVOID WINAPI GetProcAddress(HANDLE,LPCSTR);
__declspec(dllimport) DWORD  WINAPI GetVersion(void);
__declspec(dllimport) void   WINAPI ExitProcess(UINT);
__declspec(dllimport) UINT   WINAPI GetWindowsDirectoryA(LPSTR,UINT);
__declspec(dllimport) HANDLE WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL   WINAPI ReadFile(HANDLE,LPVOID,DWORD,DWORD*,LPVOID);
__declspec(dllimport) BOOL   WINAPI WriteFile(HANDLE,LPVOID,DWORD,DWORD*,LPVOID);
__declspec(dllimport) BOOL   WINAPI CloseHandle(HANDLE);
__declspec(dllimport) DWORD  WINAPI SetFilePointer(HANDLE,LONG,LONG*,DWORD);
__declspec(dllimport) DWORD  WINAPI GetFileSize(HANDLE,DWORD*);
__declspec(dllimport) LPVOID WINAPI VirtualAlloc(LPVOID,DWORD,DWORD,DWORD);
__declspec(dllimport) BOOL   WINAPI VirtualFree(LPVOID,DWORD,DWORD);
__declspec(dllimport) BOOL   WINAPI CopyFileA(LPCSTR,LPCSTR,BOOL);
__declspec(dllimport) DWORD  WINAPI GetFileAttributesA(LPCSTR);
__declspec(dllimport) BOOL   WINAPI SetFileAttributesA(LPCSTR,DWORD);
__declspec(dllimport) HANDLE WINAPI GetStdHandle(DWORD);

/* user32 */
__declspec(dllimport) WORD   WINAPI RegisterClassA(const WNDCLASSA*);
__declspec(dllimport) HWND   WINAPI CreateWindowExA(DWORD,LPCSTR,LPCSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
__declspec(dllimport) LRESULT WINAPI DefWindowProcA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) BOOL   WINAPI GetMessageA(MSG*,HWND,UINT,UINT);
__declspec(dllimport) BOOL   WINAPI IsDialogMessageA(HWND,MSG*);
__declspec(dllimport) BOOL   WINAPI TranslateMessage(const MSG*);
__declspec(dllimport) LRESULT WINAPI DispatchMessageA(const MSG*);
__declspec(dllimport) BOOL   WINAPI ShowWindow(HWND,int);
__declspec(dllimport) BOOL   WINAPI UpdateWindow(HWND);
__declspec(dllimport) LRESULT WINAPI SendMessageA(HWND,UINT,WPARAM,LPARAM);
__declspec(dllimport) BOOL   WINAPI SetWindowTextA(HWND,LPCSTR);
__declspec(dllimport) void   WINAPI PostQuitMessage(int);
__declspec(dllimport) HCURSOR WINAPI LoadCursorA(HINSTANCE,LPCSTR);
__declspec(dllimport) HCURSOR WINAPI SetCursor(HCURSOR);
#define WM_SETCURSOR 0x0020
__declspec(dllimport) HICON  WINAPI LoadIconA(HINSTANCE,LPCSTR);
__declspec(dllimport) int    WINAPI MessageBoxA(HWND,LPCSTR,LPCSTR,UINT);
__declspec(dllimport) HWND   WINAPI GetDlgItem(HWND,int);
__declspec(dllimport) int    __cdecl wsprintfA(LPSTR,LPCSTR,...);
__declspec(dllimport) HANDLE WINAPI GetStockObject(int);

__declspec(dllimport) DWORD  WINAPI GetSysColor(int);
typedef HICON (WINAPI *PFN_CreateIconFromResource)(const BYTE*,DWORD,BOOL,DWORD);
#define COLOR_HIGHLIGHT 13
#define COLOR_HIGHLIGHTTEXT 14
__declspec(dllimport) int    WINAPI FillRect(HDC,const RECT*,HBRUSH);

/* gdi32 */
__declspec(dllimport) HFONT   WINAPI CreateFontA(int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR);
__declspec(dllimport) HGDIOBJ WINAPI SelectObject(HDC,HGDIOBJ);
__declspec(dllimport) BOOL    WINAPI DeleteObject(HGDIOBJ);
__declspec(dllimport) HBRUSH  WINAPI CreateSolidBrush(COLORREF);
__declspec(dllimport) HPEN    WINAPI CreatePen(int,int,COLORREF);
__declspec(dllimport) int     WINAPI SetBkMode(HDC,int);
__declspec(dllimport) COLORREF WINAPI SetTextColor(HDC,COLORREF);
__declspec(dllimport) BOOL    WINAPI TextOutA(HDC,int,int,LPCSTR,int);
__declspec(dllimport) BOOL    WINAPI RoundRect(HDC,int,int,int,int,int,int);
__declspec(dllimport) BOOL    WINAPI Rectangle(HDC,int,int,int,int);
__declspec(dllimport) BOOL    WINAPI MoveToEx(HDC,int,int,POINT*);
__declspec(dllimport) BOOL    WINAPI LineTo(HDC,int,int);
__declspec(dllimport) BOOL    WINAPI Polygon(HDC,const POINT*,int);
__declspec(dllimport) BOOL    WINAPI Polyline(HDC,const POINT*,int);

/* advapi32 */
__declspec(dllimport) LONG WINAPI RegOpenKeyExA(HKEY,LPCSTR,DWORD,DWORD,HKEY*);
__declspec(dllimport) LONG WINAPI RegQueryValueExA(HKEY,LPCSTR,DWORD*,DWORD*,BYTE*,DWORD*);
__declspec(dllimport) LONG WINAPI RegSetValueExA(HKEY,LPCSTR,DWORD,DWORD,const BYTE*,DWORD);
__declspec(dllimport) LONG WINAPI RegCloseKey(HKEY);

/* ---- file enumeration / drives / move-delete ---- */
typedef struct { DWORD lo,hi; } FILETIME;
typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    char cFileName[260];
    char cAlternateFileName[14];
} WIN32_FIND_DATAA;
__declspec(dllimport) HANDLE WINAPI FindFirstFileA(LPCSTR,WIN32_FIND_DATAA*);
__declspec(dllimport) BOOL   WINAPI FindNextFileA(HANDLE,WIN32_FIND_DATAA*);
__declspec(dllimport) BOOL   WINAPI FindClose(HANDLE);
__declspec(dllimport) DWORD  WINAPI GetLogicalDrives(void);
__declspec(dllimport) UINT   WINAPI GetDriveTypeA(LPCSTR);
#define DRIVE_FIXED 3
__declspec(dllimport) BOOL   WINAPI MoveFileA(LPCSTR,LPCSTR);
__declspec(dllimport) BOOL   WINAPI DeleteFileA(LPCSTR);

/* ---- controls: checkbox / combobox / enable ---- */
#define BS_AUTOCHECKBOX 0x03
#define BS_AUTORADIOBUTTON 0x09
#define BM_GETCHECK 0x00F0
#define BM_SETCHECK 0x00F1
#define BST_CHECKED 1
#define BST_UNCHECKED 0
#define CBS_DROPDOWNLIST 0x0003
#define CB_ADDSTRING 0x0143
#define CB_SETCURSEL 0x014E
#define CB_GETCURSEL 0x0147
#define WS_DISABLED 0x08000000
#define NULL_BRUSH 5
#define DT_SINGLELINE 0x20
#define DT_VCENTER 0x04
#define DT_LEFT 0x00
__declspec(dllimport) BOOL    WINAPI EnableWindow(HWND,BOOL);
__declspec(dllimport) BOOL    WINAPI IsWindowEnabled(HWND);
__declspec(dllimport) HWND    WINAPI SetFocus(HWND);

/* ---- icon / painting ---- */
#define WM_PAINT 0x000F
#define MAKEINTRESOURCEA(i) ((LPCSTR)(DWORD)(WORD)(i))
typedef struct { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT;
__declspec(dllimport) HDC  WINAPI BeginPaint(HWND,PAINTSTRUCT*);
__declspec(dllimport) BOOL WINAPI EndPaint(HWND,const PAINTSTRUCT*);
__declspec(dllimport) BOOL WINAPI DrawIcon(HDC,int,int,HICON);

/* ---- native system info (for OS architecture) ---- */
typedef struct {
    WORD wProcessorArchitecture; WORD wReserved;
    DWORD dwPageSize; LPVOID lpMinimumApplicationAddress; LPVOID lpMaximumApplicationAddress;
    DWORD dwActiveProcessorMask; DWORD dwNumberOfProcessors; DWORD dwProcessorType;
    DWORD dwAllocationGranularity; WORD wProcessorLevel; WORD wProcessorRevision;
} SYSTEM_INFO;
typedef void (WINAPI *PFN_GetNativeSystemInfo)(SYSTEM_INFO*);

/* tiny mem/str helpers (compiler may emit calls to these) */
static void* memset(void* d,int c,unsigned n){ BYTE* p=(BYTE*)d; while(n--)*p++=(BYTE)c; return d; }
static void* memcpy(void* d,const void* s,unsigned n){ BYTE* a=(BYTE*)d; const BYTE* b=(const BYTE*)s; while(n--)*a++=*b++; return d; }
static int   m_strlen(const char* s){ int n=0; while(s[n])n++; return n; }
static unsigned strlen(const char* s){ unsigned n=0; while(s[n])n++; return n; }
static void  m_strcpy(char* d,const char* s){ while((*d++=*s++)); }
static void  m_strcat(char* d,const char* s){ while(*d)d++; while((*d++=*s++)); }
static int   m_stricmp(const char* a,const char* b){ for(;;){ char x=*a,y=*b; if(x>='A'&&x<='Z')x+=32; if(y>='A'&&y<='Z')y+=32; if(x!=y)return x-y; if(!x)return 0; a++;b++; } }
static int   m_stricmp_n(const char* a,const char* b,int n){ for(int i=0;i<n;i++){ char x=a[i],y=b[i]; if(x>='A'&&x<='Z')x+=32; if(y>='A'&&y<='Z')y+=32; if(x!=y)return x-y; if(!x)return 0; } return 0; }

#endif
