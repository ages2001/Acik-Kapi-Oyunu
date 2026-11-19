#include "transaction.h"
const char *names[] = {"Unknown",
                       "Microsoft Windows 1.01",
                       "Microsoft Windows 2.03",
                       "Microsoft Windows/286 2.11",
                       "Microsoft Windows/386 2.11",
                       "Microsoft Windows 3.0a",
                       "Microsoft Windows 3.1",
                       "Windows for Workgroups 3.11",
                       "Windows 95 OSR2 (4.00.1111)",
                       "Windows 98 Second Edition (4.10.2222)",
                       "Windows Millennium Edition (4.90.3000)",
                       "Microsoft MS-DOS 5.x",
                       "Microsoft MS-DOS 6.x",
                       "Microsoft MS-DOS 7.x",
                       "Microsoft MS-DOS 8.0",
                       "Windows 95 (PATCHMEM profile)",
                       "Windows 98 (PATCHMEM profile)",
                       "Windows Me (PATCHMEM profile)"};
Target *targets;
static Target **tail;
int installed(const char *p) {
    char f[PATHLEN];
    return pathjoin(f, p, "WRMSTATE\\ACTIVE.DAT") && exists(f);
}
int pending(const char *p) {
    char f[PATHLEN];
    return pathjoin(f, p, "WRMSTATE\\TXN.DAT") && exists(f);
}
static int has(const char *dir, const char *name) {
    char f[PATHLEN];
    return pathjoin(f, dir, name) && exists(f);
}
static int contains(const char *file, const char *word) {
    char buf[1030];
    size_t keep = 0, n, i, len = strlen(word);
    FILE *f = fopen(file, "rb");
    if (!f)
        return 0;
    while ((n = fread(buf + keep, 1, 1000, f)) != 0) {
        n += keep;
        for (i = 0; i + len <= n; i++)
            if (!memcmp(buf + i, word, len)) {
                fclose(f);
                return 1;
            }
        keep = n < len - 1 ? n : len - 1;
        memmove(buf, buf + n - keep, keep);
    }
    fclose(f);
    return 0;
}
static int version9x(const char *file) {
    Byte b[1040];
    size_t n, k = 0, i;
    FILE *f = fopen(file, "rb");
    if (!f)
        return 0;
    while ((n = fread(b + k, 1, 1024, f))) {
        n += k;
        for (i = 0; i + 16 <= n; i++)
            if (!memcmp(b + i, "\xbd\x04\xef\xfe\0\0\x01\0", 8) && b[i + 10] == 4 &&
                b[i + 11] == 0) {
                unsigned minor = b[i + 8];
                fclose(f);
                return minor == 90 ? GENME : minor == 10 ? GEN98 : minor == 0 ? GEN95 : 0;
            }
        k = n < 15 ? n : 15;
        memmove(b, b + n - k, k);
    }
    fclose(f);
    return 0;
}
int detect(const char *path) {
    char f[PATHLEN], hex[65];
    Byte digest[32];
    struct find_t file;
    Signature *s, *j;
    int id;
    {
        Header h;
        FILE *state;
        const char *rec[] = {"WRMSTATE\\TXN.DAT", "WRMSTATE\\ACTIVE.DAT"};
        int k;
        if (has(path, "WRMSTATE"))
            for (k = 0; k < 2; k++) {
                if (!pathjoin(f, path, rec[k]))
                    continue;
                state = fopen(f, "rb");
                if (!state)
                    continue;
                if (fread(&h, sizeof(h), 1, state) == 1 && !memcmp(h.magic, "WINRAM1", 8) &&
                    h.profile > 0 && h.profile <= GENME) {
                    fclose(state);
                    return h.profile;
                }
                fclose(state);
            }
    }
    if (!has(path, "WIN.COM") && !has(path, "WIN386.EXE") && !has(path, "IO.SYS"))
        return NONE;
    for (s = sigs; s->id; s++) {
        if (s->id >= D5 && strlen(path) != 3 && !has(path, "COMMAND.COM"))
            continue;
        if (!pathjoin(f, path, s->path) ||
            _dos_findfirst(f, _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_ARCH, &file))
            continue;
        if ((long)file.size != s->size)
            continue;
        if (!hashfile(f, digest))
            continue;
        hexhash(digest, hex);
        for (j = sigs; j->id; j++)
            if (!strcmp(j->path, s->path) && j->size == (long)file.size && !strcmp(j->sha, hex)) {
                if (j->id >= D5 && (!has(path, "MSDOS.SYS") || !has(path, "COMMAND.COM")))
                    return NONE;
                return j->id;
            }
    }
    if (has(path, "WIN.COM") && has(path, "SYSTEM\\VMM32.VXD") &&
        pathjoin(f, path, "SYSTEM\\KERNEL32.DLL")) {
        id = version9x(f);
        if (id)
            return id;
    }
    if (has(path, "IO.SYS") && has(path, "MSDOS.SYS") && pathjoin(f, path, "COMMAND.COM")) {
        if (contains(f, "MS-DOS(R) Version 5."))
            return D5;
        if (contains(f, "MS-DOS(R) Version 6."))
            return D6;
        if (contains(f, "Windows Millennium [Version"))
            return D8;
        if (contains(f, "Windows 98 [Version") || contains(f, "Windows 95."))
            return D7;
    }
    return NONE;
}
int select_path(const char *s, char *out) {
    int id;
    char *last;
    if (!canonical(out, s) || !isdir(out))
        return 0;
    id = detect(out);
    if (id)
        return id;
    last = strrchr(out, '\\');
    if (last && !stricmp(last + 1, "SYSTEM")) {
        if (last != out + 2)
            *last = 0;
        return detect(out);
    }
    if (last == out + 2 && !stricmp(last + 1, "DOS")) {
        out[3] = 0;
        id = detect(out);
        if (id >= D5 && id <= D8)
            return id;
    }
    return 0;
}
static void visit(char *path) {
    struct find_t ent;
    char spec[PATHLEN];
    unsigned rc;
    size_t len = strlen(path);
    int id = detect(path);
    Target *t;
    if (id) {
        t = calloc(1, sizeof(*t));
        if (!t) {
            fail("Insufficient memory to list every installation", path);
            return;
        }
        t->id = id;
        strcpy(t->path, path);
        *tail = t;
        tail = &t->next;
    }
    if (!pathjoin(spec, path, "*.*"))
        return;
    for (rc = _dos_findfirst(spec, _A_SUBDIR | _A_RDONLY | _A_HIDDEN | _A_SYSTEM, &ent); !rc;
         rc = _dos_findnext(&ent)) {
        if (!(ent.attrib & _A_SUBDIR) || ent.name[0] == '.' || !stricmp(ent.name, "WRMSTATE") ||
            !stricmp(ent.name, "WRMBOOT") || !stricmp(ent.name, "WRMTOOLS"))
            continue;
        if (len + strlen(ent.name) + 2 < PATHLEN) {
            pathjoin(path, path, ent.name);
            visit(path);
            path[len] = 0;
        }
    }
}
void free_targets(void) {
    Target *p;
    while (targets) {
        p = targets;
        targets = p->next;
        free(p);
    }
}
static int floppy_present(unsigned d) {
    unsigned equipment;
    _asm {int 11h
  mov equipment,ax}
    return (equipment & 1) && d <= 1 + ((equipment >> 6) & 3);
}
static int fixed_drive(unsigned d) {
    unsigned value = 0xffff;
    _asm {mov bx,d
  mov ax,4408h
  int 21h
  jc done_ioctl
  mov value,ax
 done_ioctl:}
    return value == 1;
}
void scan(void) {
    unsigned drive, bootdrive;
    char dir[PATHLEN] = "A:\\";
    free_targets();
    tail = &targets;
    _dos_getdrive(&bootdrive);
    for (drive = 1; drive <= 26; drive++) { /* DOS maps a lone floppy to both A: and B:. Avoid the
                                               phantom exchange prompt. */
        if (drive <= 2 && drive != bootdrive && !fixed_drive(drive)) {
            /* Under Windows an empty floppy can open a modal critical-error dialog. */
            if (windows_major || !floppy_present(drive))
                continue;
        }
        dir[0] = 'A' + drive - 1;
        if (isdir(dir))
            visit(dir);
    }
}
int parse_args(int argc, char **argv, Options *o) {
    int i;
    char *s;
    memset(o, 0, sizeof(*o));
    for (i = 1; i < argc; i++) {
        s = argv[i];
        if (*s == '/' || *s == '-') {
            s++;
            if (!stricmp(s, "LIST"))
                o->list = 1;
            else if (!stricmp(s, "UNINSTALL") || !stricmp(s, "U"))
                o->remove = 1;
            else if (!stricmp(s, "INSTALL") || !stricmp(s, "I"))
                o->only = 1;
            else if (!stricmp(s, "YES"))
                o->yes = 1;
            else if (!stricmp(s, "LIVE"))
                o->live = 1;
            else if (!stricmp(s, "VERIFY"))
                o->verify = 1;
            else if (!stricmp(s, "M"))
                o->move = 1;
            else if (!stricmp(s, "A"))
                o->agp = 1;
            else if (!stricmp(s, "P"))
                o->prothook = 1;
            else if (!stricmp(s, "SPLIT8MB"))
                o->split = 1;
            else if (!strnicmp(s, "C:", 2)) {
                char *end;
                long n = strtol(s + 2, &end, 10);
                if (*end || n < 1 || n > 4095)
                    return 0;
                o->cache = (int)n;
            } else if (!stricmp(s, "PATH") && i + 1 < argc && !o->path[0]) {
                s = argv[++i];
                if (strlen(s) >= PATHLEN)
                    return 0;
                strcpy(o->path, s);
            } else
                return 0;
        } else if (!o->path[0] && strlen(s) < PATHLEN)
            strcpy(o->path, s);
        else
            return 0;
    }
    return !(o->only && o->remove);
}

