#ifndef WINRAM_TX_H
#define WINRAM_TX_H
#include "common.h"
#define MAXFILES 48
typedef struct {
    char dst[PATHLEN], bkp[13], tmp[13];
    unsigned exists, attr, date, time;
    Byte before[32], after[32];
} Entry;
typedef struct {
    char magic[8];
    unsigned count, profile;
    char direction;
    char reserved;
} Header;
typedef struct {
    char dir[PATHLEN], base[PATHLEN];
    Header h;
    Entry e[MAXFILES];
} Tx;
int ini(const char *, const char *, const char *, const char *, const char *, int);
int lowcfg(const char *, const char *);
int bootcfg(const char *, const char *);
int set_profile(const char *, int);
#endif
