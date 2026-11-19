#include "common.h"
#include "transaction.h"
static char line[1024], original[1024];
static char *trim(char *s) {
    char *p;
    while (isspace((unsigned char)*s))
        s++;
    p = s + strlen(s);
    while (p > s && isspace((unsigned char)p[-1]))
        --p;
    *p = 0;
    return s;
}
static int write_setting(FILE *d, const char *key, const char *value) {
    return fprintf(d, "%s=%s\n", key, value) >= 0;
}
/* Rewrite only the requested section and key. Append mode preserves existing startup applications.
 */
int ini(const char *file, const char *out, const char *section, const char *key, const char *value,
        int mode) {
    FILE *s = fopen(file, "rt"), *d = fopen(out, "wt");
    int in = 0, saw = 0, wrote = 0, ok = 1;
    char *p, *q, *rhs, section_name[82];
    if (!d) {
        if (s)
            fclose(s);
        return fail("Cannot create configuration", out);
    }
    sprintf(section_name, "[%s]", section);
    while (s && fgets(line, sizeof(line), s)) {
        if (!strchr(line, '\n') && !feof(s)) {
            ok = 0;
            break;
        }
        strcpy(original, line);
        p = trim(line);
        if (*p == '[') {
            if (in && !wrote && value) {
                ok = write_setting(d, key, value);
                wrote = 1;
            }
            in = !stricmp(p, section_name);
            if (in)
                saw = 1;
        } else if (in && (q = strchr(p, '=')) != NULL) {
            *q = 0;
            rhs = trim(q + 1);
            if (!stricmp(trim(p), key)) {
                if (mode == 2) {
                    if (!stricmp(rhs, value))
                        wrote = 1; /* multi-valued device line */
                } else {
                    if (!wrote && value) {
                        if (mode == 1 && *rhs) {
                            if (strstr(strupr(rhs), value))
                                fputs(original, d);
                            else
                                fprintf(d, "%s=%s %s\n", key, value,
                                        trim(strchr(original, '=') + 1));
                        } else
                            write_setting(d, key, value);
                    }
                    wrote = 1;
                    continue;
                }
            }
        }
        fputs(original, d);
        if (!strchr(original, '\n'))
            fputc('\n', d);
    }
    if (s) {
        if (ferror(s))
            ok = 0;
        fclose(s);
    }
    if (value && !wrote) {
        if (!saw)
            fprintf(d, "\n%s\n", section_name);
        write_setting(d, key, value);
    }
    if (ferror(d) || fclose(d))
        ok = 0;
    return ok ? 1 : fail("Cannot edit configuration", file);
}
static int driver(const char *rhs) {
    char a[160], *p, *e;
    while (isspace((unsigned char)*rhs))
        rhs++;
    strncpy(a, rhs, sizeof(a) - 1);
    a[sizeof(a) - 1] = 0;
    p = a;
    if (*p == '"')
        p++;
    e = strpbrk(p, " \t\"");
    if (e)
        *e = 0;
    e = strrchr(p, '\\');
    if (e)
        p = e + 1;
    else if (p[1] == ':')
        p += 2;
    return !stricmp(p, "HIMEM.SYS") || !stricmp(p, "HIMEMX.EXE") || !stricmp(p, "HIMEMX2.EXE");
}
int bootcfg(const char *file, const char *out) {
    FILE *s = fopen(file, "rt"), *d;
    char *p, *eq;
    int multi = 0, ok = 1;
    if (s) {
        while (fgets(line, sizeof(line), s))
            if (!stricmp(trim(line), "[menu]"))
                multi = 1;
        rewind(s);
    }
    d = fopen(out, "wt");
    if (!d) {
        if (s)
            fclose(s);
        return fail("Cannot create boot configuration", out);
    }
    if (multi)
        fputs("[common]\n", d);
    fputs("DEVICE=\\WRMBOOT\\HIMEMX.EXE /NUMHANDLES=128\nFILES=255\n", d);
    while (s && fgets(line, sizeof(line), s)) {
        if (!strchr(line, '\n') && !feof(s)) {
            ok = 0;
            break;
        }
        strcpy(original, line);
        p = trim(line);
        eq = strchr(p, '=');
        if (eq) {
            *eq = 0;
            p = trim(p);
            if (!stricmp(p, "FILES") || !stricmp(p, "FILESHIGH"))
                continue;
            if ((!stricmp(p, "DEVICE") || !stricmp(p, "DEVICEHIGH")) && driver(eq + 1))
                continue;
        }
        fputs(original, d);
    }
    if (s) {
        if (ferror(s))
            ok = 0;
        fclose(s);
    }
    if (ferror(d) || fclose(d))
        ok = 0;
    return ok ? 1 : fail("Cannot edit boot configuration", file);
}
static unsigned word(const Byte *p) {
    return p[0] | ((unsigned)p[1] << 8);
}
int set_profile(const char *path, int minor) {
    FILE *f = fopen(path, "r+b");
    Byte hdr[32], tab[2048];
    long base, off;
    unsigned n, i, j, k, used;
    int ok = 0;
    if (!f)
        return fail("Cannot open SETVER table", path);
    if (fread(hdr, 1, 32, f) != 32 || memcmp(hdr, "MZ", 2))
        goto end;
    base = (long)word(hdr + 8) * 16;
    if (fseek(f, base, 0) || fread(hdr, 1, 26, f) != 26 || memcmp(hdr + 10, "SETVER", 6))
        goto end;
    off = base + word(hdr + 20) + 65536L * word(hdr + 22);
    n = word(hdr + 24);
    if (!n || n > sizeof(tab) || fseek(f, off, 0) || fread(tab, 1, n, f) != n)
        goto end;
    i = 0;
    k = 0;
    while (i < n && tab[i]) {
        j = tab[i];
        if (j > 12 || i + j + 3 > n)
            goto end;
        if (j != 12 || memicmp(tab + i + 1, "WINSTART.COM", 12)) {
            memmove(tab + k, tab + i, j + 3);
            k += j + 3;
        }
        i += j + 3;
    }
    used = k + 16;
    if (used > n)
        goto end;
    tab[k++] = 12;
    memcpy(tab + k, "WINSTART.COM", 12);
    k += 12;
    tab[k++] = 3;
    tab[k++] = minor;
    memset(tab + k, 0, n - k);
    ok = !fseek(f, off, 0) && fwrite(tab, 1, n, f) == n;
end:
    if (fclose(f))
        ok = 0;
    return ok ? 1 : fail("Unrecognized SETVER version table", path);
}
/* DOSX requires the High Memory Area to be available on these reference
   Windows 3.x Standard Mode builds. Keep UMB and NOAUTO modifiers intact. */