const char *display_name(int id, const char *base) {
    static char exact[44];
    char p[PATHLEN], buf[1040];
    const char tag[] = "MS-DOS(R) Version ";
    FILE *f;
    unsigned i, n, overlap = 0;
    if (id < D5 || id > D8)
        return names[id];
    if (id == D8)
        return "Microsoft MS-DOS 8.00";
    if (!pathjoin(p, base, "COMMAND.COM"))
        return names[id];
    if (id == D7) {
        if (contains(p, "Windows 98") || contains(p, "4.00.1111") || contains(p, "4.00.950 B"))
            return "Microsoft MS-DOS 7.10";
        if (contains(p, "4.00.950"))
            return "Microsoft MS-DOS 7.00";
        return names[id];
    }
    f = fopen(p, "rb");
    if (!f)
        return names[id];
    while ((n = fread(buf + overlap, 1, 1000, f))) {
        n += overlap;
        for (i = 0; i + sizeof(tag) + 3 < n; i++)
            if (!memcmp(buf + i, tag, sizeof(tag) - 1)) {
                char *v = buf + i + sizeof(tag) - 1;
                if ((v[0] == '5' || v[0] == '6') && v[1] == '.' && isdigit(v[2]) && isdigit(v[3])) {
                    sprintf(exact, "Microsoft MS-DOS %.4s", v);
                    fclose(f);
                    return exact;
                }
            }
        overlap = n < 24 ? n : 24;
        memmove(buf, buf + n - overlap, overlap);
    }
    fclose(f);
    return names[id];
}
