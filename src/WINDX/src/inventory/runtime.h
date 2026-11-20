#ifndef WINDX_RUNTIME_INVENTORY_H
#define WINDX_RUNTIME_INVENTORY_H

/* Compiles in both the Wine DxDiag provider and the standalone installer.
 * A complete four-part runtime version is kept distinct from the graphics
 * API and device feature level returned by the installed WINDX renderer. */
#define WX_INVENTORY_KEY "Software\\WINDX"
#define WX_INVENTORY_DIR "RuntimeDirectory"

struct wx_runtime_inventory {
    DWORD major, minor;
    char letter[2], registered[48], label[84], graphics[92], text[194];
};

static void wx_inventory_copy(char *to, const char *from, DWORD capacity)
{
    if (!capacity) return;
    DWORD n = 0;
    while (n + 1 < capacity && from[n]) { to[n] = from[n]; ++n; }
    to[n] = 0;
}

static BOOL wx_inventory_registry(HKEY root, const char *key, const char *name, char *out, DWORD capacity)
{
    HKEY opened;
    DWORD type = 0, size = capacity;
    out[0] = 0;
    if (RegOpenKeyExA(root, key, 0, KEY_QUERY_VALUE, &opened) != ERROR_SUCCESS) return FALSE;
    LONG status = RegQueryValueExA(opened, name, NULL, &type, (BYTE *)out, &size);
    RegCloseKey(opened);
    if (status != ERROR_SUCCESS || type != REG_SZ || !size || size > capacity || (size == capacity && out[capacity - 1]) || !out[0]) {
        out[0] = 0;
        return FALSE;
    }
    out[capacity - 1] = 0;
    return TRUE;
}

static BOOL wx_version_numbers(const char *s, unsigned fields[4])
{
    for (unsigned i = 0; i != 4; ++i) {
        unsigned v = 0;
        if (*s < '0' || *s > '9') return FALSE;
        do {
            unsigned digit = *s++ - '0';
            if (v > (65535 - digit) / 10) return FALSE;
            v = 10 * v + digit;
        } while (*s >= '0' && *s <= '9');
        fields[i] = v;
        if (i != 3 && *s++ != '.') return FALSE;
    }
    return !*s;
}

static void wx_registered_runtime(struct wx_runtime_inventory *inventory)
{
    unsigned fields[4];
    const char *label = NULL;
    if (!wx_inventory_registry(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\DirectX", "Version",
            inventory->registered, sizeof(inventory->registered))) {
        /* NT 4.0 uses the documented older four-byte binary InstalledVersion. */
        HKEY directx;
        DWORD type = 0, bytes = sizeof(DWORD), legacy = 0;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\DirectX", 0, KEY_QUERY_VALUE, &directx) == ERROR_SUCCESS) {
            LONG error = RegQueryValueExA(directx, "InstalledVersion", NULL, &type, (BYTE *)&legacy, &bytes);
            RegCloseKey(directx);
            if (error == ERROR_SUCCESS && (type == REG_BINARY || type == REG_DWORD) && bytes == sizeof(legacy) &&
                    legacy >= 1 && legacy <= 3) {
                inventory->major = legacy;
                wsprintfA(inventory->label, "DirectX %lu", legacy);
            }
        }
        return;
    }
    if (!wx_version_numbers(inventory->registered, fields)) {
        inventory->registered[0] = 0;
        return;
    }
    if (fields[0] == 4) {
        switch (fields[1]) {
        case 2: inventory->major = 1; label = "1"; break;
        case 3: inventory->major = 2; label = "2"; break;
        case 4: inventory->major = 3; label = "3"; break;
        case 5: inventory->major = 5; label = "5"; break;
        case 6:
            inventory->major = 6;
            if (fields[2] == 0) label = "6.0";
            if (fields[2] == 2 || fields[2] == 3) {
                inventory->minor = 1;
                if (fields[2] == 3) inventory->letter[0] = 'a';
                label = fields[2] == 2 ? "6.1" : "6.1a";
            }
            break;
        case 7: inventory->major = 7; label = "7.0"; break;
        case 8:
            inventory->major = 8;
            if (fields[2] <= 1) {
                inventory->minor = fields[2];
                label = fields[2] ? "8.1" : "8.0";
            }
            break;
        case 9:
            inventory->major = 9;
            if (fields[2] != 0) break;
            switch (fields[3]) {
            case 900: label = "9.0"; break;
            case 901: label = "9.0a"; inventory->letter[0] = 'a'; break;
            case 902: label = "9.0b"; inventory->letter[0] = 'b'; break;
            case 904: label = "9.0c"; inventory->letter[0] = 'c'; break;
            }
            break;
        }
    }
    if (label) wsprintfA(inventory->label, "DirectX %s (%s)", label, inventory->registered);
    else wsprintfA(inventory->label, "Registered DirectX runtime %s", inventory->registered);
}

