#include "relbase.h"
void start(void) {
    static const char version[] = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    static const char control[] = "SYSTEM\\CurrentControlSet\\Control\\Windows";
    char path[160], driver[64], *cmd = GetCommandLineA();
    int ah = 0, nv = 0, disk = 0, cd = 0, namespace = 0, aw = 0, nw = 0;
    while (*cmd && *cmd != '/')
        cmd++;
    int verify = io_mode(cmd);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    log_file = CreateFileA(verify == 1 ? "A:\\VERIFY.LOG" : "A:\\READ.LOG", GENERIC_WRITE,
                           FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    if (log_file == INVALID_HANDLE_VALUE)
        ExitProcess(2);
    DWORD v = GetVersion();
    wsprintfA(path, "GetVersion %u.%u build %lu", LOBYTE(LOWORD(v)), HIBYTE(LOWORD(v)),
              (v >> 16) & 32767);
    line(path);
    reg_info(HKEY_LOCAL_MACHINE, version, "CurrentVersion");
    reg_info(HKEY_LOCAL_MACHINE, version, "CurrentBuildNumber");
    reg_info(HKEY_LOCAL_MACHINE, version, "CSDVersion");
    reg_info(HKEY_LOCAL_MACHINE, control, "CSDVersion");
    SC_HANDLE sc = OpenSCManagerA(0, 0, SC_MANAGER_CONNECT);
    if (sc) {
        service(sc, "AHCINT");
        service(sc, "N3679182");
        service(sc, "NVME2K");
        service(sc, "N3DF5470");
        CloseServiceHandle(sc);
    }
    for (DWORD i = 0; i < 32; i++) {
        HKEY key;
        wsprintfA(path, "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port %lu", i);
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key))
            continue;
        driver[0] = 0;
        port_owner(key, driver, sizeof(driver));
        line(path);
        line(driver);
        map(key);
        RegCloseKey(key);
        int a = !lstrcmpiA(driver, "AHCINT") || !lstrcmpiA(driver, "N3679182");
        int n = !lstrcmpiA(driver, "NVME2K") || !lstrcmpiA(driver, "N3DF5470");
        if (!a && !n)
            continue;
        ah |= a;
        nv |= n;
        wsprintfA(path, "\\\\.\\Scsi%lu:", i);
        HANDLE port = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        if (port == INVALID_HANDLE_VALUE) {
            wsprintfA(path, "Port open error %lu", GetLastError());
            line(path);
            continue;
        }
        if (a) {
            disk |= read_target(port, 0, 0, disk_marker);
            if (disk && verify >= 0)
                aw |= scratch(port, 0, verify, disk_marker);
            cd |= read_target(port, 1, 1, cd_marker);
        }
        if (n) {
            namespace |= read_target(port, 0, 0, nvme_marker);
            if (namespace && verify >= 0)
                nw |= scratch(port, 0, verify, nvme_marker);
        }
        CloseHandle(port);
    }
    if (ah) {
        if (!aw)
            disk |= test_volume("AHCI.TXT", disk_marker, verify, &aw);
        if (!cd)
            cd = read_cd_volume();
    }
    if (nv && !nw)
    namespace |= test_volume("NVME.TXT", nvme_marker, verify, &nw);
    wsprintfA(path, "AHCINT SATA disk %s / SATA CD %s / disk write %s", outcome(ah, disk),
              outcome(ah, cd), verify < 0 ? "not requested" : outcome(ah, aw));
    line(path);
    wsprintfA(path, "nvme2k namespace %s / disk write %s", outcome(nv, namespace),
              verify < 0 ? "not requested" : outcome(nv, nw));
    line(path);
    CloseHandle(log_file);
    ExitProcess(0);
}
