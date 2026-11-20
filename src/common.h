#ifndef N3_COMMON_H
#define N3_COMMON_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "patch.h"
#define TITLE "NT3SCPRT"
#define ICON_APP 10
#define ICON_DEL 11
#define DLG_MAIN 20
#define FIELD_OS 100
#define INSTALL 101
#define REMOVE 102
#define ADD 103
#define ADDBOOT 104
#define OPTICAL 105
extern char osname[96], appdir[MAX_PATH], appfile[MAX_PATH], drivers[MAX_PATH], self[MAX_PATH];
extern unsigned minor;
typedef struct {
    byte *p;
    u32 n;
} File;
int readfile(const char *, File *);
int equal(const File *, const File *);
int matches(const char *, const File *);
int create(const char *, const File *);
int missing(const char *);
const char *add_driver(const char *, char *, int);
const char *install_class(const char *);
const char *remove_class(void);
const char *remove_drivers(void);
const char *check_system_disk(void);
int join(char *, const char *, const char *);
const char *port_action(int remove, int *changed);
const char *install_program(void);
const char *remove_program(void);
int icons(int remove);
#endif
