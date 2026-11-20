#include "common.h"
int readfile(const char *path, File *f) {
    DWORD got, high = 0;
    BOOL ok;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    f->p = 0;
    f->n = 0;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    f->n = GetFileSize(h, &high);
    if (high || !f->n || f->n > MAX_PORT + EXTRA) {
        CloseHandle(h);
        return 0;
    }
    f->p = LocalAlloc(LMEM_FIXED, f->n);
    ok = f->p && ReadFile(h, f->p, f->n, &got, 0) && got == f->n;
    CloseHandle(h);
    if (!ok && f->p) {
        LocalFree(f->p);
        f->p = 0;
    }
    return ok;
}
int equal(const File *a, const File *b) {
    return a->n == b->n && !memcmp(a->p, b->p, a->n);
}
int matches(const char *name, const File *data) {
    File f;
    int ok;
    if (!readfile(name, &f))
        return 0;
    ok = equal(&f, data);
    LocalFree(f.p);
    return ok;
}
int join(char *out, const char *dir, const char *file) {
    size_t n = strlen(dir), k = strlen(file);
    if (!n || n + 1 + k >= MAX_PATH)
        return 0;
    memcpy(out, dir, n);
    if (dir[n - 1] != '\\')
        out[n++] = '\\';
    memcpy(out + n, file, k + 1);
    return 1;
}
int missing(const char *path) {
    return GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES &&
           GetLastError() == ERROR_FILE_NOT_FOUND;
}
int create(const char *path, const File *file) {
    DWORD count;
    int ok;
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    ok = WriteFile(h, file->p, file->n, &count, 0) && count == file->n && FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok)
        DeleteFileA(path);
    return ok;
}
static int backup_path(char *out, unsigned i) {
    char name[16];
    if (i)
        wsprintfA(name, "SCP%05u.BKP", i);
    else
        lstrcpyA(name, "SCSIPORT.BKP");
    return join(out, drivers, name);
}
static int backup(const File *file) {
    unsigned i;
    char path[MAX_PATH];
    for (i = 0; i < 1000 && backup_path(path, i); i++) {
        if (missing(path)) {
            if (create(path, file))
                return matches(path, file);
            if (GetLastError() != ERROR_FILE_EXISTS)
                return 0;
        }
        if (matches(path, file))
            return 1;
    }
    return 0;
}
static const char *replace(const char *dest, const File *old, const File *file) {
    char tmp[MAX_PATH];
    if (!join(tmp, drivers, "N3STAGE.TMP") || !create(tmp, file))
        return "Cannot create the staging file N3STAGE.TMP. Check permissions and free space.";
    if (!matches(tmp, file) || !matches(dest, old)) {
        DeleteFileA(tmp);
        return "The source changed or the staged copy failed verification.";
    }
    if (!MoveFileExA(tmp, dest, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp);
        return "Windows could not replace SCSIPORT.SYS. Check administrator access and file "
               "attributes.";
    }
    return matches(dest, file) ? 0
                               : "The port file could not be verified after replacement. Keep the "
                                 "BKP for recovery.";
}
/* Match the installed image to a bridge rebuilt from a local backup. This
   also prevents an old backup from undoing a later service pack. */
static const char *restore(const char *path, const File *cur, int remove, int *changed) {
    unsigned i;
    char name[MAX_PATH];
    int gaps = 0;
    for (i = 0; i < 1000 && gaps < 2 && backup_path(name, i); i++) {
        File orig, expected;
        const char *err;
        if (!readfile(name, &orig)) {
            ++gaps;
            continue;
        }
        gaps = 0;
        expected.p = LocalAlloc(LMEM_FIXED, orig.n + EXTRA);
        if (!expected.p) {
            LocalFree(orig.p);
            return "Insufficient memory.";
        }
        err = bridge(orig.p, orig.n, expected.p, &expected.n);
        if (!err && equal(&expected, cur)) {
            LocalFree(expected.p);
            err = remove ? replace(path, cur, &orig) : 0;
            if (remove && !err)
                *changed = 1;
            LocalFree(orig.p);
            return err;
        }
        LocalFree(expected.p);
        LocalFree(orig.p);
    }
    return "The installed bridge has no matching local backup. Restore its matching SCSIPORT.BKP "
           "offline.";
}
const char *port_action(int remove, int *changed) {
    File cur, result;
    const char *err = 0;
    char path[MAX_PATH];
    *changed = 0;
    if (!join(path, drivers, "SCSIPORT.SYS") || !readfile(path, &cur))
        return "Cannot read the installed SCSIPORT.SYS.";
    if (minor == 10) {
        result.p = LocalAlloc(LMEM_FIXED, cur.n + EXTRA);
        if (!result.p)
            err = "Insufficient memory.";
        else {
            err = bridge(cur.p, cur.n, result.p, &result.n);
            if (err && !strcmp(err, "The NT3SCPRT bridge is already installed."))
                err = restore(path, &cur, remove, changed);
            else if (!err && !remove && !equal(&result, &cur)) {
                if (!backup(&cur))
                    err = "Could not write and verify the original port backup.";
                else {
                    err = replace(path, &cur, &result);
                    if (!err)
                        *changed = 1;
                }
            }
            LocalFree(result.p);
        }
    }
    LocalFree(cur.p);
    return err;
}
const char *install_program(void) {
    File source;
    int ok;
    if (!CreateDirectoryA(appdir, 0) && GetLastError() != ERROR_ALREADY_EXISTS)
        return "Cannot create the NT3SCPRT program directory.";
    if (!readfile(self, &source))
        return "Cannot read the setup executable.";
    ok = matches(appfile, &source) || (create(appfile, &source) && matches(appfile, &source));
    if (!ok) {
        File old;
        if (readfile(appfile, &old)) {
            char digest[65], saved[MAX_PATH];
            sha256(old.p, old.n, digest);
            if (!strcmp(digest, "ea90c9ee943e7b7ff54e2949a483a25e287be2d0f8b2cfb5ddf20eb762c50ef2") &&
                join(saved, appdir, "SETUP.BKP") &&
                (matches(saved, &old) || (create(saved, &old) && matches(saved, &old))))
                ok = replace(appfile, &old, &source) == 0;
            LocalFree(old.p);
        }
    }
    LocalFree(source.p);
    return ok ? 0 : "Cannot install the program file. An existing different file is preserved.";
}
const char *remove_program(void) {
    File source;
    int owned;
    if (missing(appfile))
        return 0;
    if (!readfile(self, &source))
        return "Cannot verify the installed program.";
    owned = matches(appfile, &source);
    LocalFree(source.p);
    if (!owned)
        return "A different program file occupies the install path.";
    if (!DeleteFileA(appfile) && !MoveFileExA(appfile, 0, MOVEFILE_DELAY_UNTIL_REBOOT))
        return "Windows could not remove the program file.";
    if (!RemoveDirectoryA(appdir))
        MoveFileExA(appdir, 0, MOVEFILE_DELAY_UNTIL_REBOOT);
    return 0;
}
