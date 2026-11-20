#include "classmap.h"
#include "common.h"
#include <winsvc.h>

static const char service_key[] = "SYSTEM\\CurrentControlSet\\Services\\Scsicdrm";
static const char new_path[] = "System32\\drivers\\N3CDROM.SYS";
static const char native_path[] = "System32\\drivers\\Scsicdrm.sys";
typedef struct {
    DWORD magic, start, type;
    char path[MAX_PATH];
} ClassState;

static int digest_matches(const File *f, u32 size, const char *expected) {
    char digest[65];
    if (f->n != size)
        return 0;
    sha256(f->p, f->n, digest);
    return !strcmp(digest, expected);
}
static int original_path(const char *path) {
    return !lstrcmpiA(path, native_path) ||
           !lstrcmpiA(path, "\\SystemRoot\\System32\\drivers\\Scsicdrm.sys");
}
static int valid_state(const ClassState *s) {
    return s->magic == 0x3144434e && s->start <= SERVICE_DISABLED && !s->path[MAX_PATH - 1] &&
           ((!s->type && !s->path[0]) ||
            ((s->type == REG_SZ || s->type == REG_EXPAND_SZ) && original_path(s->path)));
}
static int current(ClassState *s) {
    HKEY key;
    DWORD size, type;
    LONG rc;
    memset(s, 0, sizeof(*s));
    s->magic = 0x3144434e;
    size = sizeof(s->start);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, service_key, 0, KEY_QUERY_VALUE, &key))
        return 0;
    rc = RegQueryValueExA(key, "Start", 0, &type, (BYTE *)&s->start, &size);
    if (!rc && type == REG_DWORD && size == sizeof(s->start)) {
        size = sizeof(s->path) - 1;
        rc = RegQueryValueExA(key, "ImagePath", 0, &s->type, (BYTE *)s->path, &size);
        if (rc == ERROR_FILE_NOT_FOUND) {
            s->type = 0;
            rc = 0;
        } else if (!rc && s->type != REG_SZ && s->type != REG_EXPAND_SZ)
            rc = ERROR_INVALID_DATA;
    } else
        rc = ERROR_INVALID_DATA;
    RegCloseKey(key);
    return !rc;
}
static int save(const char *dir, const char *name, const File *f) {
    char path[MAX_PATH];
    return join(path, dir, name) && (matches(path, f) || (create(path, f) && matches(path, f)));
}
static int load_state(const char *path, ClassState *out) {
    File f;
    if (!readfile(path, &f))
        return 0;
    int ok = f.n == sizeof(*out);
    if (ok) {
        memcpy(out, f.p, f.n);
        ok = valid_state(out);
    }
    LocalFree(f.p);
    return ok;
}
static int set_service(const char *image, DWORD start) {
    SC_HANDLE scm = OpenSCManagerA(0, 0, SC_MANAGER_CONNECT);
    SC_HANDLE svc = scm ? OpenServiceA(scm, "Scsicdrm", SERVICE_CHANGE_CONFIG) : 0;
    int ok = svc && ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, start, SERVICE_NO_CHANGE, image, 0,
                                         0, 0, 0, 0, 0);
    if (svc)
        CloseServiceHandle(svc);
    if (scm)
        CloseServiceHandle(scm);
    return ok;
}
static int restore_service(const ClassState *s) {
    HKEY key;
    LONG rc;
    if (!set_service(s->type ? s->path : native_path, s->start))
        return 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, service_key, 0, KEY_SET_VALUE, &key))
        return 0;
    rc = s->type ? RegSetValueExA(key, "ImagePath", 0, s->type, (const BYTE *)s->path,
                                  (DWORD)strlen(s->path) + 1)
                 : RegDeleteValueA(key, "ImagePath");
    RegCloseKey(key);
    return !rc || (!s->type && rc == ERROR_FILE_NOT_FOUND);
}
static int build_class(const File *cd, const File *cl, File *out) {
    out->n = CLASS_SIZE;
    out->p = LocalAlloc(LMEM_FIXED, out->n);
    if (!out->p)
        return 0;
    memcpy(out->p, class_template, out->n);
    memcpy(out->p + CDROM_POS, cd->p, cd->n);
    memcpy(out->p + CLASS2_POS, cl->p, cl->n);
    for (u32 i = 0; i < sizeof(class_links) / sizeof(class_links[0]); i++) {
        u32 v = class_links[i].value, at = class_links[i].at;
        for (unsigned j = 0; j < 4; j++)
            out->p[at + j] = (byte)(v >> (8 * j));
    }
    return digest_matches(out, CLASS_SIZE, CLASS_DIGEST);
}
const char *install_class(const char *cdname) {
    char statefile[MAX_PATH], libname[MAX_PATH], *base = libname;
    File cd = {0}, cl = {0}, built = {0};
    ClassState now, saved;
    const char *err = 0;
    if (strlen(cdname) >= MAX_PATH || !join(statefile, appdir, "CDSTATE.BKP"))
        return "The optical-class file path is too long.";
    lstrcpyA(libname, cdname);
    for (char *p = libname; *p; ++p)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    if (lstrcmpiA(base, "CDROM.SYS") || (size_t)(base - libname) + sizeof("CLASS2.SYS") > MAX_PATH)
        return "Select the NT4 x86 CDROM.SYS beside its matching CLASS2.SYS.";
    lstrcpyA(base, "CLASS2.SYS");
    if (!readfile(cdname, &cd) || !readfile(libname, &cl) ||
        !digest_matches(&cd, CDROM_SIZE, CDROM_DIGEST) ||
        !digest_matches(&cl, CLASS2_SIZE, CLASS2_DIGEST)) {
        err = "These files do not match the supported NT4 x86 CDROM.SYS and CLASS2.SYS pair. See "
              "TESTS.md.";
        goto done;
    }
    if (!build_class(&cd, &cl, &built)) {
        err = "Could not verify the linked optical class module.";
        goto done;
    }
    if (!current(&now)) {
        err = "Cannot read the SCSI CD-ROM class service.";
        goto done;
    }
    if (missing(statefile)) {
        File record = {(byte *)&now, sizeof(now)};
        if (!valid_state(&now) || !save(appdir, "CDSTATE.BKP", &record)) {
            err = "Cannot preserve the original optical class service.";
            goto done;
        }
    }
    if (!load_state(statefile, &saved) ||
        (memcmp(&now, &saved, sizeof(now)) && lstrcmpiA(now.path, new_path))) {
        err = "The optical class configuration has changed.";
        goto done;
    }
    if (!save(appdir, "N4CDROM.BKP", &cd) || !save(appdir, "N4CLASS2.BKP", &cl) ||
        !save(drivers, "N3CDROM.SYS", &built)) {
        err = "Cannot stage and verify the optical class files.";
        goto done;
    }
    if (!set_service(new_path, SERVICE_SYSTEM_START))
        err = "Cannot register the linked optical class module.";
done:
    if (built.p)
        LocalFree(built.p);
    if (cl.p)
        LocalFree(cl.p);
    if (cd.p)
        LocalFree(cd.p);
    return err;
}
static int discard(const char *dir, const char *name, u32 size, const char *digest, int loaded) {
    char path[MAX_PATH];
    File f;
    if (!join(path, dir, name))
        return 0;
    if (missing(path))
        return 1;
    if (!readfile(path, &f))
        return 0;
    int own = digest_matches(&f, size, digest);
    LocalFree(f.p);
    return own &&
           (DeleteFileA(path) || (loaded && MoveFileExA(path, 0, MOVEFILE_DELAY_UNTIL_REBOOT)));
}
const char *remove_class(void) {
    char statefile[MAX_PATH];
    ClassState now, old;
    if (!join(statefile, appdir, "CDSTATE.BKP"))
        return "Invalid optical backup path.";
    if (missing(statefile))
        return 0;
    if (!load_state(statefile, &old) || !current(&now))
        return "Cannot verify the original optical service configuration.";
    if (!lstrcmpiA(now.path, new_path)) {
        if (!restore_service(&old))
            return "Cannot restore the original optical service.";
    } else if (memcmp(&now, &old, sizeof(now)))
        return "The optical class service was changed by another installation.";
    if (!discard(drivers, "N3CDROM.SYS", CLASS_SIZE, CLASS_DIGEST, 1) ||
        !discard(appdir, "N4CDROM.BKP", CDROM_SIZE, CDROM_DIGEST, 0) ||
        !discard(appdir, "N4CLASS2.BKP", CLASS2_SIZE, CLASS2_DIGEST, 0) || !DeleteFileA(statefile))
        return "The original service is selected. An optical class file could not be removed.";
    return 0;
}
