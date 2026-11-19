#include "queue.h"
#include <i86.h>

int windows_major;
static char current_windows[PATHLEN];

static void choose_current(const char *name) {
    char full[PATHLEN];
    if (!current_windows[0] && name && isalpha(name[0]) && name[1] == ':' &&
        canonical(full, name) && isdir(full))
        strcpy(current_windows, full);
}

/* Real-mode Windows may suspend its multiplex services while it runs an old
   application. DOS still records the path of each EXEC ancestor in its PSP. */
static int windows_parent(void) {
    unsigned seg = _psp, depth;
    char path[PATHLEN], *leaf;
    int found = 0;
    for (depth = 0; depth < 16 && seg >= 0x50 && seg < 0xa000; depth++) {
        unsigned __far *psp = MK_FP(seg, 0);
        unsigned parent = psp[0x16 / 2], envseg = psp[0x2c / 2];
        if (envseg >= 0x50 && envseg < 0xa000) {
            char __far *env = MK_FP(envseg, 0);
            unsigned __far *mcb = MK_FP(envseg - 1, 0);
            unsigned paras = *(unsigned __far *)MK_FP(envseg - 1, 3);
            unsigned k, len, max = paras >= 2048 ? 32768 : paras * 16;
            if ((*(char __far *)mcb == 'M' || *(char __far *)mcb == 'Z') && max >= 7) {
                for (k = 0; k + 5 < max && (env[k] || env[k + 1]); k++)
                    ;
                if (k + 5 < max && env[k + 2] == 1 && env[k + 3] == 0) {
                    k += 4;
                    for (len = 0; k < max && len < sizeof(path) - 1 && env[k]; k++)
                        path[len++] = env[k];
                    path[len] = 0;
                    leaf = strrchr(path, '\\');
                    if (leaf &&
                        (!stricmp(leaf + 1, "WIN.COM") || !stricmp(leaf + 1, "WIN386.EXE") ||
                         !stricmp(leaf + 1, "WINOLDAP.MOD") || !stricmp(leaf + 1, "WINOA386.MOD") ||
                         !stricmp(leaf + 1, "WRMCORE.COM") || !stricmp(leaf + 1, "WIN86.COM"))) {
                        *leaf = 0;
                        choose_current(path);
                        found = 1;
                    }
                }
            }
        }
        if (parent == seg)
            break;
        seg = parent;
    }
    return found;
}

void detect_host(void) {
    union REGS in, out;
    char path[PATHLEN], line[180];
    unsigned boot;
    FILE *f;
    memset(&in, 0, sizeof(in));
    in.w.ax = 0x3305;
    intdos(&in, &out);
    boot = out.h.al == 0xff ? 0 : out.h.dl;
    if (boot >= 1 && boot <= 26)
        hostroot[0] = 'A' + boot - 1;

    in.w.ax = 0x160a;
    int86(0x2f, &in, &out);
    if (!out.w.ax && out.h.bh >= 3 && out.h.bh <= 9)
        windows_major = out.h.bh;
    if (!windows_major) {
        in.w.ax = 0x1600;
        int86(0x2f, &in, &out);
        if (out.h.al && out.h.al != 0x80)
            windows_major = out.h.al == 0xff ? 2 : out.h.al;
    }
    if (!windows_major) {
        in.w.ax = 0x4680;
        int86(0x2f, &in, &out);
        if (!out.w.ax)
            windows_major = 3;
    }
    if (windows_parent() && !windows_major)
        windows_major = 2;
    if (!windows_major)
        return;
    choose_current(getenv("windir"));
    choose_current(getenv("WINDIR"));
    choose_current(getenv("winbootdir"));
    if (windows_major >= 4 && !current_windows[0] && pathjoin(path, hostroot, "MSDOS.SYS")) {
        f = fopen(path, "rt");
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\r\n")] = 0;
                if (!strnicmp(line, "WinDir=", 7)) {
                    choose_current(line + 7);
                    break;
                }
            }
            fclose(f);
        }
    }
    if ((!boot || boot > 26) && current_windows[0])
        hostroot[0] = current_windows[0];
}

int running_windows(const char *path) {
    return windows_major >= 4 && (!current_windows[0] || !stricmp(current_windows, path));
}
