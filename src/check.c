/* Read the known data from the secondary devices created by lab.py. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "scsiid.h"
#include <stddef.h>
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
static const char disk_marker[] = "NT3SCPRT SCSI DISK READ OK\r\n";
static const char cd_marker[] = "NT3SCPRT SCSI CD READ OK\r\n";
static const char nvme_marker[] = "NT3SCPRT NVME READ OK\r\n";

static void line(const char *text) {
    DWORD count;
    WriteFile(log_file, text, lstrlenA(text), &count, 0);
    WriteFile(log_file, "\r\n", 2, &count, 0);
    FlushFileBuffers(log_file);
}
static void reg_info(HKEY parent, const char *path, const char *key) {
    HKEY sub;
    char value[120], message[200];
    DWORD type, size = sizeof(value) - 1;
    if (RegOpenKeyExA(parent, path, 0, KEY_QUERY_VALUE, &sub))
        return;
    if (!RegQueryValueExA(sub, key, 0, &type, (BYTE *)value, &size)) {
        value[size] = 0;
        if (type == REG_SZ) {
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
static int read_target(HANDLE port, BYTE id, int optical, const char *marker) {
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
        if (!packet.request.status && packet.request.bytes >= marker_size &&
            count >= offsetof(Packet, data) + marker_size &&
            !memcmp(packet.data, marker, marker_size)) {
            line(marker);
            return 1;
        }
        return 0;
    }
    return 0;
}
static int read_cd_volume(void) {
    char path[] = "A:\\N3CD.TXT";
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
void start(void) {
    static const char version[] = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    static const char control[] = "SYSTEM\\CurrentControlSet\\Control\\Windows";
    char path[128], driver[64];
    int disk = 0, cd = 0, nvme = 0, ncr_seen = 0;
    DWORD i;
    char *command = GetCommandLineA();
    while (*command && *command != '/')
        ++command;
    int buslogic = !lstrcmpiA(command, "/buslogic");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    log_file = CreateFileA(buslogic ? "A:\\BUSLOGIC.LOG" : "A:\\CHECK.LOG", GENERIC_WRITE,
                           FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (log_file == INVALID_HANDLE_VALUE)
        ExitProcess(2);
    reg_info(HKEY_LOCAL_MACHINE, version, "CurrentVersion");
    reg_info(HKEY_LOCAL_MACHINE, version, "CurrentBuildNumber");
    reg_info(HKEY_LOCAL_MACHINE, version, "CSDVersion");
    reg_info(HKEY_LOCAL_MACHINE, control, "CSDVersion");
    for (i = 0; i < 32; ++i) {
        HKEY key;
        HANDLE port;
        wsprintfA(path, "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port %lu", i);
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key))
            continue;
        int found = port_owner(key, driver, sizeof(driver));
        RegCloseKey(key);
        if (!found)
            continue;
        int ncr = !lstrcmpiA(driver, "N3DF8825");
        if (buslogic) {
            if (lstrcmpiA(driver, "N359C39D"))
                continue;
        } else if (!ncr && lstrcmpiA(driver, "N3DF5470"))
            continue;
        line(path);
        line(driver);
        ncr_seen |= ncr;
        wsprintfA(path, "\\\\.\\Scsi%lu:", i);
        port = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           0, OPEN_EXISTING, 0, 0);
        if (port == INVALID_HANDLE_VALUE) {
            wsprintfA(path, "Open port returned Win32 error %lu", GetLastError());
            line(path);
            continue;
        }
        if (buslogic) {
            disk |= read_target(port, 1, 0, disk_marker);
        } else if (ncr) {
            disk |= read_target(port, 1, 0, disk_marker);
            cd |= read_target(port, 3, 1, cd_marker);
        } else {
            nvme |= read_target(port, 0, 0, nvme_marker);
        }
        CloseHandle(port);
    }
    if (buslogic) {
        line(disk ? "BusLogic disk PASS" : "BusLogic disk FAIL");
        CloseHandle(log_file);
        ExitProcess(disk ? 0 : 1);
    }
    if (ncr_seen && !cd)
        cd = read_cd_volume();
    wsprintfA(path, "SCSI disk %s / SCSI CD %s / NVMe %s", disk ? "PASS" : "FAIL",
              cd ? "PASS" : "FAIL", nvme ? "PASS" : "FAIL");
    line(path);
    CloseHandle(log_file);
    ExitProcess(disk && cd && nvme ? 0 : 1);
}
