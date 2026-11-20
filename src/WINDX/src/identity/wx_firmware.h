#ifndef WINDX_FIRMWARE_IDENTITY_H
#define WINDX_FIRMWARE_IDENTITY_H

#include "wx_identity.h"

struct wx_firmware_identity {
    char manufacturer[128], model[128], bios[384];
};

struct wx_firmware_fields {
    char maker[128], product[128], bios_vendor[96], revision[192], date[32], platform[96];
};

static BOOL wx_firmware_string(char *dest, unsigned capacity, const char *value, unsigned length)
{
    static const char *const placeholders[] = {
        "Not Specified", "Unknown", "None", "Not Available", "Default String",
        "System Manufacturer", "System Product Name", "To be filled by O.E.M."
    };
    if (dest[0] || !length || length >= capacity) return FALSE;
    for (unsigned i = 0; i < length; ++i) dest[i] = value[i];
    dest[length] = 0;
    wx_identity_simplify(dest);
    for (unsigned i = 0; i < sizeof(placeholders)/sizeof(*placeholders); ++i) {
        if (!lstrcmpiA(dest, placeholders[i])) {
            dest[0] = 0;
            break;
        }
    }
    return dest[0] != 0;
}

static DWORD wx_firmware_u32(const BYTE *p)
{
    return (DWORD)p[0] | (DWORD)p[1] << 8 | (DWORD)p[2] << 16 | (DWORD)p[3] << 24;
}

static void wx_smbios_string(char *dest, unsigned capacity, const BYTE *from, const BYTE *end, unsigned index)
{
    if (!index) return;
    while (from < end && *from) {
        const BYTE *next = from;
        while (next < end && *next) ++next;
        if (next >= end) return;
        if (!--index) {
            wx_firmware_string(dest, capacity, (const char *)from, next - from);
            return;
        }
        from = next + 1;
    }
}

/* RAW_SMBIOS_DATA as returned by GetSystemFirmwareTable('RSMB', 0).
 * Walk only complete DMTF records and their double-NUL-terminated string sets. */
static void wx_smbios_fields(const BYTE *raw, DWORD size, struct wx_firmware_fields *fields)
{
    const BYTE *p, *end;
    DWORD bytes;
    if (size < 8 || raw[1] < 2) return;
    bytes = wx_firmware_u32(raw + 4);
    if (bytes > size - 8) return;
    p = raw + 8;
    end = p + bytes;
    while (end - p >= 4) {
        const BYTE *strings, *next;
        unsigned length = p[1];
        if (length < 4 || length > (SIZE_T)(end - p)) return;
        strings = p + length;
        next = strings;
        while (end - next >= 2 && (next[0] || next[1])) ++next;
        if (end - next < 2) return;
        next += 2;
        if (p[0] == 0 && length > 8) {
            wx_smbios_string(fields->bios_vendor, sizeof(fields->bios_vendor), strings, next, p[4]);
            wx_smbios_string(fields->revision, sizeof(fields->revision), strings, next, p[5]);
            wx_smbios_string(fields->date, sizeof(fields->date), strings, next, p[8]);
        } else if (p[0] == 1 && length > 5) {
            wx_smbios_string(fields->maker, sizeof(fields->maker), strings, next, p[4]);
            wx_smbios_string(fields->product, sizeof(fields->product), strings, next, p[5]);
        } else if (p[0] == 127) return;
        p = next;
    }
}

static void wx_firmware_table(struct wx_firmware_fields *fields)
{
    typedef UINT (WINAPI *table_fn)(DWORD, DWORD, void *, DWORD);
    table_fn table = (table_fn)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetSystemFirmwareTable");
    const DWORD rsmb = 0x52534d42u;
    DWORD size = table ? table(rsmb, 0, NULL, 0) : 0;
    BYTE *data;
    if (size >= 8 && (data = HeapAlloc(GetProcessHeap(), 0, size))) {
        DWORD used = table(rsmb, 0, data, size);
        if (used && used <= size) wx_smbios_fields(data, used, fields);
        HeapFree(GetProcessHeap(), 0, data);
    }
}

