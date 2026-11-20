#include "common.h"
#include <errno.h>
char self[PATHLEN], error_msg[264];
long packbase;
static char buffer[4096];
int fail(const char *text, const char *path) {
    sprintf(error_msg, "%.115s%s%.125s", text, *path ? " - " : "", path);
    return 0;
}
int pathjoin(char *out, const char *dir, const char *file) {
    size_t n = strlen(dir);
    if (n + strlen(file) + 2 > PATHLEN)
        return fail("Path exceeds the DOS limit", dir);
    if (out != dir)
        strcpy(out, dir);
    if (n && out[n - 1] != '\\')
        strcat(out, "\\");
    strcat(out, file);
    return 1;
}
int canonical(char *out, const char *src) {
    size_t n;
    if (!*src || strlen(src) >= PATHLEN || !_fullpath(out, src, PATHLEN))
        return 0;
    strupr(out);
    n = strlen(out);
    while (n > 3 && out[n - 1] == '\\')
        out[--n] = 0;
    return n >= 3;
}
int exists(const char *p) {
    unsigned a;
    return _dos_getfileattr(p, &a) == 0;
}
int isdir(const char *p) {
    unsigned a;
    if (strlen(p) == 3 && p[1] == ':' && p[2] == '\\') {
        struct _diskfree_t d;
        return !_dos_getdiskfree(toupper(p[0]) - 'A' + 1, &d);
    }
    return !_dos_getfileattr(p, &a) && (a & _A_SUBDIR);
}
int rmfile(const char *p) {
    if (!exists(p))
        return 1;
    _dos_setfileattr(p, 0);
    return !remove(p);
}
static int stream(FILE *in, FILE *out, long count) {
    unsigned got, want;
    while (count) {
        want = count > sizeof(buffer) ? sizeof(buffer) : (unsigned)count;
        got = fread(buffer, 1, want, in);
        if (got != want || fwrite(buffer, 1, got, out) != got)
            return 0;
        count -= got;
    }
    return 1;
}
int copyfile(const char *src, const char *dst) {
    FILE *s, *d;
    long n;
    int ok;
    unsigned date, time;
    if (!stricmp(src, dst))
        return 1;
    s = fopen(src, "rb");
    if (!s)
        return fail("Cannot read", src);
    n = filelength(fileno(s));
    d = fopen(dst, "wb");
    if (!d) {
        fclose(s);
        return fail("Cannot create", dst);
    }
    ok = n >= 0 && stream(s, d, n) && !fflush(d);
    if (!_dos_getftime(fileno(s), &date, &time))
        _dos_setftime(fileno(d), date, time);
    if (fclose(d))
        ok = 0;
    fclose(s);
    return ok ? 1 : fail("Copy failed", dst);
}
int samefile(const char *a, const char *b) {
    Byte x[32], y[32];
    return hashfile(a, x) && hashfile(b, y) && !memcmp(x, y, 32);
}
int startpack(const char *exe) {
    FILE *f;
    char mark[8];
    U32 bytes;
    long end;
    if (!canonical(self, exe))
        return fail("Cannot resolve the running program", exe);
    f = fopen(self, "rb");
    if (!f)
        return fail("Cannot open the running program", self);
    end = filelength(fileno(f));
    if (end < 12 || fseek(f, -12L, SEEK_END) || fread(mark, 1, 8, f) != 8 ||
        memcmp(mark, "WINRAM1!", 8) || fread(&bytes, 4, 1, f) != 1 || bytes > (U32)(end - 12)) {
        fclose(f);
        return fail("Invalid WINRAM package footer", self);
    }
    packbase = end - 12 - bytes;
    fclose(f);
    return 1;
}
int extract(const char *name, const char *dst) {
    Asset *a;
    FILE *s, *d;
    int ok;
    for (a = assets; a->name; a++)
        if (!stricmp(name, a->name))
            break;
    if (!a->name)
        return fail("Missing package asset", name);
    s = fopen(self, "rb");
    if (!s)
        return fail("Cannot open package", self);
    d = fopen(dst, "wb");
    if (!d) {
        fclose(s);
        return fail("Cannot stage file", dst);
    }
    ok = !fseek(s, packbase + a->offset, SEEK_SET) && stream(s, d, a->size);
    if (fclose(d))
        ok = 0;
    fclose(s);
    return ok && matches(dst, a->sha) ? 1 : fail("Embedded file integrity check failed", name);
}
