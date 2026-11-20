#include "transaction.h"
#include "nepatch.h"
#include <process.h>
#include <errno.h>
#include <sys/locking.h>
#include <share.h>
static Tx tx;
static char staged[PATHLEN], scratch[PATHLEN], shared_root[4], helperdir[PATHLEN];
static int directory(const char *p) {
    return isdir(p) || !mkdir(p) || fail("Cannot create directory", p);
}
static int digest_is(const char *path, const Byte *want) {
    Byte have[32];
    return hashfile(path, have) && !memcmp(have, want, 32);
}
static int open_tx(const char *base, const char *sub, int id) {
    memset(&tx, 0, sizeof(tx));
    strcpy(tx.base, base);
    if (!pathjoin(tx.dir, base, sub) || !directory(tx.dir))
        return 0;
    memcpy(tx.h.magic, "WINRAM1", 8);
    tx.h.profile = id;
    tx.h.direction = 'I';
    return 1;
}
static int write_record(const char *n) {
    char dst[PATHLEN], tmp[PATHLEN];
    FILE *f;
    int ok;
    if (!pathjoin(dst, tx.dir, n) || !pathjoin(tmp, tx.dir, "INDEX.NEW"))
        return 0;
    f = fopen(tmp, "wb");
    if (!f)
        return fail("Cannot create installation journal", tmp);
    ok = fwrite(&tx.h, sizeof(tx.h), 1, f) == 1 &&
         fwrite(tx.e, sizeof(Entry), tx.h.count, f) == tx.h.count;
    if (fclose(f))
        ok = 0;
    if (!ok || exists(dst) || rename(tmp, dst))
        return fail("Cannot publish installation journal", dst);
    return 1;
}
static int load(const char *n) {
    char p[PATHLEN];
    FILE *f;
    int ok;
    unsigned ix;
    if (!pathjoin(p, tx.dir, n))
        return 0;
    f = fopen(p, "rb");
    if (!f)
        return fail("Cannot open installation record", p);
    ok = fread(&tx.h, sizeof(tx.h), 1, f) == 1 && !memcmp(tx.h.magic, "WINRAM1", 8) &&
         tx.h.count <= MAXFILES && (tx.h.direction == 'I' || tx.h.direction == 'U') &&
         fread(tx.e, sizeof(Entry), tx.h.count, f) == tx.h.count && fgetc(f) == EOF;
    fclose(f);
    for (ix = 0; ok && ix < tx.h.count; ix++) {
        Entry *e = &tx.e[ix];
        if (!memchr(e->dst, 0, PATHLEN) || e->dst[1] != ':' || e->dst[2] != 92 ||
            !memchr(e->bkp, 0, 13) || !memchr(e->tmp, 0, 13) || strchr(e->bkp, 92) ||
            strchr(e->tmp, 92))
            ok = 0;
        else
            e->dst[0] = tx.base[0];
    }
    return ok ? 1 : fail("Invalid installation record", p);
}
static int backup(const char *file, char *name, const Byte *sha) {
    unsigned n;
    char p[PATHLEN];
    for (n = 1; n < 65535; n++) {
        sprintf(name, "B%04X.BKP", n);
        if (!pathjoin(p, tx.dir, name))
            return 0;
        if (exists(p)) {
            if (digest_is(p, sha))
                return 1;
            continue;
        }
        if (!copyfile(file, p) || !digest_is(p, sha))
            return fail("Backup verification failed", file);
        return 1;
    }
    return fail("Backup directory is full", tx.dir);
}
static int begin(const char *file) {
    Entry *e;
    int f;
    if (tx.h.count >= MAXFILES)
        return fail("Too many installation files", file);
    e = &tx.e[tx.h.count];
    memset(e, 0, sizeof(*e));
    strcpy(e->dst, file);
    sprintf(e->tmp, "S%04X.NEW", tx.h.count);
    if (!pathjoin(staged, tx.dir, e->tmp))
        return 0;
    e->exists = exists(file);
    if (!e->exists)
        return 1;
    if (!hashfile(file, e->before) || _dos_getfileattr(file, &e->attr))
        return fail("Cannot read original file", file);
    f = open(file, O_RDONLY | O_BINARY);
    if (f < 0)
        return fail("Cannot open original file", file);
    _dos_getftime(f, &e->date, &e->time);
    close(f);
    return 1;
}
static int finish(void) {
    Entry *e = &tx.e[tx.h.count];
    if (!hashfile(staged, e->after))
        return fail("Cannot verify staged file", staged);
    if (e->exists && !memcmp(e->before, e->after, 32)) {
        rmfile(staged);
        return 1;
    }
    if (e->exists && !backup(e->dst, e->bkp, e->before))
        return 0;
    tx.h.count++;
    return 1;
}
static int asset_at(const char *name, const char *dst) {
    return begin(dst) && extract(name, staged) && finish();
}
static int localasset(const char *base, const char *name, const char *file) {
    char p[PATHLEN];
    return pathjoin(p, base, file) && asset_at(name, p);
}
static int clone_at(const char *source, const char *dst) {
    return begin(dst) && copyfile(source, staged) && finish();
}
static int copy_or_delete(Entry *e, int undo) {
    char src[PATHLEN];
    const Byte *want;
    int f;
    if (undo && !e->exists)
        return rmfile(e->dst) || fail("Cannot remove installed file", e->dst);
    if (!pathjoin(src, tx.dir, undo ? e->bkp : e->tmp))
        return 0;
    want = undo ? e->before : e->after;
    if (digest_is(e->dst, want))
        return 1;
    if (!digest_is(src, want))
        return fail("Recovery source failed its checksum", src);

    /* The verified stage and backup survive even if power fails during this copy. */
    if (exists(e->dst) && _dos_setfileattr(e->dst, 0))
        return fail("Cannot unlock file", e->dst);
    if (!copyfile(src, e->dst) || !digest_is(e->dst, want))
        return fail("Cannot write target file. Restart in DOS to resume", e->dst);
    if (e->exists) {
        if (undo) {
            f = open(e->dst, O_RDWR | O_BINARY);
            if (f >= 0) {
                _dos_setftime(f, e->date, e->time);
                close(f);
            }
        }
        _dos_setfileattr(e->dst, e->attr);
    }
    return 1;
}
static int live_mode;
static int defer_files(void) {
    char init[PATHLEN], work[PATHLEN], src[PATHLEN], dest[PATHLEN], old[PATHLEN], name[13],
        line[512];
    int undo = tx.h.direction == 'U', section = 0, found = 0, ok = 1;
    unsigned i;
    Entry *e;
    FILE *out, *in;
    Byte d[32];
    if (!pathjoin(init, tx.base, "WININIT.INI") || !pathjoin(work, tx.dir, "WININIT.NEW"))
        return 0;
    if (exists(init)) {
        if (!hashfile(init, d) || !backup(init, name, d))
            return 0;
    }
    out = fopen(work, "wt");
    if (!out)
        return fail("Cannot stage Windows restart", work);
    in = fopen(init, "rt");
    if (in) {
        while (fgets(line, sizeof(line), in)) {
            char *p = line;
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '[')
                section = !strnicmp(p, "[Rename]", 8);
            if (section && !strnicmp(p, "[Rename]", 8)) {
                found = 1;
                continue;
            }
            if (!section)
                fputs(line, out);
        }
        if (ferror(in))
            ok = 0;
        rewind(in);
    }
    fputs("\n[Rename]\n", out);
    if (in && found) {
        section = 0;
        while (fgets(line, sizeof(line), in)) {
            char *p = line;
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '[') {
                section = !strnicmp(p, "[Rename]", 8);
                continue;
            }
            if (section)
                fputs(line, out);
        }
        if (ferror(in))
            ok = 0;
    }
    if (in)
        fclose(in);
    for (i = 0; i < tx.h.count && ok; i++) {
        e = &tx.e[i];
        if (undo && !e->exists) {
            fprintf(out, "NUL=%s\n", e->dst);
            continue;
        }
        if (undo) {
            sprintf(name, "U%04X.NEW", i);
            pathjoin(src, tx.dir, name);
            pathjoin(old, tx.dir, e->bkp);
            ok = copyfile(old, src) && digest_is(src, e->before);
        } else {
            pathjoin(src, tx.dir, e->tmp);
            ok = digest_is(src, e->after);
        }
        if (ok)
            fprintf(out, "%s=%s\n", e->dst, src);
    }
    pathjoin(dest, tx.dir, "ACTIVE.DAT");
    pathjoin(src, tx.dir, "TXN.DAT");
    if (undo)
        fprintf(out, "NUL=%s\nNUL=%s\n", dest, src);
    else
        fprintf(out, "%s=%s\n", dest, src);
    if (ferror(out) || fclose(out))
        ok = 0;
    if (!ok)
        return fail("Windows restart data failed validation", work);
    if (!copyfile(work, init) || !samefile(work, init))
        return fail("Cannot queue Windows restart", init);
    return 1;
}