int lowcfg(const char *src, const char *out) {
    FILE *in = fopen(src, "rt"), *d = fopen(out, "wt");
    char *p, *eq, *word, *end;
    int ok = 1;
    if (!d) {
        if (in)
            fclose(in);
        return fail("Cannot create DOSX boot profile", out);
    }
    while (in && fgets(line, sizeof(line), in)) {
        strcpy(original, line);
        if (!strnicmp(line, "DEVICE=\\WRMBOOT\\HIMEMX.EXE ",
                      strlen("DEVICE=\\WRMBOOT\\HIMEMX.EXE ")) &&
            !strstr(line, "/METHOD:")) {
            fputs("DEVICE=\\WRMBOOT\\HIMEMX.EXE /METHOD:FAST /NUMHANDLES=128\n", d);
            continue;
        }
        p = trim(line);
        eq = strchr(p, '=');
        if (eq) {
            *eq = 0;
            if (!stricmp(trim(p), "DOS")) {
                word = eq + 1;
                for (;;) {
                    while (*word == ' ' || *word == '\t')
                        word++;
                    end = word;
                    while (isalpha((unsigned char)*end))
                        end++;
                    if (end - word == 4 && !memicmp(word, "HIGH", 4)) {
                        size_t j = word - line;
                        memcpy(original + j, "LOW", 3);
                        memmove(original + j + 3, original + j + 4, strlen(original + j + 4) + 1);
                        break;
                    }
                    if (*end != ',')
                        break;
                    word = end + 1;
                }
            }
        }
        fputs(original, d);
    }
    if (in) {
        if (ferror(in))
            ok = 0;
        fclose(in);
    }
    if (ferror(d) || fclose(d))
        ok = 0;
    return ok || fail("Cannot save DOSX boot profile", out);
}
