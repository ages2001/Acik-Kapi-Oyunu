#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>

/* Keep the diagnostic usable before the common controls and the newer CRTs. */
enum { SYSTEM, DISPLAY, SOUND, INPUT_PAGE, FILES, TESTS, SECTION_COUNT };
enum { PAGE = 101, RUN, SAVE, FINISHED = WM_APP + 1 };
static const char *const headings[] = {"System", "Display", "Sound and music", "Input", "DirectX files", "Functional tests"};
static const char *const elements[] = {"SystemInformation", "DisplayDevices", "SoundDevices", "InputDevices", "DirectXFiles", "Diagnostics"};
static char sections[SECTION_COUNT][12288], appdir[MAX_PATH];
static HWND pages, viewer, run_button, save_button;
static HINSTANCE instance;
static BOOL testing;

static void add(unsigned page, const char *s)
{
    unsigned used = lstrlenA(sections[page]);
    while (*s && used + 1 < sizeof(sections[page])) sections[page][used++] = *s++;
    sections[page][used] = 0;
}

static void field(unsigned page, const char *name, const char *value)
{
    add(page, name); add(page, ": "); add(page, value); add(page, "\r\n");
}

static void number(unsigned page, const char *name, DWORD value)
{
    char buf[16];
    wsprintfA(buf, "%lu", value);
    field(page, name, buf);
}

static BOOL app_path(char *dest, const char *name)
{
    if (lstrlenA(appdir) + lstrlenA(name) >= MAX_PATH) return FALSE;
    lstrcpyA(dest, appdir);
    lstrcatA(dest, name);
    return TRUE;
}

