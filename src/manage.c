#include "adapt.h"
#include "common.h"
#include "scsiid.h"
#include <winsvc.h>

static u32 crc(const File *f) {
    u32 x = ~0u, i, j;
    for (i = 0; i < f->n; i++) {
        x ^= f->p[i];
        for (j = 0; j < 8; j++)
            x = (x >> 1) ^ (0xedb88320u & -(x & 1));
    }
    return ~x;
}
static int locations(const char *id, char *dest, char *saved, char *relative) {
    char file[16];
    wsprintfA(file, "%s.SYS", id);
    wsprintfA(relative, "System32\\drivers\\%s", file);
    if (!join(dest, drivers, file))
        return 0;
    wsprintfA(file, "%s.BKP", id);
    return join(saved, appdir, file);
}
static const char *build(const File *src, File *result) {
    result->p = 0;
    result->n = 0;
    if (src->n > INPUT_MAX)
        return "The maximum miniport size is 4 MiB.";
    result->p = LocalAlloc(LMEM_FIXED, minor == 51 ? src->n : src->n + LINK_SPACE);
    if (!result->p)
        return "Insufficient memory.";
    if (minor == 51) {
        memcpy(result->p, src->p, src->n);
        result->n = src->n;
        return 0;
    }
    return adapter(src->p, src->n, minor, result->p, &result->n);
}
static const char *check_imports(const File *image) {
    char path[MAX_PATH];
    File port;
    if (!join(path, drivers, "SCSIPORT.SYS") || !readfile(path, &port))
        return "Cannot read the installed SCSI port to verify imports.";
    const char *err = port_imports(port.p, port.n, image->p, image->n);
    LocalFree(port.p);
    return err;
}
static int own(SC_HANDLE svc, const char *path) {
    DWORD needed;
    QUERY_SERVICE_CONFIGA *cfg;
    int result = 0;
    QueryServiceConfigA(svc, 0, 0, &needed);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed > 65536)
        return 0;
    cfg = LocalAlloc(LMEM_FIXED, needed);
    if (cfg && QueryServiceConfigA(svc, cfg, needed, &needed)) {
        const char *prefix = "NT3SCPRT ";
        char boot_path[80];
        wsprintfA(boot_path, "\\SystemRoot\\%s", path);
        result = cfg->dwServiceType == SERVICE_KERNEL_DRIVER && cfg->lpBinaryPathName &&
                 (!lstrcmpiA(cfg->lpBinaryPathName, path) ||
                  !lstrcmpiA(cfg->lpBinaryPathName, boot_path)) &&
                 cfg->lpDisplayName && strlen(cfg->lpDisplayName) >= strlen(prefix) &&
                 !memcmp(cfg->lpDisplayName, prefix, strlen(prefix));
    }
    if (cfg)
        LocalFree(cfg);
    return result;
}
static const char *stage(const char *id, const char *display, const File *src, const File *image,
                         int boot) {
    char dest[MAX_PATH], saved[MAX_PATH], path[48];
    SC_HANDLE sc = 0, svc = 0;
    const char *err = 0;
    int newdest = 0, newsaved = 0;
    if (!locations(id, dest, saved, path))
        return "The installation path is too long.";
    sc = OpenSCManagerA(0, 0, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
    if (!sc)
        return "Administrator access is required to register a storage driver.";
    svc = OpenServiceA(sc, id, SERVICE_QUERY_CONFIG | (boot ? SERVICE_CHANGE_CONFIG : 0));
    if (svc && !own(svc, path)) {
        err = "The service name is already in use.";
        goto done;
    }
    if (!svc && GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST) {
        err = "Cannot check the existing service.";
        goto done;
    }
    if (!matches(saved, src)) {
        if (!create(saved, src)) {
            err = "Cannot create the miniport backup. An existing different file is preserved.";
            goto done;
        }
        newsaved = 1;
        if (!matches(saved, src)) {
            err = "Backup verification failed.";
            goto done;
        }
    }
    if (!matches(dest, image)) {
        if (!create(dest, image)) {
            err = "Cannot create the adapter. An existing different file is preserved.";
            goto done;
        }
        newdest = 1;
        if (!matches(dest, image)) {
            err = "Adapter verification failed.";
            goto done;
        }
    }
    if (svc && boot &&
        !ChangeServiceConfigA(svc, SERVICE_KERNEL_DRIVER, SERVICE_BOOT_START, SERVICE_ERROR_NORMAL,
                              path, "SCSI miniport", 0, 0, 0, 0, display)) {
        err = "Windows could not set boot start for the selected driver.";
        goto done;
    }
    if (!svc)
        svc = CreateServiceA(sc, id, display, SERVICE_QUERY_CONFIG, SERVICE_KERNEL_DRIVER,
                             boot ? SERVICE_BOOT_START : SERVICE_SYSTEM_START, SERVICE_ERROR_NORMAL,
                             path, "SCSI miniport", 0, 0, 0, 0);
    if (!svc)
        err = "Windows could not register the storage driver.";
done:
    if (err) {
        if (newdest)
            DeleteFileA(dest);
        if (newsaved)
            DeleteFileA(saved);
    }
    if (svc)
        CloseServiceHandle(svc);
    CloseServiceHandle(sc);
    return err;
}
const char *add_driver(const char *path, char *name, int boot) {
    File src, image;
    char display[64], id[10];
    const char *err, *file = path, *p;
    for (p = path; *p; p++)
        if (*p == '\\' || *p == '/')
            file = p + 1;
    if (!*file || strlen(file) > 12)
        return "Select a miniport with an 8.3 file name.";
    if (!readfile(path, &src))
        return "Cannot read this miniport (maximum 4 MiB).";
    wsprintfA(id, "N3%06lX", (unsigned long)(crc(&src) & 0xffffff));
    err = build(&src, &image);
    if (!err)
        err = check_imports(&image);
    if (!err) {
        wsprintfA(display, "NT3SCPRT %s (%s)", file, id);
        err = stage(id, display, &src, &image, boot);
        if (!err)
            lstrcpyA(name, id);
    }
    if (image.p)
        LocalFree(image.p);
    LocalFree(src.p);
    return err;
}
static const char *retire(const char *dest, const char *id, const File *image) {
    char file[16], retired[MAX_PATH];
    wsprintfA(file, "%s.DEL", id);
    if (!join(retired, drivers, file))
        return "Invalid driver path.";
    if (!missing(retired) && !matches(retired, image))
        return "A different file already occupies the removal path.";
    if (!missing(dest)) {
        if (!matches(dest, image))
            return "The installed driver has changed.";
        if (!missing(retired) || !MoveFileA(dest, retired))
            return "Cannot retire the loaded miniport. Restart and run Remove again.";
    }
    if (!missing(retired) && !DeleteFileA(retired) &&
        !MoveFileExA(retired, 0, MOVEFILE_DELAY_UNTIL_REBOOT))
        return "Cannot schedule the retired miniport for removal.";
    return 0;
}
static const char *unregister(const char *id, const File *src) {
    char dest[MAX_PATH], saved[MAX_PATH], path[48];
    File image;
    SC_HANDLE sc = 0, svc = 0;
    const char *err = 0;
    DWORD e;
    if (!locations(id, dest, saved, path))
        return "Invalid installation path.";
    err = build(src, &image);
    if (!err && !missing(dest) && !matches(dest, &image))
        err = "The adapter has changed. Its service and backup were preserved.";
    if (err)
        goto done;
    sc = OpenSCManagerA(0, 0, SC_MANAGER_CONNECT);
    if (!sc) {
        err = "Cannot open the service manager.";
        goto done;
    }
    svc = OpenServiceA(sc, id, DELETE | SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG);
    if (svc) {
        if (!own(svc, path))
            err = "The service configuration has changed.";
        else if (!ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE,
                                       0, 0, 0, 0, 0, 0, 0))
            err = "Cannot disable the miniport service.";
        else if (!DeleteService(svc) && GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE)
            err = "Cannot remove the driver service.";
    } else {
        e = GetLastError();
        if (e != ERROR_SERVICE_DOES_NOT_EXIST && e != ERROR_SERVICE_MARKED_FOR_DELETE)
            err = "Cannot open the driver service.";
    }
    /* NT 3.1 may keep a deleted service's image mapped through the next
       loader pass. Rename it first so the old service path cannot reload it. */
    if (!err && minor == 10)
        err = retire(dest, id, &image);
    else if (!err && !missing(dest) && !DeleteFileA(dest) &&
             !MoveFileExA(dest, 0, MOVEFILE_DELAY_UNTIL_REBOOT))
        err = "Cannot remove the adapter file.";
    if (!err && !DeleteFileA(saved))
        err = "Cannot remove the saved miniport copy.";
done:
    if (svc)
        CloseServiceHandle(svc);
    if (sc)
        CloseServiceHandle(sc);
    if (image.p)
        LocalFree(image.p);
    return err;
}
/* The disk class supplies this address for a SCSI-backed system volume.
   A native IDE volume may reject the SCSI-specific control. */