static BOOL wx_component_path(char buffer[MAX_PATH], const char *directory, const char *name)
{
    int length = lstrlenA(directory);
    if (!length || length + lstrlenA(name) + 2 > MAX_PATH) return FALSE;
    lstrcpyA(buffer, directory);
    if (buffer[length - 1] != '\\' && buffer[length - 1] != '/') lstrcatA(buffer, "\\");
    lstrcatA(buffer, name);
    DWORD attr = GetFileAttributesA(buffer);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL wx_renderer_directory(const char *dir)
{
    char path[MAX_PATH];
    if (!dir[0]) return FALSE;
    if (wx_component_path(path, dir, "wxrender.exe") && wx_component_path(path, dir, "wined3d.dll")) return TRUE;
    return wx_component_path(path, dir, "wxddraw.exe") && wx_component_path(path, dir, "ddraw.dll");
}

/* Rendering software may initialize slowly on the oldest targets. A warm
 * diagnostic run must not be the prerequisite for registering a cold one.
 * Wait while it makes CPU progress; also allow five minutes of disk or driver
 * waits before treating the child as unresponsive. */
static DWORD wx_wait_for_device(HANDLE process)
{
    typedef BOOL (WINAPI *times_fn)(HANDLE,FILETIME *,FILETIME *,FILETIME *,FILETIME *);
    times_fn times = (times_fn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcessTimes");
    FILETIME old_kernel = {0}, old_user = {0};
    DWORD began = GetTickCount(), changed = began;
    for (;;) {
        DWORD waited = WaitForSingleObject(process, 10000);
        if (waited != WAIT_TIMEOUT) return waited;
        DWORD now = GetTickCount();
        FILETIME created, ended, kernel, user;
        if (times && times(process, &created, &ended, &kernel, &user) &&
                (old_kernel.dwLowDateTime != kernel.dwLowDateTime || old_kernel.dwHighDateTime != kernel.dwHighDateTime ||
                 old_user.dwLowDateTime != user.dwLowDateTime || old_user.dwHighDateTime != user.dwHighDateTime)) {
            changed = now;
            old_kernel = kernel;
            old_user = user;
        }
        if ((now - changed >= 300000) || (now - began >= 900000)) return WAIT_TIMEOUT;
    }
}

/* A separate process gives the graphics DLLs their normal application search
 * path on NT 3.1, which does not implement LOAD_WITH_ALTERED_SEARCH_PATH for
 * their transitive dependencies. It also prevents a DxDiag host's already
 * loaded system DLLs from replacing the application's local D3D modules. */
static BOOL wx_child_query(const char *directory, const char *operation, char *label, DWORD size)
{
    char executable[MAX_PATH], command[2 * MAX_PATH];
    if (!wx_renderer_directory(directory) || !wx_component_path(executable, directory, "wxinv.exe")) return FALSE;
    wsprintfA(command, "\"%s\" %s", executable, operation);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE read_pipe = NULL, write_pipe = NULL, reader = NULL, null_device = INVALID_HANDLE_VALUE;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    BOOL ok = FALSE;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 512)) return FALSE;
    if (!DuplicateHandle(GetCurrentProcess(), read_pipe, GetCurrentProcess(), &reader, 0, FALSE, DUPLICATE_SAME_ACCESS))
        goto done;
    CloseHandle(read_pipe);
    read_pipe = NULL;
    null_device = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (null_device == INVALID_HANDLE_VALUE) goto done;
    ZeroMemory(&si, sizeof(si)); ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; si.hStdOutput = write_pipe; si.hStdError = null_device; si.hStdInput = null_device;
    if (!CreateProcessA(executable, command, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE,
            NULL, directory, &si, &pi)) goto done;
    CloseHandle(write_pipe);
    write_pipe = NULL;
    DWORD waited = wx_wait_for_device(pi.hProcess), status = 1;
    if (waited == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, ERROR_TIMEOUT);
    if (waited == WAIT_OBJECT_0 && GetExitCodeProcess(pi.hProcess, &status) && status == 0) {
        DWORD received = 0;
        if (ReadFile(reader, label, size - 1, &received, NULL) && received && received < size) {
            label[received] = 0;
            ok = TRUE;
        }
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
done:
    if (!ok) label[0] = 0;
    if (reader) CloseHandle(reader);
    if (read_pipe) CloseHandle(read_pipe);
    if (write_pipe) CloseHandle(write_pipe);
    if (null_device != INVALID_HANDLE_VALUE) CloseHandle(null_device);
    return ok;
}

static void wx_runtime_directory(const char *requested, char directory[MAX_PATH])
{
    directory[0] = 0;
    if (requested) {
        if (lstrlenA(requested) < MAX_PATH) lstrcpyA(directory, requested);
        return;
    }
    if (wx_inventory_registry(HKEY_CURRENT_USER, WX_INVENTORY_KEY, WX_INVENTORY_DIR, directory, MAX_PATH) &&
            wx_renderer_directory(directory)) return;
    DWORD n = GetModuleFileNameA(NULL, directory, MAX_PATH);
    if (!n || n >= MAX_PATH) directory[0] = 0;
    else {
        while (n && directory[n - 1] != '\\' && directory[n - 1] != '/') --n;
        if (n > 3) --n;
        directory[n] = 0;
    }

}

static void wx_query_runtime(const char *requested, struct wx_runtime_inventory *inventory)
{
    char directory[MAX_PATH];
    ZeroMemory(inventory, sizeof(*inventory));
    wx_registered_runtime(inventory);
    wx_runtime_directory(requested, directory);
    wx_child_query(directory, "/device", inventory->graphics, sizeof(inventory->graphics));
    if (inventory->label[0] && inventory->graphics[0])
        wsprintfA(inventory->text, "%s / %s", inventory->label, inventory->graphics);
    else if (inventory->label[0]) lstrcpyA(inventory->text, inventory->label);
    else if (inventory->graphics[0]) lstrcpyA(inventory->text, inventory->graphics);
    else lstrcpyA(inventory->text, "No registered DirectX runtime");
}
#endif
