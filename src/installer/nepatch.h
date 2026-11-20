#ifndef WINRAM_NEPATCH_H
#define WINRAM_NEPATCH_H
#include <stdio.h>

/* The caller supplies a private, seekable staging file opened for update. */
const char *patch_ne_user(FILE *stage, int win9x);
const char *patch_pe_memory(FILE *stage);
const char *patch_ne_panel(FILE *stage);
unsigned bound_windows_version(FILE *file);
int pe_product_version(FILE *file, unsigned *minor, unsigned *build);
int patch_objects(FILE *stage, FILE *rules);
int digest_at(FILE *file, long start, long length, unsigned char *actual);
int ne_windows_version(FILE *file, const char *module);
#endif
