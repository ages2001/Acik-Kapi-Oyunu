/* In-guest fixture diagnostics for the v1.0 release tests. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include "scsiid.h"
#include <string.h>

typedef struct {
    WORD length;
    BYTE status, bus, target, lun, cdb_length, sense_length, data_in;
    DWORD bytes, timeout, data_offset, sense_offset;
    BYTE cdb[16];
} PassThrough;
typedef struct {
    PassThrough request;
    BYTE sense[32];
    BYTE data[2048];
} Packet;
_Static_assert(sizeof(PassThrough) == 44, "32-bit SCSI_PASS_THROUGH required");

static HANDLE log_file;
static const char disk_marker[] = "NT3SCPRT AHCINT SATA DISK\r\n";
static const char cd_marker[] = "NT3SCPRT AHCINT SATA CD-ROM\r\n";
static const char nvme_marker[] = "NT3SCPRT NVME2K NAMESPACE\r\n";

static inline void line(const char *text) {
    DWORD count;
    WriteFile(log_file, text, lstrlenA(text), &count, 0);
    WriteFile(log_file, "\r\n", 2, &count, 0);
    FlushFileBuffers(log_file);
}
static inline void reg_info(HKEY parent, const char *path, const char *key) {
    HKEY sub;
    char value[120], message[200];
    DWORD type, size = sizeof(value) - 1;
    if (RegOpenKeyExA(parent, path, 0, KEY_QUERY_VALUE, &sub))
        return;
    if (!RegQueryValueExA(sub, key, 0, &type, (BYTE *)value, &size)) {
        value[size] = 0;
        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            wsprintfA(message, "%s = %s", key, value);
            line(message);
        } else if (type == REG_DWORD && size == sizeof(DWORD)) {
            DWORD number;
            CopyMemory(&number, value, sizeof(number));
            wsprintfA(message, "%s = 0x%08lx", key, number);
            line(message);
        }
    }
    RegCloseKey(sub);
}
static inline int read_target(HANDLE port, BYTE id, int optical, const char *marker) {
    Packet packet;
    DWORD attempt, count, lba = optical ? 24 : 478;
    char message[140];
    DWORD marker_size = lstrlenA(marker);
    for (attempt = 0; attempt < 3; ++attempt) {
        ZeroMemory(&packet, sizeof(packet));
        packet.request.length = sizeof(PassThrough);
        packet.request.target = id;
        packet.request.cdb_length = 10;
        packet.request.sense_length = sizeof(packet.sense);
        packet.request.data_in = 1;
        packet.request.bytes = optical ? 2048 : 512;
        packet.request.timeout = 6;
        packet.request.data_offset = offsetof(Packet, data);
        packet.request.sense_offset = offsetof(Packet, sense);
        packet.request.cdb[0] = 0x28; /* SCSI READ(10), single logical block */
        packet.request.cdb[4] = (BYTE)(lba >> 8);
        packet.request.cdb[5] = (BYTE)lba;
        packet.request.cdb[8] = 1;
        if (!DeviceIoControl(port, 0x4d004, &packet, sizeof(packet), &packet, sizeof(packet),
                             &count, 0)) {
            wsprintfA(message, "Target %u READ(10) returned Win32 error %lu", id, GetLastError());
            line(message);
            return 0;
        }
        wsprintfA(message, "Target %u LBA %lu SCSI status %u, %lu bytes", id, lba,
                  packet.request.status, packet.request.bytes);
        line(message);
        if (packet.request.status == 2)
            continue; /* First access may report unit attention. */
        if (!packet.request.status && packet.request.bytes == (optical ? 2048u : 512u) &&
            count >= offsetof(Packet, data) + packet.request.bytes &&
            !memcmp(packet.data, marker, marker_size)) {
            line(marker);
            return 1;
        }
        return 0;
    }
    return 0;
}
static inline int read_cd_volume(void) {
    char path[] = "A:\\AHCICD.TXT";
    char data[sizeof(cd_marker)];
    DWORD count;
    for (path[0] = 'C'; path[0] <= 'Z'; ++path[0]) {
        HANDLE file;
        char root[] = "C:\\";
        root[0] = path[0];
        if (GetDriveTypeA(root) != DRIVE_CDROM)
            continue;
        file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
                           0, 0);
        if (file == INVALID_HANDLE_VALUE)
            continue;
        int matched = ReadFile(file, data, sizeof(data), &count, 0) &&
                      count == sizeof(cd_marker) - 1 && !memcmp(data, cd_marker, count);
        CloseHandle(file);
        if (matched) {
            line(path);
            line(cd_marker);
            return 1;
        }
    }
    return 0;
}
static inline int read_name(HKEY key, const char *value, char *out, DWORD capacity) {
    DWORD type, size = capacity - 1;
    if (RegQueryValueExA(key, value, 0, &type, (BYTE *)out, &size) || type != REG_SZ)
        return 0;
    out[size] = 0;
    return 1;
}