static void wx_firmware_registry_table(struct wx_firmware_fields *fields)
{
    HKEY key;
    DWORD type = 0, size = 0;
    BYTE *data;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\mssmbios\\Data",
                      0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
    if (RegQueryValueExA(key, "SMBiosData", NULL, &type, NULL, &size) == ERROR_SUCCESS &&
            type == REG_BINARY && size >= 8 && (data = HeapAlloc(GetProcessHeap(), 0, size))) {
        DWORD used = size;
        if (RegQueryValueExA(key, "SMBiosData", NULL, &type, data, &used) == ERROR_SUCCESS &&
                type == REG_BINARY && used <= size) wx_smbios_fields(data, used, fields);
        HeapFree(GetProcessHeap(), 0, data);
    }
    RegCloseKey(key);
}

static void wx_firmware_value(HKEY key, const char *name, char *dest, unsigned capacity)
{
    char data[384] = {0};
    if (!dest[0] && wx_identity_reg(key, name, REG_SZ, data, sizeof(data)))
        wx_firmware_string(dest, capacity, data, lstrlenA(data));
}

static void wx_firmware_registry_oem(const char *path, struct wx_firmware_fields *fields)
{
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
    wx_firmware_value(key, "SystemManufacturer", fields->maker, sizeof(fields->maker));
    wx_firmware_value(key, "SystemProductName", fields->product, sizeof(fields->product));
    wx_firmware_value(key, "BIOSVendor", fields->bios_vendor, sizeof(fields->bios_vendor));
    wx_firmware_value(key, "BIOSVersion", fields->revision, sizeof(fields->revision));
    wx_firmware_value(key, "BIOSReleaseDate", fields->date, sizeof(fields->date));
    RegCloseKey(key);
}

static void wx_firmware_registry_legacy(struct wx_firmware_fields *fields)
{
    HKEY key;
    DWORD type, bytes;
    char values[768];
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return;
    wx_firmware_value(key, "Identifier", fields->platform, sizeof(fields->platform));
    wx_firmware_value(key, "SystemBiosDate", fields->date, sizeof(fields->date));
    bytes = sizeof(values);
    if (!fields->revision[0] &&
            RegQueryValueExA(key, "SystemBiosVersion", NULL, &type, (BYTE *)values, &bytes) == ERROR_SUCCESS &&
            (type == REG_MULTI_SZ || type == REG_SZ) && bytes && bytes <= sizeof(values)) {
        struct wx_text text = {fields->revision, sizeof(fields->revision)};
        for (unsigned start = 0, end; start < bytes && values[start]; start = end + 1) {
            for (end = start; end < bytes && values[end]; ++end) {}
            if (end == bytes) break;
            wx_identity_simplify(values + start);
            if (values[start]) {
                if (fields->revision[0]) wx_identity_text(&text, " / ");
                wx_identity_text(&text, values + start);
            }
            if (type == REG_SZ) break;
        }
    }
    RegCloseKey(key);
}

static void wx_read_firmware_identity(struct wx_firmware_identity *out)
{
    struct wx_firmware_fields fields = {0};
    struct wx_text maker = {out->manufacturer, sizeof(out->manufacturer)};
    struct wx_text model = {out->model, sizeof(out->model)};
    struct wx_text bios = {out->bios, sizeof(out->bios)};
    const char *unavailable = "Not reported by firmware or Windows";
    out->manufacturer[0] = out->model[0] = out->bios[0] = 0;
    wx_firmware_table(&fields);
    if (!fields.maker[0] || !fields.product[0] || !fields.revision[0])
        wx_firmware_registry_table(&fields);
    wx_firmware_registry_oem("HARDWARE\\DESCRIPTION\\System\\BIOS", &fields);
    wx_firmware_registry_oem("SYSTEM\\CurrentControlSet\\Control\\SystemInformation", &fields);
    wx_firmware_registry_legacy(&fields);

    wx_identity_text(&maker, fields.maker[0] ? fields.maker : unavailable);
    if (fields.product[0]) wx_identity_text(&model, fields.product);
    else if (fields.platform[0]) {
        wx_identity_text(&model, fields.platform);
        wx_identity_text(&model, " (Windows platform identifier)");
    } else wx_identity_text(&model, unavailable);

    if (fields.bios_vendor[0]) wx_identity_text(&bios, fields.bios_vendor);
    if (fields.revision[0]) {
        if (fields.bios_vendor[0]) wx_identity_text(&bios, " ");
        wx_identity_text(&bios, fields.revision);
    }
    if (fields.date[0]) {
        wx_identity_text(&bios, out->bios[0] ? ", " : "ROM BIOS dated ");
        wx_identity_text(&bios, fields.date);
    }
    if (!out->bios[0]) wx_identity_text(&bios, unavailable);
}

#endif