static void inventory(void)
{
    OSVERSIONINFOA os = { sizeof(os) };
    SYSTEM_INFO si;
    char buf[256];
    HMODULE kernel = GetModuleHandleA("kernel32.dll");
    BOOL (WINAPI *version)(OSVERSIONINFOA *) = (void *)GetProcAddress(kernel, "GetVersionExA");
    void (WINAPI *native_info)(SYSTEM_INFO *) = (void *)GetProcAddress(kernel, "GetNativeSystemInfo");
    if (!version || !version(&os)) {
        DWORD v = GetVersion();
        os.dwMajorVersion = LOBYTE(LOWORD(v));
        os.dwMinorVersion = HIBYTE(LOWORD(v));
        os.dwPlatformId = v & 0x80000000 ? VER_PLATFORM_WIN32_WINDOWS : VER_PLATFORM_WIN32_NT;
        os.dwBuildNumber = v & 0x80000000 ? 0 : HIWORD(v);
    }
    if (os.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) os.dwBuildNumber &= 0xffff;
    wsprintfA(buf, "%s %lu.%lu (build %lu)", os.dwPlatformId == VER_PLATFORM_WIN32_NT ?
            "Windows NT" : "Windows", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);
    field(SYSTEM, "Operating system", buf);
    if (*os.szCSDVersion) field(SYSTEM, "Service release", os.szCSDVersion);
    if (native_info) native_info(&si); else GetSystemInfo(&si);
    number(SYSTEM, "Logical processors", si.dwNumberOfProcessors);
    field(SYSTEM, "Processor architecture", si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ?
            "AMD64" : si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL ? "x86" : "Other");
    field(SYSTEM, "Diagnostic process", sizeof(void *) == 8 ? "64-bit" : "32-bit");
    MEMORYSTATUSEX extended = { sizeof(extended) };
    MEMORYSTATUS memory = { sizeof(memory) };
    BOOL (WINAPI *extended_memory)(MEMORYSTATUSEX *) =
            (void *)GetProcAddress(kernel, "GlobalMemoryStatusEx");
    DWORD physical_mib;
    if (extended_memory && extended_memory(&extended))
        physical_mib = (DWORD)(extended.ullTotalPhys >> 20);
    else {
        GlobalMemoryStatus(&memory);
        physical_mib = (DWORD)(memory.dwTotalPhys >> 20);
    }
    wsprintfA(buf, "%lu MiB", physical_mib);
    field(SYSTEM, "Physical memory", buf);
    field(SYSTEM, "Executable directory", appdir);
    field(SYSTEM, "Diagnostic", "WINDX 1.0 development");
    HDC dc = GetDC(NULL);
    if (dc) {
        wsprintfA(buf, "%d x %d, %d bits per pixel", GetDeviceCaps(dc, HORZRES), GetDeviceCaps(dc, VERTRES),
                GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES));
        field(DISPLAY, "Current mode", buf);
        ReleaseDC(NULL, dc);
    }
    BOOL (WINAPI *display_device)(LPCSTR, DWORD, DISPLAY_DEVICEA *, DWORD) =
            (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "EnumDisplayDevicesA");
    /* NT 3.x exported an earlier interface with this name. */
    BOOL multiple_displays = os.dwPlatformId == VER_PLATFORM_WIN32_NT ? os.dwMajorVersion >= 5 :
            os.dwMajorVersion > 4 || (os.dwMajorVersion == 4 && os.dwMinorVersion >= 10);
    if (display_device && multiple_displays) {
        DISPLAY_DEVICEA device = { sizeof(device) };
        for (DWORD i = 0; display_device(NULL, i, &device, 0); ++i)
            field(DISPLAY, device.DeviceName, device.DeviceString);
    } else field(DISPLAY, "Adapter", "Primary display");
    add(DISPLAY, "Run functional tests for the OpenGL renderer, Direct3D feature level and pixel readback.\r\n");
    HMODULE mm = LoadLibraryA("winmm.dll");
    if (mm) {
        typedef UINT (WINAPI *count_fn)(void);
        count_fn wave_out = (count_fn)GetProcAddress(mm, "waveOutGetNumDevs");
        count_fn wave_in = (count_fn)GetProcAddress(mm, "waveInGetNumDevs");
        count_fn midi_out = (count_fn)GetProcAddress(mm, "midiOutGetNumDevs");
        MMRESULT (WINAPI *wave_caps)(UINT_PTR, WAVEOUTCAPSA *, UINT) = (void *)GetProcAddress(mm, "waveOutGetDevCapsA");
        UINT count = wave_out ? wave_out() : 0;
        if (wave_out) number(SOUND, "Wave output devices", count);
        for (UINT i = 0; wave_caps && i < count; ++i) {
            WAVEOUTCAPSA caps;
            if (wave_caps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) field(SOUND, "Output", caps.szPname);
        }
        if (wave_in) number(SOUND, "Wave input devices", wave_in());
        if (midi_out) number(SOUND, "MIDI output devices", midi_out());
        FreeLibrary(mm);
    } else field(SOUND, "Windows multimedia", "Unavailable");
    number(INPUT_PAGE, "Keyboard type", GetKeyboardType(0));
    number(INPUT_PAGE, "Function keys", GetKeyboardType(2));
    number(INPUT_PAGE, "Mouse buttons", GetSystemMetrics(SM_CMOUSEBUTTONS));

    static const char *const modules[] = {"ddraw.dll","d3dim.dll","d3drm.dll","d3d8.dll","d3d9.dll",
            "d3d10.dll","d3d10_1.dll","d3d10core.dll","d3d11.dll","d3d12.dll","dxgi.dll","dxdiagn.dll",
            "d3dcompiler_43.dll","d3dcompiler_47.dll","d3dx9_43.dll","d3dx10_43.dll","d3dx11_43.dll",
            "dsound.dll","dinput.dll","dinput8.dll","dplayx.dll","dpnet.dll","dmusic.dll","quartz.dll",
            "xinput1_3.dll","xaudio2_7.dll","xactengine3_7.dll","d2d1.dll","dwrite.dll","dxva2.dll","opengl32.dll"};
    for (unsigned i = 0; i < sizeof(modules)/sizeof(modules[0]); ++i) {
        char local[MAX_PATH], resolved[MAX_PATH];
        BOOL local_file = app_path(local, modules[i]) && GetFileAttributesA(local) != INVALID_FILE_ATTRIBUTES;
        if (!local_file) {
            const char *alias = NULL;
            if (!lstrcmpA(modules[i], "d3d10core.dll")) alias = "d10core.dll";
            if (!lstrcmpA(modules[i], "d3dcompiler_43.dll")) alias = "wxcomp43.dll";
            if (alias && app_path(local, alias) && GetFileAttributesA(local) != INVALID_FILE_ATTRIBUTES)
                local_file = TRUE;
        }
        HMODULE dll = LoadLibraryA(local_file ? local : modules[i]);
        if (dll && GetModuleFileNameA(dll, resolved, MAX_PATH)) field(FILES, modules[i], resolved);
        else {
            wsprintfA(buf, "%s (Windows error %lu)", local_file ? "Local DLL failed to load" : "Unavailable", GetLastError());
            field(FILES, modules[i], buf);
        }
        if (dll) FreeLibrary(dll);
    }
    add(TESTS, "No functional tests have run in this session.\r\n");
}

