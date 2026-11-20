#ifndef WINRAM_COMMON_H
#define WINRAM_COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PATHLEN 128
#define DIGLEN 32
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
typedef unsigned char Byte;
typedef unsigned long U32;
enum Profile {
    NONE,
    W101,
    W203,
    W286,
    W386,
    W30,
    W31,
    WFW,
    W95,
    W98,
    WME,
    D5,
    D6,
    D7,
    D8,
    GEN95,
    GEN98,
    GENME
};
typedef struct {
    int id;
    const char *path;
    long size;
    const char *sha;
} Signature;
typedef struct {
    int id;
    const char *path;
    long objects;
} Patch;
typedef struct {
    const char *name;
    long offset, size;
    const char *sha;
} Asset;
typedef struct Target {
    struct Target *next;
    int id;
    char path[PATHLEN];
} Target;
typedef struct {
    int agp, move, prothook, cache, split, only, live, remove, yes, list, verify;
    char path[PATHLEN];
} Options;
extern const char *names[];
extern Signature sigs[];
extern Asset assets[];
extern Patch patches[];
extern char self[PATHLEN], error_msg[264];
extern Target *targets;
extern int windows_major;
void detect_host(void);
int running_windows(const char *path);
extern long packbase;
int fail(const char *message, const char *path);
int pathjoin(char *out, const char *dir, const char *file);
int canonical(char *out, const char *in);
int exists(const char *path);
int isdir(const char *path);
int copyfile(const char *src, const char *dest);
int hashfile(const char *path, Byte digest[DIGLEN]);
void hexhash(const Byte *bin, char *hex);
int matches(const char *path, const char *hex);
int samefile(const char *a, const char *b);
int rmfile(const char *path);
int startpack(const char *exe);
int extract(const char *name, const char *destination);
int detect(const char *path);
const char *display_name(int id, const char *base);
int installed(const char *path);
int pending(const char *path);
void scan(void);
void free_targets(void);
int select_path(const char *in, char *out);
int parse_args(int argc, char **argv, Options *o);
int prepare_queue(const char *path, int id, const Options *o);
int transact(const char *path, int id, Options *o);
#endif
