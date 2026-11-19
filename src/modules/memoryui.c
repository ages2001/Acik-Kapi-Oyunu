#include <windows.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

/* Use only USER/KERNEL APIs present in the first retail Windows version.
   The shell keeps ownership of the dialog and its existing size readouts. */
static FARPROC top_proc, child_proc;
static HWND title_ctrl, icon_ctrl, live_ctrl, legacy_ctrl;
static int has_disk, has_memory, has_capacity;
static char text[128], class_name[36], heading[48], last_value[88];
static DWORD total;
static WORD kind;
static HINSTANCE instance;
static WORD enhanced;
static void log_text(const char *s) {
    while (*s)
        outp(0xe9, *s++);
}
static DWORD multiplex(WORD function, WORD *type) {
    DWORD result;
    WORD ok, mode;
    _asm {
  push si
  push di
  mov ax,0d74fh
  mov bx,function
  int 2fh
  mov ok,ax
  mov mode,bx
  mov result,edx
  pop di
  pop si
    }
    *type = mode;
    return ok == 0x5842 ? result : 0xffffffffUL;
}
static DWORD physical(void) {
    DWORD info[12];
    WORD ok = 0;
    _asm {push es
  push ss
  pop es
  lea di,info
  mov ax,0500h
  int 31h
  jc no_data
  mov ok,1
 no_data:
  pop es
    }
    return ok && info[5] <= 0xfffffUL ? info[5] * 4 : 0xffffffffUL;
}
static void digits(char *out, DWORD n) {
    char rev[15];
    int i = 0, j = 0;
    do {
        if (i && i % 4 == 3)
            rev[i++] = ',';
        rev[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i)
        out[j++] = rev[--i];
    out[j] = 0;
}
static void format(char *buf, const char *label, DWORD kb) {
    strcpy(buf, label);
    digits(buf + strlen(buf), kb);
    strcat(buf, " KiB");
}
BOOL FAR PASCAL __export __loadds scan_child(HWND h, LPARAM l) {
    LONG style;
    int id = GetWindowWord(h, GWW_ID);
    (void)l;
    if (id == 0x5a70) {
        live_ctrl = h;
        return TRUE;
    }
    if (id == 0x5a71)
        return TRUE;
    if (!IsWindowVisible(h))
        return TRUE;
    style = GetWindowLong(h, GWL_STYLE);
    if ((style & 15) == SS_ICON)
        icon_ctrl = h;
    GetWindowText(h, text, sizeof(text));
    if (!strcmp(text, "MS-DOS Executive"))
        title_ctrl = h;
    if (strstr(text, "Disk Space Free"))
        has_disk = 1;
    if (strstr(text, "Memory Free") || strstr(text, "Free Memory") || strstr(text, "KB Free") ||
        strstr(text, "K Free")) {
        has_memory = 1;
        legacy_ctrl = h;
    }
    if (strstr(text, "KiB") || strstr(text, "KB RAM") || strstr(text, "Extended memory"))
        has_capacity = 1;
    return TRUE;
}
static void redraw(HWND h) {
    InvalidateRect(h, NULL, TRUE);
    UpdateWindow(h);
}
BOOL FAR PASCAL __export __loadds scan_top(HWND h, LPARAM l) {
    RECT frame, client;
    int height, width;
    WORD k = 0;
    DWORD avail;
    char label[90], native[100];
    (void)l;
    if (!IsWindowVisible(h))
        return TRUE;
    GetClassName(h, class_name, sizeof(class_name));
    if (strcmp(class_name, "#32770") && stricmp(class_name, "Dialog"))
        return TRUE;
    GetWindowText(h, text, sizeof(text));
    /* The Executive has a captionless About box; later shells name theirs. */
    {
        int shell_about = !stricmp(text, "About") || !stricmp(text, "About Program Manager") ||
                          !stricmp(text, "About File Manager") || !stricmp(text, "About Windows") ||
                          !stricmp(text, "About MS-DOS Executive") || !*text;
        if (!shell_about)
            return TRUE;
    }
    has_disk = has_memory = has_capacity = 0;
    title_ctrl = icon_ctrl = live_ctrl = legacy_ctrl = 0;
    EnumChildWindows(h, (WNDENUMPROC)child_proc, 0);
    if (!live_ctrl && !(has_capacity || (title_ctrl && has_disk && has_memory)))
        return TRUE;
    if (title_ctrl && total != 0xffffffffUL) {
        format(heading, kind ? "Bank total: " : "XMS total: ", total);
        SetWindowText(title_ctrl, heading);
        redraw(title_ctrl);
        if (icon_ctrl)
            redraw(icon_ctrl);
    }
    if (enhanced && !kind) {
        avail = physical();
        k = 2;
    } else
        avail = multiplex(0x5856, &k);
    if (avail == 0xffffffffUL)
        return TRUE;
    format(label, k == 2 ? "Physical RAM free: " : k ? "Bank free: " : "XMS free: ", avail);
    /* Some enhanced shells originally show a Win16 estimate in their KB Free
       field. Refresh that field from the same physical-page sample as the live row. */
    if (k == 2 && legacy_ctrl) {
        char *tail, *start, num[20], value[120];
        int prefix;
        GetWindowText(legacy_ctrl, native, sizeof(native));
        tail = strstr(native, " KB Free");
        if (tail) {
            start = tail;
            while (start > native && ((start[-1] >= '0' && start[-1] <= '9') || start[-1] == ','))
                --start;
            prefix = start - native;
            digits(num, avail);
            if (start != tail && prefix + strlen(num) + strlen(tail) < sizeof(value)) {
                memcpy(value, native, prefix);
                value[prefix] = 0;
                strcat(value, num);
                strcat(value, tail);
                if (strcmp(value, native)) {
                    SetWindowText(legacy_ctrl, value);
                    redraw(legacy_ctrl);
                }
            }
        }
    }
    if (!live_ctrl) {
        GetWindowRect(h, &frame);
        GetClientRect(h, &client);
        height = client.bottom;
        width = client.right;
        MoveWindow(h, frame.left, frame.top, frame.right - frame.left,
                   frame.bottom - frame.top + 20, TRUE);
        live_ctrl = CreateWindow("STATIC", "", WS_CHILD | WS_VISIBLE | SS_CENTER, 2, height,
                                 width - 4, 18, h, (HMENU)0x5a70, instance, NULL);
        if (!live_ctrl)
            return TRUE;
        /* Some shells draw the free-value and its label in different controls.
           Preserve the original value and identify its address space explicitly. */
        if (legacy_ctrl && (has_disk || !enhanced || kind)) {
            char *p;
            GetWindowText(legacy_ctrl, native, sizeof(native));
            p = strstr(native, "Memory Free");
            if (!p)
                p = strstr(native, "Free Memory");
            if (p) {
                char s[96];
                int pos = p - native;
                native[pos] = 0;
                strcpy(s, native);
                strcat(s, has_disk || (!kind && !enhanced) ? "Conventional" : "Win16 heap");
                strncat(s, p + 11, sizeof(s) - strlen(s) - 1);
                SetWindowText(legacy_ctrl, s);
                redraw(legacy_ctrl);
            }
        }
    }
    GetWindowText(live_ctrl, text, sizeof(text));
    if (strcmp(text, label)) {
        SetWindowText(live_ctrl, label);
        redraw(live_ctrl);
        if (strcmp(last_value, label)) {
            log_text(label);
            log_text("\n");
            strcpy(last_value, label);
        }
    }
    return TRUE;
}
typedef void (WINAPI *SweepProc)(void);
static SweepProc start_pager(HINSTANCE instance) {
    char path[144], *name;
    HINSTANCE pager;
    typedef int (WINAPI *StartProc)(void);
    StartProc start;
    int length = GetModuleFileName(instance, path, sizeof(path));
    if (length <= 0 || length >= sizeof(path) - 13)
        return NULL;
    path[length] = 0;
    name = strrchr(path, '\\');
    if (!name)
        return NULL;
    strcpy(name + 1, "WRMPAGE.DLL");
    pager = LoadLibrary(path);
    if (pager < 32)
        return NULL;
    start = (StartProc)GetProcAddress(pager, (LPCSTR)250L);
    if (start && start())
        log_text("WINRAM global memory service ready\n");
    /* The module holds live Windows allocations and stays loaded for the session. */
    return (SweepProc)GetProcAddress(pager, (LPCSTR)251L);
}
int PASCAL WinMain(HINSTANCE h, HINSTANCE prev, LPSTR cmd, int show) {
    typedef DWORD(WINAPI * GetFlags)(void);
    GetFlags flags;
    MSG m;
    UINT timer, ticks = 0;
    SweepProc sweep;
    (void)cmd;
    (void)show;
    log_text("WINRAM memory service started\n");
    if (prev)
        return 0;
    instance = h;
    sweep = start_pager(h);
#ifndef EARLY_SHELL
    flags = (GetFlags)GetProcAddress(GetModuleHandle("KERNEL"), (LPCSTR)132L);
    if (flags)
        enhanced = (flags() & 0x20) != 0;
#endif
    total = multiplex(0x5855, &kind);
    if (total == 0xffffffffUL)
        kind = 0;
    if (total == 0xffffffffUL && !enhanced)
        return 1;
    top_proc = MakeProcInstance((FARPROC)scan_top, h);
    child_proc = MakeProcInstance((FARPROC)scan_child, h);
    timer = SetTimer(NULL, 0, 400, NULL);
    if (!timer)
        return 2;
    log_text("WINRAM memory display ready\n");
    while (GetMessage(&m, NULL, 0, 0)) {
        if (m.message == WM_TIMER) {
            EnumWindows((WNDENUMPROC)top_proc, 0);
            if (sweep && ++ticks == 10) {
                ticks = 0;
                sweep();
            }
        }
        else {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
    }
    KillTimer(NULL, timer);
    FreeProcInstance(top_proc);
    FreeProcInstance(child_proc);
    return 0;
}