static void probe(const char *exe, const char *arg)
{
    char path[MAX_PATH], command[MAX_PATH + 32], temp[MAX_PATH], file[MAX_PATH], chunk[512];
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    STARTUPINFOA start = {sizeof(start)};
    PROCESS_INFORMATION child;
    DWORD n, code;
    add(TESTS, "\r\n"); add(TESTS, exe);
    if (*arg) { add(TESTS, " "); add(TESTS, arg); }
    add(TESTS, "\r\n");
    if (!app_path(path, exe) || GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        add(TESTS, "Companion executable is absent.\r\n"); return;
    }
    n = GetTempPathA(MAX_PATH, temp);
    if (!n || n >= MAX_PATH || !GetTempFileNameA(temp, "wdx", 0, file)) {
        add(TESTS, "Cannot create a temporary output file.\r\n"); return;
    }
    HANDLE out = CreateFileA(file, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE in = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    start.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    start.wShowWindow = SW_HIDE;
    start.hStdInput = in;
    start.hStdOutput = out;
    /* The rendering libraries can write large debug traces to standard error. */
    HANDLE err = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    start.hStdError = err;
    wsprintfA(command, "\"%s\" %s", path, arg);
    if (out != INVALID_HANDLE_VALUE && in != INVALID_HANDLE_VALUE && err != INVALID_HANDLE_VALUE &&
            CreateProcessA(path, command, NULL, NULL, TRUE, 0, NULL, appdir, &start, &child)) {
        CloseHandle(child.hThread);
        DWORD wait = WaitForSingleObject(child.hProcess, 120000);
        if (wait == WAIT_TIMEOUT) {
            TerminateProcess(child.hProcess, 124);
            WaitForSingleObject(child.hProcess, 5000);
            add(TESTS, "Test timed out.\r\n");
        }
        if (SetFilePointer(out, 0, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER)
            while (ReadFile(out, chunk, sizeof(chunk) - 1, &n, NULL) && n) { chunk[n] = 0; add(TESTS, chunk); }
        if (GetExitCodeProcess(child.hProcess, &code)) number(TESTS, "Exit code", code);
        CloseHandle(child.hProcess);
    } else number(TESTS, "Cannot start test, Windows error", GetLastError());
    if (err != INVALID_HANDLE_VALUE) CloseHandle(err);
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    DeleteFileA(file);
}

static DWORD WINAPI run_tests(void *window)
{
    sections[TESTS][0] = 0;
    probe("wxtest.exe", ""); probe("wxgl.exe", ""); probe("wxearly.exe", "");
    probe("wxold3d.exe", ""); probe("wxd3d81.exe", "");
    probe("wxrender.exe", "9"); probe("wxrender.exe", "10"); probe("wxrender.exe", "10.1");
    probe("wxrender.exe", "11"); probe("wxrender.exe", "cs"); probe("wx12test.exe", ""); probe("wxredist.exe", "");
    probe("early\\wxddraw.exe", ""); probe("wxmedia.exe", "");
    if (window) PostMessageA((HWND)window, FINISHED, 0, 0);
    return 0;
}

static BOOL write_bytes(HANDLE file, const void *data, DWORD size)
{
    DWORD written;
    return WriteFile(file, data, size, &written, NULL) && written == size;
}
static BOOL write_text(HANDLE file, const char *s) { return write_bytes(file, s, lstrlenA(s)); }

/* XML is UTF-16LE, so device names from the guest's ANSI code page survive export. */
static BOOL wide_text(HANDLE file, const char *s, BOOL escape)
{
    static WCHAR wide[16384];
    int count = MultiByteToWideChar(CP_ACP, 0, s, -1, wide, sizeof(wide)/sizeof(wide[0]));
    if (!count) return FALSE;
    if (!escape) return write_bytes(file, wide, (count - 1) * sizeof(WCHAR));
    int first = 0;
    for (int i = 0; i < count - 1; ++i) {
        const WCHAR *entity = wide[i] == '&' ? L"&amp;" : wide[i] == '<' ? L"&lt;" : wide[i] == '>' ? L"&gt;" : NULL;
        if (entity || (wide[i] < 32 && wide[i] != 9 && wide[i] != 10 && wide[i] != 13)) {
            if (!write_bytes(file, wide + first, (i - first) * sizeof(WCHAR))) return FALSE;
            if (entity) {
                unsigned length = 0;
                while (entity[length]) ++length;
                if (!write_bytes(file, entity, length * sizeof(WCHAR))) return FALSE;
            }
            first = i + 1;
        }
    }
    return write_bytes(file, wide + first, (count - 1 - first) * sizeof(WCHAR));
}

static BOOL save_report(const char *name, BOOL xml)
{
    HANDLE file = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    const WCHAR bom = 0xfeff;
    BOOL ok = xml ? write_bytes(file, &bom, sizeof(bom)) &&
            wide_text(file, "<?xml version=\"1.0\" encoding=\"utf-16\"?>\r\n<WindxCompatibilityTests>\r\n", FALSE) :
            write_text(file, "WINDX Compatibility Tests\r\n");
    for (unsigned i = 0; ok && i < SECTION_COUNT; ++i) {
        if (xml) {
            char tag[96];
            wsprintfA(tag, "  <%s><Notes>", elements[i]);
            ok = wide_text(file, tag, FALSE) && wide_text(file, sections[i], TRUE);
            wsprintfA(tag, "</Notes></%s>\r\n", elements[i]);
            ok = ok && wide_text(file, tag, FALSE);
        } else ok = write_text(file, "\r\n[") && write_text(file, headings[i]) &&
                write_text(file, "]\r\n") && write_text(file, sections[i]);
    }
    if (ok && xml) ok = wide_text(file, "</WindxCompatibilityTests>\r\n", FALSE);
    return CloseHandle(file) && ok;
}

static void show_page(void)
{
    int selected = (int)SendMessageA(pages, LB_GETCURSEL, 0, 0);
    if (selected >= 0 && selected < SECTION_COUNT)
        SetWindowTextA(viewer, selected == TESTS && testing ? "Running the WINDX tests..." : sections[selected]);
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        pages = CreateWindowExA(0, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LBS_NOTIFY,
                0,0,0,0,window,(HMENU)PAGE,instance,NULL);
        viewer = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,0,0,0,0,window,NULL,instance,NULL);
        run_button = CreateWindowExA(0,"BUTTON","Run tests",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,window,(HMENU)RUN,instance,NULL);
        save_button = CreateWindowExA(0,"BUTTON","Save report...",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,window,(HMENU)SAVE,instance,NULL);
        for (int i = 0; i < SECTION_COUNT; ++i) SendMessageA(pages, LB_ADDSTRING, 0, (LPARAM)headings[i]);
        SendMessageA(pages, LB_SETCURSEL, 0, 0); show_page(); return 0;
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(pages,8,8,120,h > 54 ? h-54 : 1,TRUE);
        MoveWindow(viewer,136,8,w>144?w-144:1,h>54?h-54:1,TRUE);
        MoveWindow(run_button,8,h-36,110,26,TRUE); MoveWindow(save_button,136,h-36,120,26,TRUE); return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == PAGE) show_page();
        if (LOWORD(wp) == RUN && !testing) {
            testing = TRUE; EnableWindow(run_button,FALSE); EnableWindow(save_button,FALSE);
            SendMessageA(pages,LB_SETCURSEL,TESTS,0); show_page();
            DWORD thread_id;
            HANDLE worker = CreateThread(NULL,0,run_tests,window,0,&thread_id);
            if (worker) CloseHandle(worker); else SendMessageA(window,FINISHED,0,0);
        }
        if (LOWORD(wp) == SAVE && !testing) {
            char name[MAX_PATH] = "WindxTests.txt";
            OPENFILENAMEA dialog = {0};
            dialog.lStructSize = OPENFILENAME_SIZE_VERSION_400;
            dialog.hwndOwner = window; dialog.lpstrFile = name; dialog.nMaxFile = sizeof(name);
            dialog.lpstrFilter = "Text report (*.txt)\0*.txt\0XML report (*.xml)\0*.xml\0";
            dialog.nFilterIndex = 1; dialog.lpstrDefExt = "txt";
            dialog.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
            HMODULE common = LoadLibraryA("comdlg32.dll");
            BOOL (WINAPI *choose)(OPENFILENAMEA *) = common ? (void *)GetProcAddress(common,"GetSaveFileNameA") : NULL;
            if (choose && choose(&dialog) && !save_report(name, dialog.nFilterIndex == 2))
                MessageBoxA(window,"Could not write the report.","WINDX",MB_OK|MB_ICONEXCLAMATION);
            if (common) FreeLibrary(common);
        }
        return 0;
    case FINISHED:
        testing = FALSE; EnableWindow(run_button,TRUE); EnableWindow(save_button,TRUE); show_page(); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(window,msg,wp,lp);
}

