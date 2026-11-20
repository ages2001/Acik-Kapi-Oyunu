#include "queue.h"
#include <errno.h>
static int __far hardfail(unsigned word, unsigned code, unsigned __far *dev) {
    (void)word;
    (void)code;
    (void)dev;
    return _HARDERR_FAIL;
}
static void help(void) {
    puts("WINRAM - DOS and Windows memory support\n"
         "WINRAM [drive:\\path] [/INSTALL | /UNINSTALL | /VERIFY] [/YES]\n"
         "WINRAM /PIF                  Write early Windows task settings beside the EXE\n"
         "WINRAM /LIST                 Scan mounted DOS drives\n"
         "WINRAM /PATH drive:\\path     Select an installation directly\n"
         "WINRAM /PATCHMEM [options] [VMM32File VMMFile VCACHEFile]\n"
         "Windows 9x options: /A /C:MiB /I /M /P /SPLIT8MB\n"
         "Without arguments, enter a system number or a directory. Enter 0 to exit.");
}
static int choose(char *out) {
    Target *t;
    unsigned i = 0;
    char item[PATHLEN], *e;
    long n;
    scan();
    for (t = targets; t; t = t->next)
        printf("%3u  %s  %s%s%s\n", ++i, display_name(t->id, t->path), t->path,
               installed(t->path) ? " [installed]" : "", pending(t->path) ? " [pending]" : "");
    printf("System number or path (0 to exit): ");
    fflush(stdout);
    if (!fgets(item, sizeof(item), stdin))
        return 0;
    item[strcspn(item, "\r\n")] = 0;
    n = strtol(item, &e, 10);
    if (e != item && !*e) {
        if (n <= 0)
            return 0;
        for (t = targets; t && --n; t = t->next)
            ;
        if (!t) {
            fail("Unknown system number", item);
            return -1;
        }
        strcpy(out, t->path);
        return t->id;
    }
    if ((i = select_path(item, out)) == 0) {
        fail("No supported installation found", item);
        return -1;
    }
    return i;
}
static int run_selected(char *p, int id, Options *o) {
    char answer[32];
    int state = installed(p);
    printf("%s\n%s\n", display_name(id, p), p);
    if (state && !o->verify && !pending(p)) {
        if (o->only) {
            puts("Already installed. Install-only request skipped.");
            return 1;
        }
        if (!o->remove || !o->yes) {
            printf("Already installed. Uninstall? [y/N] ");
            fflush(stdout);
            if (!fgets(answer, sizeof(answer), stdin) || toupper(answer[0]) != 'Y') {
                puts("Kept installed.");
                return 1;
            }
        }
        o->remove = 1;
    }
    if (windows_major && windows_major < 4 && !o->verify) {
        if (!prepare_queue(p, id, o))
            return 0;
        puts("Operation queued. Restart DOS to apply it before Windows starts.");
        return 1;
    }
    if ((id >= W95 && id <= WME) || (id >= GEN95 && id <= GENME))
        o->live |= running_windows(p);
    if (!transact(p, id, o))
        return 0;
    if (o->verify)
        puts("Installation verified.");
    else if (o->live)
        puts("Operation queued for the next Windows boot.");
    else
        puts(o->remove ? "Uninstalled. Original backups retained."
                       : "Installed. Restart the target system.");
    return 1;
}
int main(int argc, char **argv) {
    Options o;
    int id, ok = 0;
    char path[PATHLEN];
    Target *t;
    unsigned n = 0;
    _harderr(hardfail);
    detect_host();
    puts("WINRAM 1.0.1");
    if (windows_major)
        puts("Running inside Windows.");
    if (!startpack(argv[0]))
        goto end;
    if (argc == 2 && !stricmp(argv[1], "/PIF")) {
        char *leaf;
        strcpy(path, self);
        leaf = strrchr(path, '\\');
        if (!leaf)
            goto end;
        leaf[1] = 0;
        if (!pathjoin(path, path, "WINRAM.PIF"))
            goto end;
        if (exists(path)) {
            puts("WINRAM.PIF already exists.");
            return 0;
        }
        ok = extract("WINRAM.PIF", path);
        if (ok)
            puts("Created WINRAM.PIF beside the executable.");
        goto end;
    }
    if (argc > 1 && !stricmp(argv[1], "/LOWMEM")) {
        int run_splitter(void);
        return run_splitter();
    }
    if (argc > 1 && !stricmp(argv[1], "/RUNQUEUE")) {
        ok = run_queue();
        goto end;
    }
    if (argc > 1 && !stricmp(argv[1], "/PATCHMEM")) {
        int raw_reference(int, char **);
        return raw_reference(argc - 1, argv + 1);
    }
    if (argc > 1 && (!stricmp(argv[1], "/?") || !stricmp(argv[1], "/HELP"))) {
        help();
        return 0;
    }
    if (!parse_args(argc, argv, &o)) {
        help();
        return 2;
    }
    if (o.list) {
        scan();
        for (t = targets; t; t = t->next)
            printf("%u  %s  %s%s\n", ++n, display_name(t->id, t->path), t->path,
                   installed(t->path) ? " [installed]" : "");
        printf("%u installation(s).\n", n);
        return 0;
    }
    if (o.path[0]) {
        id = select_path(o.path, path);
        if (!id) {
            fail("No supported installation found", o.path);
            goto end;
        }
    } else {
        do {
            id = choose(path);
            if (id < 0)
                puts(error_msg);
        } while (id < 0);
        if (!id)
            return 0;
    }
    ok = run_selected(path, id, &o);
end:
    if (!ok)
        fprintf(stderr, "WINRAM: %s\n", error_msg);
    return ok ? 0 : 1;
}
