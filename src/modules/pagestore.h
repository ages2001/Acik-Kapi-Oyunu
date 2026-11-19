#ifndef WINRAM_PAGESTORE_H
#define WINRAM_PAGESTORE_H

#include <windows.h>

typedef struct {
    DWORD base;
    DWORD handle;
} PageExtent;

int store_open(void);
int store_reserve(PageExtent FAR *extent, DWORD bytes);
int store_transfer(const PageExtent FAR *extent, DWORD offset, void FAR *buffer, DWORD bytes,
                   int writing);
int store_release(const PageExtent FAR *extent, DWORD bytes);
void store_close(void);

#endif
