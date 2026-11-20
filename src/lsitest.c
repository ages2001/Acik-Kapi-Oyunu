#include "relbase.h"
void start(void) {
    char *cmd = GetCommandLineA(), path[260], drv[90], dir[MAX_PATH];
    DWORD v = GetVersion();
    int reads = 0, written = 0, cd = 0, found = 0, buslogic = 0;
    while (*cmd && *cmd != '/')
        cmd++;
    int verify = io_mode(cmd), boot = !lstrcmpiA(cmd, "/boot");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    log_file = CreateFileA(verify == 1 ? "A:\\LVERIFY.LOG"
                           : boot      ? "A:\\BOOT.LOG"
                                       : "A:\\LSIO.LOG",
                           GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    if (log_file == INVALID_HANDLE_VALUE)
        ExitProcess(2);
    wsprintfA(path, "Windows %u.%u build %u", LOBYTE(LOWORD(v)), HIBYTE(LOWORD(v)),
              HIWORD(v) & 32767);
    line(path);
    reg_info(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Windows", "CSDVersion");
    GetSystemDirectoryA(dir, sizeof(dir));
    wsprintfA(path, "%s\\NTOSKRNL.EXE", dir);
    HANDLE f =
        CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (f != INVALID_HANDLE_VALUE) {
        BYTE mz[2];
        DWORD got;
        if (ReadFile(f, mz, 2, &got, 0) && got == 2 && mz[0] == 'M' && mz[1] == 'Z') {
            line("Loaded installation and read system kernel");
            line(path);
        }
        CloseHandle(f);
    }
    SC_HANDLE sc = OpenSCManagerA(0, 0, SC_MANAGER_CONNECT);
    const char *names[] = {"N35BEEDD", "N3DF8825", "N3ACEC56", "N359C39D",
                           "N3679182", "N3DF5470", "Scsicdrm", "Scsidisk"};
    for (int j = 0; j < 8; j++) {
        if (sc)
            service(sc, names[j]);
        wsprintfA(path, "SYSTEM\\CurrentControlSet\\Services\\%s", names[j]);
        reg_info(HKEY_LOCAL_MACHINE, path, "ImagePath");
        reg_info(HKEY_LOCAL_MACHINE, path, "Start");
    }
    if (sc)
        CloseServiceHandle(sc);
    optical_modules();
    for (DWORD i = 0; i < 32; i++) {
        HKEY k;
        wsprintfA(path, "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port %lu", i);
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &k))
            continue;
        drv[0] = 0;
        port_owner(k, drv, sizeof(drv));
        line(path);
        line(drv);
        map(k);
        RegCloseKey(k);
        int bus = !lstrcmpiA(drv, "N359C39D");
        if (boot || (!bus && lstrcmpiA(drv, "N35BEEDD") && lstrcmpiA(drv, "N3DF8825") &&
                     lstrcmpiA(drv, "N3ACEC56")))
            continue;
        found = 1;
        buslogic |= bus;
        wsprintfA(path, "\\\\.\\Scsi%lu:", i);
        f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                        OPEN_EXISTING, 0, 0);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        const char *mark = bus ? "NT3SCPRT SCSI DISK READ OK\r\n" : disk_marker;
        if (read_target(f, 1, 0, mark)) {
            reads = 1;
            if (verify >= 0)
                written |= scratch(f, 1, verify, mark);
        }
        if (!bus)
            cd |= read_target(f, 3, 1, cd_marker);
        CloseHandle(f);
    }
    if (found && !written) {
        const char *mark = buslogic ? "NT3SCPRT SCSI DISK READ OK\r\n" : disk_marker;
        reads |= test_volume(buslogic ? "N3SCSI.TXT" : "AHCI.TXT", mark, verify, &written);
    }
    int volume = read_cd_volume();
    if (!boot) {
        wsprintfA(path,
                  "SCSI disk read %s / scratch write verification %s / optical data %s / mounted "
                  "optical file %s",
                  reads ? "PASS" : "FAIL",
                  verify < 0 ? "not requested" : (written ? "PASS" : "FAIL"),
                  (cd || volume) ? "PASS" : "FAIL", volume ? "PASS" : "FAIL");
        line(path);
    } else
        line("Boot diagnostic complete");
    CloseHandle(log_file);
    ExitProcess(0);
}