static int publish(void) {
    char j[PATHLEN], a[PATHLEN], p[PATHLEN];
    unsigned i;
    int undo = tx.h.direction == 'U';
    pathjoin(j, tx.dir, "TXN.DAT");
    pathjoin(a, tx.dir, "ACTIVE.DAT");
    if (live_mode)
        return defer_files();
    for (i = 0; i < tx.h.count; i++)
        if (!copy_or_delete(&tx.e[i], undo))
            return 0;
    if (undo) {
        if (!rmfile(a) || !rmfile(j))
            return fail("Cannot finish uninstall record", tx.dir);
    } else {
        if (exists(a) || rename(j, a))
            return fail("Cannot finish installation record", tx.dir);
    }
    for (i = 0; i < tx.h.count; i++) {
        pathjoin(p, tx.dir, tx.e[i].tmp);
        rmfile(p);
    }
    return 1;
}
static int journal(void) {
    unsigned i;
    Entry *e;
    for (i = 0; i < tx.h.count; i++) {
        e = &tx.e[i];
        if (e->exists ? !digest_is(e->dst, e->before) : exists(e->dst))
            return fail("A file changed during staging", e->dst);
    }
    return write_record("TXN.DAT") && publish();
}
static int remove_tx(void) {
    unsigned i;
    Byte changed[32];
    char bkp[13], src[PATHLEN];
    Entry *e;
    if (!load("ACTIVE.DAT"))
        return 0;
    for (i = 0; i < tx.h.count; i++) {
        e = &tx.e[i];
        if (e->exists && (!pathjoin(src, tx.dir, e->bkp) || !digest_is(src, e->before)))
            return fail("Original backup failed verification", e->dst);
        if (exists(e->dst) && !digest_is(e->dst, e->after)) {
            if (!hashfile(e->dst, changed) || !backup(e->dst, bkp, changed))
                return 0;
            printf("Saved later edits to %s as %s\\%s\n", e->dst, tx.dir, bkp);
        }
    }
    tx.h.direction = 'U';
    return write_record("TXN.DAT") && publish();
}
enum DynamicComponent { USER3, USER9, MEMORY9, PANEL9 };
static int patch_component(const char *relative, enum DynamicComponent kind, int optional) {
    char path[PATHLEN];
    FILE *out;
    const char *reason;
    int io_error;
    if (!pathjoin(path,tx.base,relative)) return 0;
    if (optional && !exists(path)) {
        printf("Component absent: %s\n",path);
        return 1;
    }
    if (!begin(path) || !copyfile(path, staged)) return 0;
    out = fopen(staged, "r+b");
    if (!out) return fail("Cannot open staged component", path);
    if (kind == MEMORY9) reason = patch_pe_memory(out);
    else if (kind == PANEL9) reason = patch_ne_panel(out);
    else reason = patch_ne_user(out,kind == USER9);
    io_error = ferror(out);
    if (fclose(out) || io_error) return fail("Cannot finish staged component",path);
    if (reason) {
        if (!rmfile(staged)) return fail("Cannot discard staged component",path);
        if (!optional) return fail(reason,path);
        printf("Skipped %s: %s. Continuing with PATCHMEM.\n",path,reason);
        return 1;
    }
    return finish();
}
static int patch_legacy_objects(const Patch *first, const char *file) {
    const Patch *q;
    FILE *in, *out;
    int ok, io_error;
    if (!begin(file)) return 0;
    for (q=first; q->id; ++q) {
        if (q->id!=first->id || strcmp(q->path,first->path)) continue;
        if (!copyfile(file,staged)) return 0;
        in=fopen(self,"rb"); out=fopen(staged,"r+b");
        if (!in || !out) {
            if (in) fclose(in);
            if (out) fclose(out);
            return fail("Cannot open executable object recipe",file);
        }
        ok=!fseek(in,packbase+q->objects,SEEK_SET) && patch_objects(out,in);
        io_error=ferror(in) || ferror(out);
        fclose(in);
        if (fclose(out) || io_error) return fail("Cannot read or write executable object",file);
        if (ok) return finish();
    }
    rmfile(staged);
    return fail("Incompatible code or relocation data in executable object",file);
}
static int patch_binaries(int id) {
    Patch *p, *q;
    char f[PATHLEN];
    int standard_only=0;
    if (id==W31) {
        int standard,enhanced,loader;
        if (!pathjoin(f,tx.base,"SYSTEM\\KRNL286.EXE")) return 0;
        standard=exists(f);
        if (!pathjoin(f,tx.base,"SYSTEM\\KRNL386.EXE")) return 0;
        enhanced=exists(f);
        if (!pathjoin(f,tx.base,"SYSTEM\\WIN386.EXE")) return 0;
        loader=exists(f);
        standard_only=standard && !enhanced && !loader;
    }
    if (id >= W30 && id <= WFW &&
        !patch_component("SYSTEM\\USER.EXE",USER3,0)) return 0;
    if ((id >= W95 && id <= WME) || (id >= GEN95 && id <= GENME)) {
        int optional = id >= GEN95;
        return patch_component("SYSTEM\\USER.EXE",USER9,optional) &&
               patch_component("SYSTEM\\KERNEL32.DLL",MEMORY9,optional) &&
               patch_component("SYSTEM\\SYSDM.CPL",PANEL9,optional);
    }
    for (p = patches; p->id; p++) {
        if (p->id != id)
            continue;
        for (q = patches; q != p; q++)
            if (q->id == id && !strcmp(q->path, p->path))
                break;
        if (q != p)
            continue;
        if (!pathjoin(f, tx.base, p->path))
            return 0;
        /* Setup can omit kernels for boot modes the user did not install. */
        if (!exists(f) && ((id == W30 && (!strcmp(p->path, "SYSTEM\\KERNEL.EXE") ||
                                            !strcmp(p->path, "SYSTEM\\KRNL286.EXE") ||
                                            !strcmp(p->path, "SYSTEM\\KRNL386.EXE"))) ||
                           (id == W31 && !strcmp(p->path, "SYSTEM\\KRNL286.EXE"))))
            continue;
        if (standard_only && (!strcmp(p->path,"SYSTEM\\KRNL386.EXE") ||
                              !strcmp(p->path,"SYSTEM\\WIN386.EXE"))) continue;
        if (!exists(f))
            return fail("Required system file is missing", f);
        if (!patch_legacy_objects(p,f)) return 0;
    }
    return 1;
}
static int setting(const char *src, const char *sect, const char *key, const char *value,
                   int mode) {
    if (!pathjoin(scratch, tx.dir, "CONFIG.WRK") || !ini(src, scratch, sect, key, value, mode))
        return 0;
    if (!rmfile(staged) || rename(scratch, staged))
        return fail("Cannot stage configuration", src);
    return 1;
}
static int prepare_early(int id) {
    char launch[PATHLEN], alias[PATHLEN], cfg[PATHLEN], src[PATHLEN], local[PATHLEN];
    const char *c, *asset;
    int i;
    if (!pathjoin(helperdir, tx.base, "WRMTOOLS") || !directory(helperdir))
        return 0;
    if (!localasset(helperdir, "RAMCAP.COM", "RAMCAP.COM"))
        return 0;
    if (!localasset(tx.base, id <= W386 ? "ABOUT12.EXE" : "ABOUTX.EXE", "ABOUTX.EXE"))
        return 0;
    if (!localasset(tx.base, id == W101 ? "PAGER1.DLL" : id <= W386 ? "PAGER12.DLL" : "PAGER3.DLL", "WRMPAGE.DLL"))
        return 0;
    if (!pathjoin(cfg, tx.base, "WIN.INI") || !begin(cfg) ||
        !setting(cfg, "windows", "load", id == W386 ? "SVSESS.EXE ABOUTX.EXE" : "ABOUTX.EXE", 1) ||
        !finish())
        return 0;
    if (id <= W386) {
        if (!localasset(helperdir, "WINSTART.COM", "WINSTART.COM"))
            return 0;
        if (!pathjoin(local, helperdir, "SETVER.EXE") || !begin(local))
            return 0;
        {
            const char *choice[] = {"DOS\\SETVER.EXE", "WINDOWS\\SETVER.EXE",
                                    "WINDOWS\\COMMAND\\SETVER.EXE", "SETVER.EXE"};
            int found = 0;
            if (pathjoin(src, tx.base, "SETVER.EXE") && exists(src)) {
                if (!copyfile(src, staged))
                    return 0;
                found = 1;
            }
            for (i = 0; !found && i < COUNT(choice); i++) {
                if (pathjoin(src, shared_root, choice[i]) && exists(src)) {
                    if (!copyfile(src, staged))
                        return 0;
                    found = 1;
                }
            }
            if (!found && !extract("SETVER.EXE", staged))
                return 0;
        }
        if (!set_profile(staged, id == W101 ? 30 : 40) || !finish())
            return 0;
        if (id == W386 && (!localasset(helperdir, "SV386.COM", "SV386.COM") ||
                           !localasset(tx.base, "SVSESS.EXE", "SVSESS.EXE")))
            return 0;
    }
    if (id == W30 || id == W31 || id == W386) {
        c = id == W30 ? "BANKWIN.COM" : id == W386 ? "BANK2.COM" : "BANKX.COM";
        if (!localasset(helperdir, c, "BANKX.COM"))
            return 0;
    }
    if (id >= W30) {
        if (id == W30 && !localasset(tx.base, "BANKWIN.386", "SYSTEM\\BANKWIN.386"))
            return 0;
        if (!pathjoin(cfg, tx.base, "SYSTEM.INI") || !begin(cfg) ||
            !setting(cfg, "386Enh", "MaxPhysPage", NULL, 0))
            return 0;
        if (id == W30) {
            if (!setting(staged, "386Enh", "device", "BANKWIN.386", 2))
                return 0;
        } else if (!setting(staged, "386Enh", "PageOverCommit", "1", 0))
            return 0;
        if (!finish())
            return 0;
    }
    if (id == WFW)
        return 1;
    c = id == W386 ? "WIN386.EXE" : "WIN.COM";
    asset = id == W386  ? "START386.BIN"
            : id == W30 ? "START30.BIN"
            : id == W31 ? "START31.BIN"
                        : "START12.BIN";
    if (!pathjoin(launch, tx.base, c) ||
        !pathjoin(alias, tx.base, id == W386 ? "WRMCORE.EXE" : "WRMCORE.COM"))
        return 0;
    if (exists(alias))
        return fail("An unrelated Windows launcher backup already exists", alias);
    return clone_at(launch, alias) && asset_at(asset, launch);
}
static int reference(int id, const Options *o) {
    char wd[PATHLEN], old[PATHLEN], dir[PATHLEN], src[3][PATHLEN], local[PATHLEN], cache[14];
    char *arg[12];
    int n = 0, i, rc;
    const char *files[] = {"VMM32.VXD", "VMM.VXD", "VCACHE.VXD"};
    const char *paths[] = {"SYSTEM\\VMM32.VXD", "SYSTEM\\VMM32\\VMM.VXD",
                           "SYSTEM\\VMM32\\VCACHE.VXD"};
    if (!pathjoin(wd, tx.dir, "PMWORK") || !directory(wd) ||
        !pathjoin(dir, tx.base, "SYSTEM\\VMM32") || !directory(dir))
        return 0;
    for (i = 0; i < 3; i++) {
        char bak[13];
        strcpy(bak, files[i]);
        strcpy(strrchr(bak, '.'), ".BAK");
        pathjoin(local, wd, bak);
        rmfile(local);
        if (!pathjoin(src[i], tx.base, paths[i]) || !pathjoin(local, wd, files[i]))
            return 0;
        rmfile(local);
        if (exists(src[i]) && !copyfile(src[i], local))
            return 0;
        {
            char original[PATHLEN];
            strcpy(original, src[i]);
            strcpy(strrchr(original, '.'), ".BAK");
            if (exists(original)) {
                pathjoin(local, wd, bak);
                if (!copyfile(original, local))
                    return 0;
            }
        }
    }
    if (!pathjoin(local, wd, "ENGINE.OVL") || !extract("PATCHMEM.EXE", local))
        return 0;
    arg[n++] = "PATCHMEM";
    arg[n++] = "/I";
    if (o->agp)
        arg[n++] = "/A";
    if (o->move || id == W95 || id == GEN95)
        arg[n++] = "/M";
    if (o->prothook)
        arg[n++] = "/P";
    sprintf(cache, "/C:%d", o->cache ? o->cache : 512);
    arg[n++] = cache;
    for (i = 0; i < 3; i++)
        arg[n++] = (char *)files[i];
    arg[n] = NULL;
    if (!getcwd(old, sizeof(old)) || _chdrive(toupper(wd[0]) - 'A' + 1) || chdir(wd))
        return fail("Cannot enter reference workspace", wd);
    fflush(stdout);
    rc = spawnv(P_WAIT, "ENGINE.OVL", (const char *const *)arg);
    _chdrive(toupper(old[0]) - 'A' + 1);
    chdir(old);
    if (rc == 5)
        puts("PATCHMEM is already installed. Preserving its configuration.");
    if (rc != 0 && rc != 5)
        return fail("PATCHMEM did not accept the selected VMM files. The target is intact", wd);
    for (i = 0; i < 3; i++) {
        if (!pathjoin(local, wd, files[i]) || !clone_at(local, src[i]))
            return 0;
    }
    for (i = 0; i < 3; i++) {
        pathjoin(local, wd, files[i]);
        rmfile(local);
        strcpy(strrchr(local, '.'), ".BAK");
        rmfile(local);
    }
    pathjoin(local, wd, "ENGINE.OVL");
    rmfile(local);
    rmdir(wd);
    return 1;
}
static int prepare9x(int id, const Options *o) {
    char cfg[PATHLEN];
    if (!reference(id, o))
        return 0;
    if (!pathjoin(cfg, tx.base, "SYSTEM.INI") || !begin(cfg) ||
        !setting(cfg, "386Enh", "MaxPhysPage", NULL, 0))
        return 0;
    if (id == WME || id == GENME) {
        char limit[12];
        sprintf(limit, "%lu", (unsigned long)(o->cache ? o->cache : 64) * 1024);
        if (!setting(staged, "vcache", "MaxFileCache", limit, 0))
            return 0;
    }
    if (!finish() || !pathjoin(cfg, tx.base, "WIN.INI") || !begin(cfg) ||
        !setting(cfg,"windows","GDIMaxHeapSize","32767",0)) return 0;
    return finish();
}
static int preparedos(void) {
    char dir[PATHLEN], loc[PATHLEN];
    const char *cmds[] = {"MEM.COM", "MEMX.COM", "XMSCHECK.COM", "FILEMAX.COM"};
    unsigned i;
    if (!pathjoin(dir, tx.base, "DOS") || !isdir(dir))
        strcpy(dir, tx.base);
    else if (!pathjoin(loc, dir, "MEM.EXE") || !exists(loc))
        strcpy(dir, tx.base);
    for (i = 0; i < COUNT(cmds); i++)
        if (!localasset(dir, cmds[i], cmds[i]))
            return 0;
    if (strcmp(dir, tx.base) && !localasset(tx.base, "MEM.COM", "MEM.COM"))
        return 0;
    if (pathjoin(loc, tx.base, "WINDOWS\\COMMAND\\MEM.EXE") && exists(loc) &&
        !localasset(tx.base, "MEM.COM", "WINDOWS\\COMMAND\\MEM.COM"))
        return 0;
    return 1;
}
static int refname(const char *base, const char *area, char *out) {
    char dir[PATHLEN], name[13], pattern[PATHLEN], text[PATHLEN];
    unsigned i, rc;
    FILE *f;
    struct find_t ent;
    if (!pathjoin(dir, shared_root, area) || !pathjoin(pattern, dir, "*.REF"))
        return 0;
    for (rc = _dos_findfirst(pattern, _A_ARCH | _A_RDONLY, &ent); !rc; rc = _dos_findnext(&ent)) {
        if (!pathjoin(out, dir, ent.name))
            return 0;
        f = fopen(out, "rt");
        if (!f)
            return fail("Cannot read boot reference", out);
        if (fgets(text, sizeof(text), f) && strlen(text) > 1 && !stricmp(text + 2, base + 2)) {
            fclose(f);
            return 1;
        }
        fclose(f);
    }
    for (i = 1; i < 65535; i++) {
        sprintf(name, "R%04X.REF", i);
        if (!pathjoin(out, dir, name))
            return 0;
        if (!exists(out))
            return 1;
    }
    return fail("Too many boot profiles", dir);
}
static int sharepart(const char *base, int remove, int low) {
    const char *area = low ? "WRMLOW" : "WRMBOOT";
    char ref[PATHLEN], cfg[PATHLEN], p[PATHLEN], active[PATHLEN], j[PATHLEN];
    struct find_t ff;
    FILE *f;
    if (!open_tx(shared_root, area, NONE) || !refname(base, area, ref))
        return 0;
    pathjoin(active, tx.dir, "ACTIVE.DAT");
    pathjoin(j, tx.dir, "TXN.DAT");
    if (exists(j) && (!load("TXN.DAT") || !publish()))
        return 0;
    if (remove) {
        if (!rmfile(ref))
            return fail("Cannot release boot profile", ref);
        pathjoin(p, tx.dir, "*.REF");
        if (!_dos_findfirst(p, _A_ARCH | _A_RDONLY, &ff))
            return 1;
        return !exists(active) || remove_tx();
    }
    if (!exists(active)) {
        if ((!low && !localasset(tx.dir, "HIMEMX.EXE", "HIMEMX.EXE")) ||
            !pathjoin(cfg, shared_root, "CONFIG.SYS") || !begin(cfg) ||
            !(low ? lowcfg(cfg, staged) : bootcfg(cfg, staged)) || !finish() || !journal())
            return 0;
    }
    if (exists(ref))
        return 1;
    f = fopen(ref, "wt");
    if (!f)
        return fail("Cannot record the boot profile", ref);
    fputs(base, f);
    return !fclose(f) || fail("Cannot save the boot profile", ref);
}
static int shareboot(const char *base, int remove, int id) {
    int needs_low = id == W30 || id == W31;
    if (remove && needs_low && !sharepart(base, 1, 1))
        return 0;
    if (!sharepart(base, remove, 0))
        return 0;
    return remove || !needs_low || sharepart(base, 0, 1);
}
static int splitboot(void) {
    char p[PATHLEN], s[PATHLEN], line[160];
    FILE *in, *out;
    int c;
    if (!pathjoin(p, shared_root, "AUTOEXEC.BAT") || !begin(p))
        return 0;
    if (!pathjoin(s, tx.base, "WINRAM.EXE"))
        return 0;
    strcpy(line, s + 2);
    strcat(line, " /LOWMEM");
    out = fopen(staged, "wb");
    if (!out)
        return fail("Cannot create startup file", staged);
    fprintf(out, "@%s\r\n", line);
    in = fopen(p, "rb");
    if (in) {
        while ((c = fgetc(in)) != EOF)
            fputc(c, out);
        fclose(in);
    }
    if (ferror(out) || fclose(out))
        return fail("Cannot stage SPLIT8MB startup", p);
    return finish();
}
static int verify_active(void) {
    unsigned i;
    if (!load("ACTIVE.DAT"))
        return 0;
    for (i = 0; i < tx.h.count; i++)
        if (!digest_is(tx.e[i].dst, tx.e[i].after))
            return fail("Installed file differs", tx.e[i].dst);
    return 1;
}
int transact(const char *path, int id, Options *o) {
    int lock, ok = 0, boot = (id <= WFW || (id >= D5 && id <= D8));
    char p[PATHLEN], j[PATHLEN], a[PATHLEN];
    live_mode = o->live && !boot;
    strcpy(shared_root, "C:\\");
    shared_root[0] = path[0];
    if (!open_tx(path, "WRMSTATE", id))
        return 0;
    pathjoin(p, tx.dir, "LOCK.DAT");
    lock = sopen(p, O_RDWR | O_CREAT | O_BINARY, SH_DENYRW, S_IREAD | S_IWRITE);
    if (lock < 0)
        return fail("Another WINRAM session is using this installation", path);
    pathjoin(j, tx.dir, "TXN.DAT");
    pathjoin(a, tx.dir, "ACTIVE.DAT");
    if (exists(j)) {
        if (o->verify) {
            fail("File changes are pending. Complete the operation before verification", path);
            goto end;
        }
        if (!load("TXN.DAT"))
            goto end;
        o->remove = tx.h.direction == 'U';
        if (boot && !o->remove &&
            (!shareboot(path, 0, id) || !open_tx(path, "WRMSTATE", id) || !load("TXN.DAT")))
            goto end;
        ok = publish();
        goto end;
    }
    if (o->verify) {
        ok = verify_active();
        goto end;
    }
    if (o->remove) {
        if (!exists(a)) {
            fail("WINRAM is not installed", path);
            goto end;
        }
        if (boot && !shareboot(path, 1, id))
            goto end;
        if (!open_tx(path, "WRMSTATE", id))
            goto end;
        ok = remove_tx();
        goto end;
    }
    if (exists(a)) {
        ok = 1;
        goto end;
    }
    if (!patch_binaries(id))
        goto end;
    if (id <= WFW)
        ok = prepare_early(id);
    else if (id >= D5 && id <= D8)
        ok = preparedos();
    else {
        ok = prepare9x(id, o);
        if (ok && o->split) {
            char localexe[PATHLEN];
            ok = pathjoin(localexe, path, "WINRAM.EXE") &&
                 (samefile(self, localexe) || clone_at(self, localexe)) && splitboot();
        }
    }
    if (!ok)
        goto end;
    if (!write_record("TXN.DAT")) {
        ok = 0;
        goto end;
    }
    if (boot) {
        if (!shareboot(path, 0, id) || !open_tx(path, "WRMSTATE", id) || !load("TXN.DAT")) {
            ok = 0;
            goto end;
        }
    }
    ok = publish();
end:
    close(lock);
    return ok;
}
static int temporary_overlay(const char *dir, char *out, const char *asset) {
    unsigned i;
    char leaf[13];
    int f;
    for (i = 0; i < 65535; i++) {
        sprintf(leaf, "WR%04X.OVL", i);
        if (!pathjoin(out, dir, leaf))
            return 0;
        f = open(out, O_CREAT | O_EXCL | O_WRONLY | O_BINARY, S_IREAD | S_IWRITE);
        if (f < 0) {
            if (errno == EEXIST)
                continue;
            return fail("Cannot reserve temporary overlay", out);
        }
        close(f);
        if (extract(asset, out))
            return 1;
        rmfile(out);
        return 0;
    }
    return fail("Temporary names exhausted", dir);
}

