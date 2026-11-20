#ifndef WINDX_DISPLAY_INVENTORY_H
#define WINDX_DISPLAY_INVENTORY_H

struct wx_display_inventory {
    char adapter[192], device[128], driver[128], chip[128], dac[96], renderer[128], manufacturer[96];
    char native_device_id[256], native_key[256], driver_version[64], description[384], mode[96], memory[80];
    DWORD width, height, bits, hz;
    BOOL software, active_windx;
};

static int wx_ascii_prefix(const char *s, const char *prefix)
{
    while (*prefix) {
        unsigned a = (unsigned char)*s++, b = (unsigned char)*prefix++;
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return FALSE;
    }
    return TRUE;
}

static BOOL wx_display_value(HKEY key, const char *name, char *out, DWORD capacity)
{
    BYTE bytes[512] = {0};
    DWORD type = 0, size = sizeof(bytes) - 2;
    if (RegQueryValueExA(key, name, NULL, &type, bytes, &size) || !size) return FALSE;
    if (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ) {
        wx_inventory_copy(out, (const char *)bytes, capacity);
    } else if (type == REG_BINARY && size >= 2 && !(size & 1) && !bytes[1]) {
        /* Older NT display miniports store their Unicode identification strings
         * as REG_BINARY. Limit conversion to the value's actual byte length. */
        int count = 0;
        while ((DWORD)(2 * count) < size && ((const WCHAR *)bytes)[count]) ++count;
        if (!count) return FALSE;
        int written = WideCharToMultiByte(CP_ACP, 0, (const WCHAR *)bytes, count, out, capacity - 1, NULL, NULL);
        if (!written) return FALSE;
        out[written] = 0;
    } else return FALSE;
    return out[0] != 0;
}

static void wx_display_registry(UINT index, struct wx_display_inventory *d)
{
    const char *prefix = "\\Registry\\Machine\\";
    HKEY map, adapter;
    char video[36], path[512] = {0};
    wsprintfA(video, "\\Device\\Video%u", index);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\VIDEO", 0, KEY_QUERY_VALUE, &map)) return;
    BOOL found = wx_display_value(map, video, path, sizeof(path));
    RegCloseKey(map);
    if (!found || !wx_ascii_prefix(path, prefix)) return;
    if (!d->device[0]) wx_inventory_copy(d->device, video, sizeof(d->device));
    if (!d->native_key[0]) wx_inventory_copy(d->native_key, path, sizeof(d->native_key));
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path + lstrlenA(prefix), 0, KEY_QUERY_VALUE, &adapter)) return;
    if (!d->adapter[0]) wx_display_value(adapter, "HardwareInformation.AdapterString", d->adapter, sizeof(d->adapter));
    wx_display_value(adapter, "HardwareInformation.ChipType", d->chip, sizeof(d->chip));
    wx_display_value(adapter, "HardwareInformation.DacType", d->dac, sizeof(d->dac));
    wx_display_value(adapter, "HardwareInformation.Manufacturer", d->manufacturer, sizeof(d->manufacturer));
    wx_display_value(adapter, "InstalledDisplayDrivers", d->driver, sizeof(d->driver));
    DWORD bytes = sizeof(DWORD), type = 0, memory = 0;
    if (RegQueryValueExA(adapter, "HardwareInformation.MemorySize", NULL, &type, (BYTE *)&memory, &bytes) == ERROR_SUCCESS &&
            (type == REG_DWORD || type == REG_BINARY) && bytes == sizeof(memory) && memory)
        wsprintfA(d->memory, "%lu KiB", memory / 1024u);
    RegCloseKey(adapter);
    if (d->driver[0]) {
        if (!d->adapter[0]) wsprintfA(d->adapter, "Windows display driver (%s)", d->driver);
        BOOL extension = FALSE;
        for (const char *p = d->driver; *p; ++p) if (*p == '.') extension = TRUE;
        if (!extension && lstrlenA(d->driver) + 4 < sizeof(d->driver)) lstrcatA(d->driver, ".dll");
    } else if (!d->adapter[0]) {
        /* The loaded service name is still an identity when no card name
         * was provided by the miniport. Avoid substituting a PCI card name. */
        for (const char *p = path; *p; ++p) if (wx_ascii_prefix(p, "\\Services\\")) {
            char service[80];
            unsigned n = 0;
            p += 10;
            while (*p && *p != '\\' && n + 1 < sizeof(service)) service[n++] = *p++;
            service[n] = 0;
            if (n) wsprintfA(d->adapter, "Windows display service (%s)", service);
            break;
        }
    }
}

/* Old Windows 9x installations may expose the adapter class in the registry
 * before they implement EnumDisplayDevices. Only use a single installed class
 * entry when no enumeration associated a name with the primary desktop. */
static void wx_legacy_display_class(struct wx_display_inventory *d)
{
    HKEY list, adapter;
    char first[80], second[80];
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\Class\\Display",
            0, KEY_READ, &list) != ERROR_SUCCESS) return;
    if (RegEnumKeyA(list, 0, first, sizeof(first)) == ERROR_SUCCESS &&
            RegEnumKeyA(list, 1, second, sizeof(second)) == ERROR_NO_MORE_ITEMS &&
            RegOpenKeyExA(list, first, 0, KEY_QUERY_VALUE, &adapter) == ERROR_SUCCESS) {
        wx_display_value(adapter, "DriverDesc", d->adapter, sizeof(d->adapter));
        if (!d->adapter[0]) wx_display_value(adapter, "DeviceDesc", d->adapter, sizeof(d->adapter));
        RegCloseKey(adapter);
    }
    RegCloseKey(list);
}

