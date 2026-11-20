#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "runtime.h"
#include "device.h"
#include "display.h"

void *memset(void *buffer, int value, size_t length)
{
    BYTE *p = buffer;
    while (length--) *p++ = (BYTE)value;
    return buffer;
}

static void print(const char *s)
{
    DWORD written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, lstrlenA(s), &written, NULL);
}

static char *argument(char **tail, char *dest, UINT capacity)
{
    char *p = *tail;
    UINT n = 0;
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) { *tail = p; return NULL; }
    BOOL quoted = *p == '"';
    if (quoted) ++p;
    while (*p && (quoted ? *p != '"' : (*p != ' ' && *p != '\t'))) {
        if (n + 1 >= capacity) return NULL;
        dest[n++] = *p++;
    }
    if (quoted && *p++ != '"') return NULL;
    dest[n] = 0;
    *tail = p;
    return dest;
}

void WINAPI inventory_entry(void)
{
    char *tail = GetCommandLineA(), exe[MAX_PATH], op[20] = "", supplied[MAX_PATH], path[MAX_PATH];
    struct wx_runtime_inventory inventory;
    if (!argument(&tail, exe, sizeof(exe))) ExitProcess(2);
    const char *directory = NULL;
    BOOL store = FALSE, show_display = FALSE;
    if (argument(&tail, op, sizeof(op))) {
        if (!lstrcmpiA(op, "/renderer")) {
            char gl[128];
            DWORD n = GetCurrentDirectoryA(MAX_PATH, path);
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
            if (!n || n >= MAX_PATH || !wx_measure_renderer(path, gl, sizeof(gl))) ExitProcess(1);
            print(gl);
            ExitProcess(0);
        }
        if (!lstrcmpiA(op, "/device")) {
            char graphics[92] = {0};
            DWORD length = GetCurrentDirectoryA(MAX_PATH, path);
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
            if (!length || length >= MAX_PATH || !wx_renderer_directory(path)) ExitProcess(1);
            if (!wx_probe_device12(path, graphics) && !wx_probe_device11(path, graphics) &&
                    !wx_probe_device10(path, TRUE, graphics) && !wx_probe_device10(path, FALSE, graphics) &&
                    !wx_probe_legacy(path, graphics))
                ExitProcess(1);
            print(graphics);
            ExitProcess(0);
        }
        if (!lstrcmpiA(op, "/register")) store = TRUE;
        else if (!lstrcmpiA(op, "/display")) show_display = TRUE;
        else if (lstrcmpiA(op, "/probe")) {
            print("Usage: wxinv.exe [/probe [directory] | /display [directory] | /register directory]\r\n");
            ExitProcess(2);
        }
        if (argument(&tail, supplied, sizeof(supplied))) {
            DWORD size = GetFullPathNameA(supplied, MAX_PATH, path, NULL);
            if (!size || size >= MAX_PATH) ExitProcess(2);
            if (size > 3 && (path[size-1] == '\\' || path[size-1] == '/')) path[size-1] = 0;
            directory = path;
        }
        while (*tail == ' ' || *tail == '\t') ++tail;
        if ((store && !directory) || *tail) ExitProcess(2);
    }
    if (show_display) {
        struct wx_display_inventory d;
        wx_display_inventory_read(0, directory, &d);
        print("Display: "); print(d.description[0] ? d.description : "Not reported"); print("\r\n");
        print("Driver: "); print(d.driver[0] ? d.driver : "Not reported"); print("\r\n");
        print("Desktop mode: "); print(d.mode[0] ? d.mode : "Not reported"); print("\r\n");
        print("GL_RENDERER: "); print(d.renderer[0] ? d.renderer : "Not reported"); print("\r\n");
        ExitProcess(0);
    }
    wx_query_runtime(directory, &inventory);
    print(inventory.text);
    print("\r\n");
    if (store) {
        HKEY key;
        LONG status;
        if (!inventory.graphics[0]) {
            print("A WINDX graphics device could not be opened from the supplied directory.\r\n");
            ExitProcess(1);
        }
        status = RegCreateKeyExA(HKEY_CURRENT_USER, WX_INVENTORY_KEY, 0, NULL, 0,
                KEY_SET_VALUE, NULL, &key, NULL);
        if (status == ERROR_SUCCESS) {
            status = RegSetValueExA(key, WX_INVENTORY_DIR, 0, REG_SZ, (const BYTE *)directory,
                    lstrlenA(directory) + 1);
            RegCloseKey(key);
        }
        if (status != ERROR_SUCCESS) { print("Unable to register the WINDX runtime directory.\r\n"); ExitProcess(1); }
        print("Registered WINDX runtime directory: ");
        print(directory);
        print("\r\n");
    }
    ExitProcess(0);
}