#include "block.inc"
static inline void service(SC_HANDLE sc, const char *name) {
    char message[180];
    SERVICE_STATUS status;
    SC_HANDLE s = OpenServiceA(sc, name, SERVICE_QUERY_STATUS);
    if (s && QueryServiceStatus(s, &status))
        wsprintfA(message, "Service %s state %lu, Win32 exit %lu, service exit %lu", name,
                  status.dwCurrentState, status.dwWin32ExitCode, status.dwServiceSpecificExitCode);
    else
        wsprintfA(message, "Service %s query error %lu", name, GetLastError());
    line(message);
    if (s)
        CloseServiceHandle(s);
}
static inline int contents(HANDLE f, int write) {
    static BYTE expected[4096], readback[4096];
    DWORD done;
    for (DWORD page = 0; page < 256; page++) {
        DWORD i;
        for (i = 0; i < sizeof(expected); i++)
            expected[i] = (BYTE)((i * 37u) ^ (i >> 7) ^ (page * 13u));
        if (write) {
            if (!WriteFile(f, expected, sizeof(expected), &done, 0) || done != sizeof(expected))
                return 0;
        } else if (!ReadFile(f, readback, sizeof(readback), &done, 0) || done != sizeof(readback) ||
                   memcmp(expected, readback, sizeof(readback)))
            return 0;
    }
    return write ? FlushFileBuffers(f) : GetFileSize(f, 0) == 1048576;
}
/* Only volumes with a known lab marker can be written. CREATE_NEW never
   replaces a preexisting file. The second launch verifies the same bytes. */
static inline int test_volume(const char *file, const char *marker, int verify, int *write) {
    DWORD bytes;
    char name[32], root[] = "C:\\", data[96], msg[150];
    HANDLE f;
    for (; root[0] <= 'Z'; root[0]++) {
        if (GetDriveTypeA(root) != DRIVE_FIXED)
            continue;
        wsprintfA(name, "%s%s", root, file);
        f = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0,
                        0);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        int match = ReadFile(f, data, sizeof(data), &bytes, 0) &&
                    bytes == (DWORD)lstrlenA(marker) && !memcmp(data, marker, bytes);
        CloseHandle(f);
        if (!match)
            continue;
        wsprintfA(msg, "Mounted fixture %s", name);
        line(msg);
        if (verify < 0)
            return 1;
        wsprintfA(name, "%sN3WRITE.BIN", root);
        if (!verify) {
            f = CreateFileA(name, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_FLAG_WRITE_THROUGH, 0);
            if (f == INVALID_HANDLE_VALUE) {
                wsprintfA(msg, "Create test file returned %lu", GetLastError());
                line(msg);
                return 1;
            }
            int ok = contents(f, 1);
            CloseHandle(f);
            if (!ok) {
                line("Write or flush failed");
                return 1;
            }
        }
        f = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        if (f == INVALID_HANDLE_VALUE) {
            line("Cannot reopen test file");
            return 1;
        }
        *write = contents(f, 0);
        CloseHandle(f);
        wsprintfA(msg, "%s 1048576 bytes %s %s", name,
                  verify ? "after reboot" : "after write, flush, close and reopen",
                  *write ? "PASS" : "FAIL");
        line(msg);
        return 1;
    }
    return 0;
}
static inline void map(HKEY port) {
    HKEY bus, key, lun;
    char target[64], leaf[90], v[160], t[40], msg[300];
    if (RegOpenKeyExA(port, "Scsi Bus 0", 0, KEY_READ, &bus))
        return;
    for (DWORD i = 0; !RegEnumKeyA(bus, i, target, sizeof(target)); i++) {
        if (RegOpenKeyExA(bus, target, 0, KEY_READ, &key))
            continue;
        for (DWORD j = 0; !RegEnumKeyA(key, j, leaf, sizeof(leaf)); j++) {
            if (RegOpenKeyExA(key, leaf, 0, KEY_READ, &lun))
                continue;
            if (read_name(lun, "Identifier", v, sizeof(v)) &&
                read_name(lun, "Type", t, sizeof(t))) {
                wsprintfA(msg, "%s %s %s %s", target, leaf, t, v);
                line(msg);
            }
            RegCloseKey(lun);
        }
        RegCloseKey(key);
    }
    RegCloseKey(bus);
}
static inline const char *outcome(int seen, int good) {
    return seen ? (good ? "PASS" : "FAIL") : "NOT RUN (no adapter)";
}
/* Enumerate the names of the currently loaded optical-class modules. */
typedef LONG(WINAPI *SystemInfo)(ULONG, void *, ULONG, ULONG *);
static inline void optical_modules(void) {
    typedef struct {
        void *section, *mapped, *image;
        DWORD size, flags;
        WORD order, init, loads, nameoff;
        char path[256];
    } Mod;
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    SystemInfo query = nt ? (SystemInfo)(void *)GetProcAddress(nt, "NtQuerySystemInformation") : 0;
    const DWORD cap = 262144;
    BYTE *buffer = LocalAlloc(LMEM_FIXED, cap);
    if (query && buffer && query(11, buffer, cap, 0) >= 0) {
        DWORD n = *(DWORD *)buffer;
        if (n <= (cap - 4) / sizeof(Mod))
            for (DWORD i = 0; i < n; i++) {
                Mod *m = (Mod *)(buffer + 4) + i;
                m->path[255] = 0;
                if (m->nameoff >= 255)
                    continue;
                const char *name = m->path + m->nameoff;
                if (!lstrcmpiA(name, "N3CDROM.SYS") || !lstrcmpiA(name, "SCSICDRM.SYS") ||
                    !lstrcmpiA(name, "CDROM.SYS") || !lstrcmpiA(name, "CLASS2.SYS")) {
                    line("Loaded optical class module");
                    line(m->path);
                }
            }
    } else
        line("Module inventory unavailable");
    if (buffer)
        LocalFree(buffer);
}
static int io_mode(const char *command) {
    while (*command && *command != '/')
        ++command;
    if (!lstrcmpiA(command, "/write"))
        return 0;
    if (!lstrcmpiA(command, "/verify"))
        return 1;
    return -1;
}
