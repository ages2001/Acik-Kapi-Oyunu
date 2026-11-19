#include "queue.h"
char hostroot[4] = "C:\\";
int write_job(const char *file, const char *path, int id, const Options *opt) {
    Job j;
    FILE *f;
    memset(&j, 0, sizeof(j));
    memcpy(j.magic, "WINJOB1", 8);
    j.id = id;
    j.opts = *opt;
    strcpy(j.opts.path, path);
    j.opts.yes = 1;
    if (!j.opts.remove)
        j.opts.only = 1;
    f = fopen(file, "wb");
    if (!f)
        return fail("Cannot create queued operation", file);
    if (fwrite(&j, sizeof(j), 1, f) != 1) {
        fclose(f);
        return fail("Cannot write queued operation", file);
    }
    return !fclose(f);
}
int read_job(const char *p, Job *j) {
    FILE *f = fopen(p, "rb");
    int ok;
    if (!f)
        return fail("Cannot read queued operation", p);
    ok = fread(j, sizeof(*j), 1, f) == 1 && !memcmp(j->magic, "WINJOB1", 8) &&
         j->opts.path[PATHLEN - 1] == 0;
    fclose(f);
    return ok || fail("Invalid queued operation", p);
}
int prepare_queue(const char *path, int id, const Options *o) {
    char dir[PATHLEN], dest[PATHLEN], autoexec[PATHLEN], origin[PATHLEN], entry[PATHLEN],
        marker[PATHLEN], backup[PATHLEN];
    unsigned n;
    int missing;
    FILE *f;
    Job job;
    if (!pathjoin(dir, hostroot, "WRMQUEUE") || (!isdir(dir) && mkdir(dir)))
        return fail("Cannot create the boot queue", hostroot);
    if (!pathjoin(dest, dir, "WINRAM.EXE") || !copyfile(self, dest))
        return 0;
    pathjoin(origin, dir, "ORIGIN.BAT");
    pathjoin(autoexec, hostroot, "AUTOEXEC.BAT");
    pathjoin(marker, dir, "BOOT.DAT");
    if (!exists(marker)) {
        missing = !exists(autoexec);
        if (!missing) {
            for (n = 1; n < 65535; n++) {
                char b[13];
                sprintf(b, "A%04X.BKP", n);
                pathjoin(backup, dir, b);
                if (!exists(backup)) {
                    if (!copyfile(autoexec, backup))
                        return 0;
                    break;
                }
                if (samefile(autoexec, backup))
                    break;
            }
            if (!copyfile(autoexec, origin))
                return 0;
        } else {
            f = fopen(origin, "wb");
            if (!f)
                return fail("Cannot prepare startup", origin);
            fclose(f);
        }
        pathjoin(dest, dir, "START.BAT");
        f = fopen(dest, "wt");
        if (!f)
            return fail("Cannot create startup command", dest);
        fprintf(f,
                "@%sWRMQUEUE\\WINRAM.EXE /RUNQUEUE\n@IF NOT ERRORLEVEL 1 DEL "
                "%sWRMQUEUE\\WINRAM.EXE\n@CALL %sWRMQUEUE\\ORIGIN.BAT\n",
                hostroot, hostroot, hostroot);
        if (fclose(f))
            return fail("Cannot save startup command", dest);
        f = fopen(marker, "wt");
        if (!f)
            return fail("Cannot create queue index", marker);
        fprintf(f, "%d\n", missing);
        if (fclose(f))
            return 0;
        pathjoin(dest, dir, "BOOT.NEW");
        f = fopen(dest, "wt");
        if (!f)
            return fail("Cannot create boot hook", dest);
        fprintf(f, "@%sWRMQUEUE\\START.BAT\n", hostroot);
        if (fclose(f) || !copyfile(dest, autoexec)) {
            rmfile(marker);
            return fail("Cannot install boot hook", autoexec);
        }
    }
    for (n = 1; n < 65535; n++) {
        char name[13];
        sprintf(name, "Q%04X.JOB", n);
        pathjoin(entry, dir, name);
        if (!exists(entry))
            return write_job(entry, path, id, o);
        if (read_job(entry, &job) && !stricmp(job.opts.path, path))
            return fail("An operation for this target is already queued", path);
    }
    return fail("Boot queue is full", dir);
}
int run_queue(void) {
    struct find_t ff;
    char spec[PATHLEN], dir[PATHLEN], p[PATHLEN], a[PATHLEN], origin[PATHLEN], marker[PATHLEN];
    Job j;
    int missing;
    FILE *f;
    hostroot[0] = self[0];
    pathjoin(dir, hostroot, "WRMQUEUE");
    pathjoin(spec, dir, "*.JOB");
    while (!_dos_findfirst(spec, _A_ARCH, &ff)) {
        pathjoin(p, dir, ff.name);
        if (!read_job(p, &j) || !transact(j.opts.path, j.id, &j.opts))
            return 0;
        printf("Completed: %s\n", j.opts.path);
        if (!rmfile(p))
            return fail("Cannot clear completed operation", p);
    }
    pathjoin(a, hostroot, "AUTOEXEC.BAT");
    pathjoin(p, dir, "BOOT.NEW");
    pathjoin(origin, dir, "ORIGIN.BAT");
    pathjoin(marker, dir, "BOOT.DAT");
    f = fopen(marker, "rt");
    if (!f)
        return fail("Boot queue index is missing", marker);
    missing = fgetc(f) == '1';
    fclose(f);
    if (samefile(a, p)) {
        if (missing) {
            if (!rmfile(a))
                return 0;
        } else if (!copyfile(origin, a))
            return 0;
    }
    return rmfile(marker);
}
