#include "wdm_target.h"
/* Resident VMM system pages for kernel-owned objects. Each occupied run starts
 * on a page boundary. The first allocation map and its data live in the same
 * locked VMM allocation, so freeing never needs another allocation. */
#include "resident.h"

#define WK_PAGE_SIZE 4096UL
#define RESERVE_PAGES 16UL
#define CONTINUATION ((ULONG)-1)

typedef struct _ARENA {
    struct _ARENA *next;
    PUCHAR data;
    ULONG handle;
    ULONG capacity;
    ULONG used;
    ULONG map[];
} ARENA;

static ARENA *arenas;
static ARENA *reserve;
extern PVOID WkGetSystemPages(ULONG pages, PULONG handle);
extern VOID WkReturnSystemPages(ULONG handle);
extern KIRQL WkMaskInterrupts(void);
extern void WkRestoreInterrupts(KIRQL previous);

static ARENA *createArena(ULONG capacity)
{
    ULONG mapPages, handle;
    ARENA *arena;
    /* The payload comes from a 32-bit address space. Consequently its map
     * takes at most 4 MiB and the arithmetic below fits in an ULONG. */
    mapPages = (sizeof(ARENA) + capacity * sizeof(ULONG) + WK_PAGE_SIZE - 1) / WK_PAGE_SIZE;
    if (capacity + mapPages > 0xffffffffUL / WK_PAGE_SIZE)
        return NULL;
    arena = WkGetSystemPages(capacity + mapPages, &handle);
    if (arena) {
        arena->handle = handle;
        arena->capacity = capacity;
        arena->data = (PUCHAR)arena + mapPages * WK_PAGE_SIZE;
    }
    return arena;
}

BOOLEAN WkStartResidentPool(void)
{
    reserve = createArena(WkTargetVersion() == WK_FULL_TARGET ? RESERVE_PAGES : 4);
    arenas = reserve;
    return reserve != NULL;
}

static PVOID takeRun(ARENA *arena, ULONG pages)
{
    ULONG i, available = 0;
    if (pages > arena->capacity - arena->used)
        return NULL;
    for (i = 0; i < arena->capacity; ++i) {
        if (arena->map[i]) {
            available = 0;
            continue;
        }
        if (++available == pages) {
            ULONG first = i + 1 - pages;
            while (i > first)
                arena->map[i--] = CONTINUATION;
            arena->map[first] = pages;
            arena->used += pages;
            return arena->data + first * WK_PAGE_SIZE;
        }
    }
    return NULL;
}

PVOID WkResidentAllocate(SIZE_T bytes)
{
    ULONG pages;
    ARENA *arena;
    PVOID allocation = NULL;
    KIRQL previous;
    if (!bytes || bytes > 0xfffff000UL)
        return NULL;
    pages = (ULONG)((bytes - 1) / WK_PAGE_SIZE + 1);
    previous = WkMaskInterrupts();
    for (arena = arenas; arena && !allocation; arena = arena->next)
        allocation = takeRun(arena, pages);
    WkRestoreInterrupts(previous);
    if (allocation || previous || !reserve)
        return allocation;

    /* VMM page services may block. An interrupt caller only uses the pages
     * already available in an arena, returning NULL if those are exhausted. */
    arena = createArena(pages < RESERVE_PAGES ? RESERVE_PAGES : pages);
    if (!arena)
        return NULL;
    previous = WkMaskInterrupts();
    arena->next = arenas;
    arenas = arena;
    allocation = takeRun(arena, pages);
    WkRestoreInterrupts(previous);
    return allocation;
}

VOID WkResidentFree(PVOID address)
{
    ARENA **link;
    ULONG handle = 0;
    ULONG_PTR value = (ULONG_PTR)address;
    KIRQL previous;
    if (!address || value % WK_PAGE_SIZE) return;
    previous = WkMaskInterrupts();
    for (link = &arenas; *link; link = &(*link)->next) {
        ARENA *arena = *link;
        ULONG_PTR base = (ULONG_PTR)arena->data;
        ULONG i, count;
        if (value < base || (value - base) / WK_PAGE_SIZE >= arena->capacity)
            continue;
        i = (ULONG)((value - base) / WK_PAGE_SIZE);
        count = arena->map[i];
        if (count && count != CONTINUATION) {
            arena->used -= count;
            while (count--)
                arena->map[i++] = 0;
            if (!arena->used && arena != reserve && !previous) {
                *link = arena->next;
                handle = arena->handle;
            }
        }
        break;
    }
    WkRestoreInterrupts(previous);
    if (handle) WkReturnSystemPages(handle);
}

VOID WkStopResidentPool(void)
{
    KIRQL previous = WkMaskInterrupts();
    ARENA *arena = arenas;
    arenas = reserve = NULL;
    WkRestoreInterrupts(previous);
    while (arena) {
        ARENA *next = arena->next;
        WkReturnSystemPages(arena->handle);
        arena = next;
    }
}