/* Keep the reference utility's .BAK contract and retain a separate .BKP even
   when a later reference uninstall consumes its .BAK. */
static int keep_reference_original(const char *path) {
    char base[PATHLEN], bak[PATHLEN], dst[PATHLEN], *leaf, *dot;
    unsigned index;
    if (!canonical(base, path))
        return fail("Invalid reference file path", path);
    strcpy(bak, base);
    leaf = strrchr(bak, '\\') + 1;
    dot = strrchr(leaf, '.');
    if (!dot)
        dot = bak + strlen(bak);
    if ((unsigned)(dot - bak) + 5 > sizeof(bak))
        return fail("Reference backup path is too long", path);
    strcpy(dot, ".BAK");
    if (!exists(bak))
        return 1;
    strcpy(dst, bak);
    strcpy(dst + (dot - bak), ".BKP");
    if (!exists(dst) || samefile(dst, bak))
        return exists(dst) || copyfile(bak, dst);
    *leaf = 0;
    for (index = 1; index < 65535; index++) {
        char name[13], original[PATHLEN];
        strcpy(original, base);
        dot = strrchr(strrchr(original, '\\') + 1, '.');
        if (!dot)
            dot = original + strlen(original);
        strcpy(dot, ".BAK");
        sprintf(name, "R%04X.BKP", index);
        if (!pathjoin(dst, bak, name))
            return 0;
        if (!exists(dst))
            return copyfile(original, dst);
        if (samefile(dst, original))
            return 1;
    }
    return fail("Reference backup directory is full", base);
}