const char *check_system_disk(void) {
    char root[MAX_PATH], volume[] = "\\\\.\\C:", key[MAX_PATH], owner[64], name[16],
                         backup[MAX_PATH];
    BYTE address[8];
    DWORD count, n = GetWindowsDirectoryA(root, sizeof(root));
    if (!n || n >= sizeof(root) || root[1] != ':')
        return "Cannot identify the Windows system volume.";
    volume[4] = root[0];
    HANDLE disk = CreateFileA(volume, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                              OPEN_EXISTING, 0, 0);
    if (disk == INVALID_HANDLE_VALUE)
        return "Cannot open the system volume to check its storage controller.";
    BOOL ok = DeviceIoControl(disk, 0x41018, 0, 0, address, sizeof(address), &count, 0);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(disk);
    if (!ok) {
        if (error == ERROR_INVALID_FUNCTION || error == ERROR_NOT_SUPPORTED ||
            error == ERROR_INVALID_PARAMETER)
            return 0;
        return "Cannot check the storage controller for the system volume.";
    }
    if (count != sizeof(address))
        return "Invalid SCSI address for the system volume.";
    wsprintfA(key, "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port %u", address[4]);
    HKEY port;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &port))
        return "Cannot identify the SCSI adapter owning the system volume.";
    ok = port_owner(port, owner, sizeof(owner));
    RegCloseKey(port);
    if (!ok)
        return "Cannot identify the SCSI miniport owning the system volume.";
    if (strlen(owner) != 8 || owner[0] != 'N' || owner[1] != '3')
        return 0;
    for (unsigned i = 2; i < 8; ++i)
        if (!((owner[i] >= '0' && owner[i] <= '9') || (owner[i] >= 'A' && owner[i] <= 'F') ||
              (owner[i] >= 'a' && owner[i] <= 'f')))
            return 0;
    wsprintfA(name, "%s.BKP", owner);
    if (join(backup, appdir, name) && !missing(backup))
        return "The running Windows volume uses an NT3SCPRT driver. Boot it through an independent "
               "controller before removal.";
    return 0;
}
const char *remove_drivers(void) {
    char pattern[MAX_PATH], p[MAX_PATH], id[9];
    WIN32_FIND_DATAA f;
    HANDLE h;
    const char *err = 0;
    if (!join(pattern, appdir, "N3??????.BKP"))
        return "Invalid program path.";
    h = FindFirstFileA(pattern, &f);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND
                   ? 0
                   : "Cannot enumerate saved miniports.";
    }
    do {
        File src;
        char wanted[9];
        if (strlen(f.cFileName) != 12 || !join(p, appdir, f.cFileName) || !readfile(p, &src)) {
            err = "Cannot read a saved miniport.";
            break;
        }
        memcpy(id, f.cFileName, 8);
        id[8] = 0;
        wsprintfA(wanted, "N3%06lX", (unsigned long)(crc(&src) & 0xffffff));
        if (lstrcmpiA(wanted, id))
            err = "A saved miniport has changed.";
        else
            err = unregister(id, &src);
        LocalFree(src.p);
    } while (!err && FindNextFileA(h, &f));
    if (!err && GetLastError() != ERROR_NO_MORE_FILES)
        err = "Cannot finish enumerating miniports.";
    FindClose(h);
    return err;
}