static const char *argument(const char *s, char *out, unsigned capacity)
{
    unsigned n = 0;
    BOOL quote = FALSE;
    while (*s == ' ' || *s == '\t') ++s;
    while (*s && (quote || (*s != ' ' && *s != '\t'))) {
        if (*s == '"') quote = !quote;
        else if (n + 1 < capacity) out[n++] = *s;
        else return NULL;
        ++s;
    }
    out[n] = 0;
    return quote ? NULL : s;
}

void WINAPI diag_entry(void)
{
    char arg[MAX_PATH], output[MAX_PATH] = "";
    const char *args = argument(GetCommandLineA(), arg, sizeof(arg));
    BOOL tests = FALSE, xml = FALSE;
    const char *usage = "wxdiag [/test] [/t filename | /x filename]\r\nWithout a filename, opens the diagnostic window.";
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    while (args && (args = argument(args, arg, sizeof(arg))) && *arg) {
        if (!lstrcmpiA(arg,"/test")) tests = TRUE;
        else if (!lstrcmpiA(arg,"/t") || !lstrcmpiA(arg,"/x")) {
            if (*output) { args = NULL; break; }
            xml = !lstrcmpiA(arg,"/x");
            args = argument(args, output, sizeof(output));
            if (!args || !*output) { args = NULL; break; }
        } else if (!lstrcmpiA(arg,"/?")) { MessageBoxA(NULL,usage,"WINDX Compatibility Tests",MB_OK); ExitProcess(0); }
        else { args = NULL; break; }
    }
    if (!args) ExitProcess(2);
    DWORD n = GetModuleFileNameA(NULL,appdir,sizeof(appdir));
    if (!n || n >= sizeof(appdir)) ExitProcess(1);
    while (n && appdir[n-1] != '\\' && appdir[n-1] != '/') --n;
    appdir[n] = 0;
    instance = GetModuleHandleA(NULL);
    inventory();
    if (tests) run_tests(NULL);
    if (*output) ExitProcess(save_report(output,xml) ? 0 : 1);
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = window_proc; wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL,IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1); wc.lpszClassName = "WindxDiag";
    if (!RegisterClassA(&wc)) ExitProcess(1);
    HWND window = CreateWindowExA(0,wc.lpszClassName,"WINDX Compatibility Tests",WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,CW_USEDEFAULT,620,410,NULL,NULL,instance,NULL);
    if (!window) ExitProcess(1);
    ShowWindow(window,SW_SHOW); UpdateWindow(window);
    MSG msg;
    while (GetMessageA(&msg,NULL,0,0) > 0) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    ExitProcess(0);
}