static void wx_display_driver_version(struct wx_display_inventory *d)
{
    if (!d->driver[0]) return;
    char dir[MAX_PATH], path[MAX_PATH];
    UINT n = GetSystemDirectoryA(dir, MAX_PATH);
    if (!n || n >= MAX_PATH || !wx_component_path(path, dir, d->driver)) return;
    HMODULE module = LoadLibraryA("version.dll");
    if (!module) return;
    DWORD (WINAPI *size)(const char *,DWORD *) = (void *)GetProcAddress(module,"GetFileVersionInfoSizeA");
    BOOL (WINAPI *get)(const char *,DWORD,DWORD,void *) = (void *)GetProcAddress(module,"GetFileVersionInfoA");
    BOOL (WINAPI *query)(const void *,const char *,void **,UINT *) = (void *)GetProcAddress(module,"VerQueryValueA");
    DWORD ignored = 0, bytes = size ? size(path,&ignored) : 0;
    BYTE *buf = bytes && bytes <= (1u<<20) ? HeapAlloc(GetProcessHeap(),0,bytes) : NULL;
    VS_FIXEDFILEINFO *fixed = NULL;
    UINT length = 0;
    if (buf && get && query && get(path,0,bytes,buf) && query(buf,"\\",(void **)&fixed,&length) &&
            length >= sizeof(*fixed) && fixed && fixed->dwSignature == 0xfeef04bd)
        wsprintfA(d->driver_version,"%u.%u.%u.%u",HIWORD(fixed->dwFileVersionMS),LOWORD(fixed->dwFileVersionMS),
                HIWORD(fixed->dwFileVersionLS),LOWORD(fixed->dwFileVersionLS));
    if (buf) HeapFree(GetProcessHeap(),0,buf);
    FreeLibrary(module);
}

static void wx_display_inventory_read(UINT index, const char *renderer_dir, struct wx_display_inventory *d)
{
    ZeroMemory(d, sizeof(*d));
    typedef BOOL (WINAPI *enum_fn)(const char *,DWORD,DISPLAY_DEVICEA *,DWORD);
    enum_fn enumerate = (enum_fn)GetProcAddress(GetModuleHandleA("user32.dll"), "EnumDisplayDevicesA");
    DISPLAY_DEVICEA device;
    ZeroMemory(&device, sizeof(device));
    device.cb = sizeof(device);
    if (enumerate && enumerate(NULL, index, &device, 0)) {
        wx_inventory_copy(d->device, device.DeviceName, sizeof(d->device));
        wx_inventory_copy(d->adapter, device.DeviceString, sizeof(d->adapter));
        wx_inventory_copy(d->native_device_id, device.DeviceID, sizeof(d->native_device_id));
        wx_inventory_copy(d->native_key, device.DeviceKey, sizeof(d->native_key));
    }
    wx_display_registry(index, d);
    if (!index && !d->adapter[0] && (GetVersion() & 0x80000000u)) wx_legacy_display_class(d);
    wx_display_driver_version(d);
    HDC dc = index && d->device[0] ? CreateDCA("DISPLAY", d->device, NULL, NULL) : index ? NULL : GetDC(NULL);
    if (dc) {
        d->width = GetDeviceCaps(dc,HORZRES);
        d->height = GetDeviceCaps(dc,VERTRES);
        d->bits = GetDeviceCaps(dc,PLANES) * GetDeviceCaps(dc,BITSPIXEL);
        int refresh = GetDeviceCaps(dc,VREFRESH);
        if (refresh > 1) d->hz = refresh;
        if (index) DeleteDC(dc); else ReleaseDC(NULL,dc);
    }
    if (d->width && d->height && d->bits) {
        wsprintfA(d->mode, "%lu x %lu (%lu bit)", d->width, d->height, d->bits);
        if (d->hz) wsprintfA(d->mode + lstrlenA(d->mode), " (%lu Hz)", d->hz);
    }
    if (!index) {
        char directory[MAX_PATH];
        wx_runtime_directory(renderer_dir, directory);
        d->active_windx = wx_renderer_directory(directory);
        if (d->active_windx) wx_child_query(directory, "/renderer", d->renderer, sizeof(d->renderer));
    }
    d->active_windx |= GetModuleHandleA("wined3d.dll") != NULL;
    d->software = d->renderer[0] && (wx_ascii_prefix(d->renderer, "softpipe") ||
            wx_ascii_prefix(d->renderer, "llvmpipe") || wx_ascii_prefix(d->renderer, "software rasterizer") ||
            wx_ascii_prefix(d->renderer, "GDI Generic") || wx_ascii_prefix(d->renderer, "swrast"));
    if (d->renderer[0]) {
        if (d->adapter[0]) wsprintfA(d->description, "%s / WINDX %s (%s)", d->adapter,
                d->software ? "software renderer" : "renderer", d->renderer);
        else wsprintfA(d->description, "WINDX %s (%s)", d->software ? "software renderer" : "renderer", d->renderer);
    } else if (d->adapter[0]) lstrcpyA(d->description, d->adapter);
    else if (d->active_windx) lstrcpyA(d->description, "WINDX Direct3D compatibility renderer");
    if (!d->memory[0] && d->software) lstrcpyA(d->memory, "Shared system memory");
}
#endif