static int keep_reference_backups(int argc, char **argv) {
    const char *names[] = {"SYSTEM\\VMM32.VXD", "SYSTEM\\VMM32\\VMM.VXD",
                           "SYSTEM\\VMM32\\VCACHE.VXD"};
    char root[PATHLEN] = "", path[PATHLEN], line[180];
    int i = 1, k;
    FILE *f;
    while (i < argc && argv[i][0] == '/') {
        if (!argv[i][1] || !strchr("ACIMP", toupper(argv[i][1])))
            return 1;
        i++;
    }
    if (i < argc && argc - i != 3)
        return 1;
    f = fopen("C:\\MSDOS.SYS", "rt");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strnicmp(line, "WinDir=", 7) && strlen(line + 7) < sizeof(root)) {
                strcpy(root, line + 7);
                break;
            }
        }
        fclose(f);
    }
    for (k = 0; k < 3; k++) {
        const char *selected = i == argc ? "*" : argv[i + k];
        if (*selected == '-')
            continue;
        if (*selected == '*') {
            if (!*root)
                continue;
            if (!pathjoin(path, root, names[k]))
                return 0;
            selected = path;
        }
        if (!keep_reference_original(selected))
            return 0;
    }
    return 1;
}

int raw_reference(int argc, char **argv) {
    char dir[PATHLEN], exe[PATHLEN], *tmp;
    char *args[32];
    int n, rc;
    if (argc >= 31) {
        puts("Too many PATCHMEM arguments.");
        return 2;
    }
    if (!keep_reference_backups(argc, argv)) {
        puts(error_msg);
        return 1;
    }
    tmp = getenv("TEMP");
    if (!tmp || !isdir(tmp))
        tmp = ".";
    if (!canonical(dir, tmp) || !temporary_overlay(dir, exe, "PATCHMEM.EXE")) {
        puts(error_msg);
        return 1;
    }
    args[0] = "PATCHMEM";
    for (n = 1; n < argc; n++)
        args[n] = argv[n];
    args[n] = NULL;
    fflush(stdout);
    rc = spawnv(P_WAIT, exe, (const char *const *)args);
    rmfile(exe);
    if (!keep_reference_backups(argc, argv)) {
        puts(error_msg);
        return 1;
    }
    return rc < 0 ? 1 : rc;
}

int run_splitter(void) {
    char file[PATHLEN], dir[PATHLEN];
    int rc;
    const char *args[] = {"SPLIT8MB", NULL};
    if (!canonical(dir, ".") || !temporary_overlay(dir, file, "SPLIT8MB.EXE"))
        return 1;
    fflush(stdout);
    rc = spawnv(P_WAIT, file, args);
    rmfile(file);
    return rc < 0 ? 1 : rc;
}
