/* Back unlocked private global objects with extended memory under pressure.
   Native Windows owns the handle and every pointer returned to the application. */
#include "pagestore.h"
#include <dos.h>
#include <string.h>

#define GMEM_INVALID_HANDLE 0x8000u
#define SLOTS 24
#define HASH_BUCKETS 128
#define TOKEN_BYTES 16
#define COOKIE 0x4d525750UL

typedef struct Block Block;
typedef struct Slab Slab;
struct Block {
    Block FAR *next;
    Slab FAR *slab;
    HGLOBAL object, window;
    unsigned locks;
    DWORD bytes, extent_bytes, serial;
    PageExtent extent;
    unsigned stored : 1, busy : 1, token : 1, pinned : 1, owns : 1;
    unsigned persistent : 1, fresh : 1;
};
struct Slab {
    Slab FAR *next;
    HGLOBAL object;
    unsigned count;
    Block slots[SLOTS];
};
typedef struct {
    DWORD magic, serial, inverse, reserved;
} Token;

static Block FAR *buckets[HASH_BUCKETS];
static Slab FAR *slabs;
static DWORD serial;
static int enabled, protected_mode, loaded, starting;
static unsigned scan_bucket, persistent_objects;
static HGLOBAL spare_window;
static DWORD spare_size;

/* Protected-mode selectors must exist for the entire duration of a lock.
   Keep a native, shared working allocation available for the next lock. */
static void discard_spare(void) {
    if (spare_window)
        GlobalFree(spare_window);
    spare_window = 0;
    spare_size = 0;
}
static HGLOBAL take_window(DWORD bytes) {
    HGLOBAL h;
    if (spare_window && spare_size >= bytes) {
        h = spare_window;
        spare_window = 0;
        spare_size = 0;
        return h;
    }
    h = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, bytes);
    return h;
}
static void return_window(HGLOBAL h) {
    DWORD bytes = GlobalSize(h);
    if (spare_window && bytes > spare_size)
        discard_spare();
    if (!spare_window && persistent_objects) {
        spare_window = h;
        spare_size = bytes;
    } else {
        GlobalFree(h);
    }
}
static void clear_window(void FAR *p, DWORD bytes) {
    WORD seg = FP_SEG(p), off = FP_OFF(p);
    while (bytes) {
        DWORD chunk = 0x10000UL - off;
        if (chunk > 32768)
            chunk = 32768;
        if (chunk > bytes)
            chunk = bytes;
        _fmemset(MK_FP(seg, off), 0, (WORD)chunk);
        off += (WORD)chunk;
        if (!off)
            seg += protected_mode ? 8 : 0x1000;
        bytes -= chunk;
    }
}

static unsigned bucket_of(HGLOBAL h) {
    return ((h >> 2) ^ (h >> 9)) % HASH_BUCKETS;
}
static Block FAR *lookup(HGLOBAL h) {
    Block FAR *b;
    for (b = buckets[bucket_of(h)]; b; b = b->next)
        if (b->object == h)
            return b;
    return NULL;
}
static void drop(Block FAR *b) {
    unsigned hash = bucket_of(b->object);
    Block FAR *p = buckets[hash], FAR *prev = NULL;
    Slab FAR *s = b->slab, FAR *last = NULL, FAR * cur;
    while (p != b) {
        prev = p;
        p = p->next;
    }
    if (prev)
        prev->next = b->next;
    else
        buckets[hash] = b->next;
    b->object = 0;
    if (--s->count)
        return;
    for (cur = slabs; cur != s; cur = cur->next)
        last = cur;
    if (last)
        last->next = s->next;
    else
        slabs = s->next;
    {
        HGLOBAL h = s->object;
        GlobalUnlock(h);
        GlobalFree(h);
    }
}
static Block FAR *remember(HGLOBAL h, DWORD bytes) {
    Slab FAR *s;
    Block FAR *b;
    unsigned i, hash = bucket_of(h);
    for (s = slabs; s && s->count == SLOTS; s = s->next)
        ;
    if (!s) {
        HGLOBAL meta = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT | GMEM_SHARE, sizeof(Slab));
        s = meta ? (Slab FAR *)GlobalLock(meta) : NULL;
        if (!s) {
            if (meta)
                GlobalFree(meta);
            return NULL;
        }
        s->object = meta;
        s->next = slabs;
        slabs = s;
    }
    for (i = 0; i < SLOTS && s->slots[i].object; i++)
        ;
    if (i == SLOTS)
        return NULL;
    b = &s->slots[i];
    _fmemset(b, 0, sizeof(*b));
    b->object = h;
    b->bytes = bytes;
    b->slab = s;
    b->serial = ++serial;
    b->next = buckets[hash];
    buckets[hash] = b;
    ++s->count;
    return b;
}
static int supported(UINT flags, DWORD bytes) {
    return enabled && (flags & GMEM_MOVEABLE) && !(flags & ~(GMEM_MOVEABLE | GMEM_ZEROINIT)) &&
           bytes > sizeof(Block) + TOKEN_BYTES;
}
static int cookie(Block FAR *b, int set) {
    Token FAR *t;
    int ok;
    if (!set && GlobalSize(b->object) > 64UL)
        return 0;
    t = (Token FAR *)GlobalLock(b->object);
    if (!t)
        return 0;
    if (set) {
        t->magic = COOKIE;
        t->serial = b->serial;
        t->inverse = ~b->serial;
        t->reserved = 0;
    }
    ok = t->magic == COOKIE && t->serial == b->serial && t->inverse == ~b->serial;
    GlobalUnlock(b->object);
    return ok;
}
static void forget(Block FAR *b) {
    if (b->owns && !store_release(&b->extent, b->extent_bytes))
        return;
    if (b->persistent) {
        if (b->window)
            GlobalFree(b->window);
        if (!--persistent_objects)
            discard_spare();
    }
    drop(b);
}
static int evict(Block FAR *b) {
    void FAR *ptr;
    PageExtent e;
    HGLOBAL source = b->window ? b->window : b->object, h;
    UINT flags;
    int ok;
    if (b->busy || b->stored || b->pinned || b->locks)
        return 0;
    flags = GlobalFlags(source);
    if (flags == GMEM_INVALID_HANDLE ||
        flags & (GMEM_LOCKCOUNT | GMEM_DISCARDED | GMEM_DISCARDABLE))
        return 0;
    if (GlobalSize(source) < b->bytes)
        return 0;
    b->busy = 1;
    ptr = GlobalLock(source);
    if (!ptr) {
        b->busy = 0;
        return 0;
    }
    if (b->owns && b->extent_bytes != b->bytes) {
        if (!store_release(&b->extent, b->extent_bytes)) {
            GlobalUnlock(source);
            b->busy = 0;
            return 0;
        }
        b->owns = 0;
    }
    e = b->extent;
    ok = b->owns || store_reserve(&e, b->bytes);
    if (ok && !store_transfer(&e, 0, ptr, b->bytes, 1)) {
        if (!b->owns)
            store_release(&e, b->bytes);
        ok = 0;
    }
    GlobalUnlock(source);
    if (ok) {
        if (b->window) {
            if (b->persistent) {
                return_window(b->window);
                ok = 1;
            } else
                ok = GlobalFree(b->window) == 0;
            if (ok)
                b->window = 0;
        } else {
            h = GlobalReAlloc(b->object, TOKEN_BYTES, GMEM_MOVEABLE);
            ok = h == b->object;
            if (ok) {
                b->token = protected_mode && b->bytes > 0xff00UL;
                cookie(b, 1);
            }
        }
        if (ok) {
            b->extent = e;
            b->extent_bytes = b->bytes;
            b->stored = 1;
            b->owns = 1;
        } else if (!b->owns)
            store_release(&e, b->bytes);
    }
    b->busy = 0;
    return ok;
}
static int make_room(Block FAR *except) {
    unsigned i, slot;
    Block FAR *b;
    for (i = 0; i < HASH_BUCKETS; i++) {
        slot = (scan_bucket + i) % HASH_BUCKETS;
        for (b = buckets[slot]; b; b = b->next) {
            if (b != except && evict(b)) {
                scan_bucket = (slot + 1) % HASH_BUCKETS;
                return 1;
            }
        }
    }
    return 0;
}
static int materialize(Block FAR *b) {
    void FAR *ptr;
    HGLOBAL h;
    int ok;
    if (!b || !b->stored)
        return 1;
    if (b->busy)
        return 0;
    if (!cookie(b, 0)) {
        forget(b);
        return 1;
    }
    b->busy = 1;
    do {
        h = b->persistent ? take_window(b->bytes)
            : b->token    ? GlobalAlloc(GMEM_MOVEABLE, b->bytes)
                          : GlobalReAlloc(b->object, b->bytes, GMEM_MOVEABLE);
    } while (!h && make_room(b));
    if (!h) {
        b->busy = 0;
        return 0;
    }
    if (!b->token && h != b->object) {
        b->busy = 0;
        return 0;
    }
    ptr = GlobalLock(h);
    if (ptr && b->fresh)
        clear_window(ptr, b->bytes);
    ok = ptr && (b->fresh || store_transfer(&b->extent, 0, ptr, b->bytes, 0));
    if (ptr)
        GlobalUnlock(h);
    if (ok) {
        if (b->token)
            b->window = h;
        if (!b->persistent && store_release(&b->extent, b->extent_bytes))
            b->owns = 0;
        b->fresh = 0;
        b->stored = 0;
    } else if (b->token)
        GlobalFree(h);
    else if (GlobalReAlloc(h, TOKEN_BYTES, GMEM_MOVEABLE) == h)
        cookie(b, 1);
    b->busy = 0;
    return ok;
}
#define PAGER_API FAR PASCAL __loadds
int PAGER_API WrStart(void);
static HGLOBAL resident(HGLOBAL h) {
    Block FAR *b = lookup(h);
    if (!materialize(b))
        return 0;
    b = lookup(h);
    return b && b->window ? b->window : h;
}
static HGLOBAL public_handle(HGLOBAL h) {
    unsigned i;
    Block FAR *b;
    for (i = 0; i < HASH_BUCKETS; i++)
        for (b = buckets[i]; b; b = b->next)
            if (b->window == h)
                return b->object;
    return h;
}
/* Reserve backing and a real working window before promising a large object.
   The first lock sees zeroes and later locks read the bytes written previously. */
static HGLOBAL allocate_large(DWORD bytes) {
    HGLOBAL object, window;
    DWORD capacity;
    PageExtent extent;
    Block FAR *b;
    window = take_window(bytes);
    if (!window)
        return 0;
    capacity = bytes;
    if (GlobalSize(window) < bytes || !store_reserve(&extent, capacity)) {
        return_window(window);
        return 0;
    }
    object = GlobalAlloc(GMEM_MOVEABLE, TOKEN_BYTES);
    if (object && (b = lookup(object)) != NULL)
        forget(b);
    b = object && !lookup(object) ? remember(object, capacity) : NULL;
    if (!b || !cookie(b, 1)) {
        if (b)
            drop(b);
        if (object)
            GlobalFree(object);
        store_release(&extent, capacity);
        return_window(window);
        return 0;
    }
    b->token = b->owns = b->stored = b->persistent = b->fresh = 1;
    b->extent = extent;
    b->extent_bytes = capacity;
    ++persistent_objects;
    return_window(window);
    return object;
}
HGLOBAL PAGER_API WrAlloc(UINT flags, DWORD bytes) {
    HGLOBAL h;
    Block FAR *b;
    if (!enabled && loaded && !starting && (flags & GMEM_MOVEABLE))
        WrStart();
    if (bytes >= (protected_mode ? 65536UL : 16384UL) && supported(flags, bytes)) {
        h = allocate_large(bytes);
        if (h)
            return h;
    }
    do {
        h = GlobalAlloc(flags, bytes);
    } while (!h && enabled && !(flags & GMEM_NOCOMPACT) && make_room(NULL));
    if (h) {
        b = lookup(h);
        if (b && !b->busy)
            forget(b);
        if (supported(flags, bytes) && !lookup(h))
            remember(h, GlobalSize(h));
    }
    return h;
}
void FAR *PAGER_API WrLock(HGLOBAL h) {
    HGLOBAL live = resident(h);
    void FAR *ptr;
    if (!live)
        return NULL;
    ptr = GlobalLock(live);
    if (ptr && live != h && !GlobalLock(h)) {
        GlobalUnlock(live);
        return NULL;
    }
    if (ptr) {
        Block FAR *b = lookup(h);
        if (b) {
            if (b->locks == 0xffffu)
                b->pinned = 1;
            else
                ++b->locks;
        }
    }
    return ptr;
}
BOOL PAGER_API WrUnlock(HGLOBAL h) {
    Block FAR *b = lookup(h);
    if (b && b->locks)
        --b->locks;
    if (b && b->window) {
        BOOL result;
        GlobalUnlock(h);
        result = GlobalUnlock(b->window);
        if (b->persistent && !b->locks && !b->pinned)
            evict(b);
        return result;
    }
    return GlobalUnlock(h);
}
DWORD PAGER_API WrSize(HGLOBAL h) {
    Block FAR *b = lookup(h);
    if (b && (b->window || (b->stored && cookie(b, 0))))
        return b->bytes;
    return GlobalSize(h);
}
UINT PAGER_API WrFlags(HGLOBAL h) {
    Block FAR *b = lookup(h);
    return GlobalFlags(b && b->window ? b->window : h) &
           (b && b->persistent ? ~GMEM_SHARE : 0xffffu);
}
DWORD PAGER_API WrHandle(UINT segment) {
    DWORD result = GlobalHandle(segment);
    if (!LOWORD(result))
        return result;
    return MAKELONG(public_handle(LOWORD(result)), HIWORD(result));
}
HGLOBAL PAGER_API WrFree(HGLOBAL h) {
    Block FAR *b = lookup(h);
    HGLOBAL failed;
    if (b && b->busy)
        return h;
    if (b && b->window) {
        if (GlobalFree(b->window))
            return h;
        b->window = 0;
    }
    failed = GlobalFree(h);
    if (!failed && b)
        forget(b);
    return failed;
}
HGLOBAL PAGER_API WrReAlloc(HGLOBAL h, DWORD bytes, UINT flags) {
    Block FAR *b = lookup(h);
    HGLOBAL live, res;
    if (!b) {
        do {
            res = GlobalReAlloc(h, bytes, flags);
        } while (!res && bytes && enabled && !(flags & GMEM_NOCOMPACT) && make_room(NULL));
        return res;
    }
    if (b->busy || !materialize(b))
        return NULL;
    b = lookup(h);
    if (!b)
        return GlobalReAlloc(h, bytes, flags);
    b->busy = 1;
    live = b->window ? b->window : h;
    do {
        res = GlobalReAlloc(live, bytes, flags);
    } while (!res && bytes && enabled && !(flags & GMEM_NOCOMPACT) && make_room(b));
    b->busy = 0;
    if (!res) {
        if (!bytes && GlobalFlags(live) == GMEM_INVALID_HANDLE) {
            if (live != h)
                GlobalFree(h);
            forget(b);
        }
        return res;
    }
    if (!(flags & GMEM_MODIFY) && supported(flags | GMEM_MOVEABLE, bytes)) {
        DWORD size = b->persistent ? bytes : GlobalSize(res);
        if (live != h || res == h) {
            if (live != h)
                b->window = res;
            b->bytes = size;
            return h;
        }
        /* Windows may return a different handle when it grows a native object. */
        {
            unsigned locks = b->locks, pinned = b->pinned;
            forget(b);
            b = remember(res, size);
            if (b) {
                b->locks = locks;
                b->pinned = pinned;
            }
        }
        return res;
    }
    if (live != h) {
        GlobalFree(h);
        b->window = 0;
    }
    forget(b);
    return res;
}
/* Additional pinning conventions retain a native resident object until free. */
static HGLOBAL pin(HGLOBAL h) {
    HGLOBAL live = resident(h);
    Block FAR *b = lookup(h);
    if (live && b)
        b->pinned = 1;
    return live;
}
typedef UINT(WINAPI *WordAPI)(UINT);
typedef void(WINAPI *VoidAPI)(UINT);
typedef void FAR *(WINAPI *PointerAPI)(UINT);
static FARPROC kernel_api(unsigned ordinal) {
    return GetProcAddress(GetModuleHandle("KERNEL"), (LPCSTR)(DWORD)ordinal);
}
void FAR *PAGER_API WrWire(HGLOBAL h) {
    HGLOBAL live = pin(h);
    PointerAPI f = (PointerAPI)kernel_api(111);
    return live && f ? f(live) : NULL;
}
BOOL PAGER_API WrUnwire(HGLOBAL h) {
    HGLOBAL live = resident(h);
    WordAPI f = (WordAPI)kernel_api(112);
    return live && f ? f(live) : 0;
}
UINT PAGER_API WrPageLock(HGLOBAL h) {
    HGLOBAL live = pin(h);
    WordAPI f = (WordAPI)kernel_api(191);
    return live && f ? f(live) : 0;
}
UINT PAGER_API WrPageUnlock(HGLOBAL h) {
    HGLOBAL live = resident(h);
    WordAPI f = (WordAPI)kernel_api(192);
    return live && f ? f(live) : 0;
}
void PAGER_API WrFix(HGLOBAL h) {
    HGLOBAL live = pin(h);
    VoidAPI f = (VoidAPI)kernel_api(197);
    if (live && f)
        f(live);
}
void PAGER_API WrUnfix(HGLOBAL h) {
    HGLOBAL live = resident(h);
    VoidAPI f = (VoidAPI)kernel_api(198);
    if (live && f)
        f(live);
}
HGLOBAL PAGER_API WrLockSegment(UINT h) {
    HGLOBAL live = pin(h);
    return live ? LockSegment(live) : 0;
}
void PAGER_API WrUnlockSegment(UINT h) {
    HGLOBAL live = resident(h);
    if (live)
        UnlockSegment(live);
}
void FAR *PAGER_API WrLockResource(HGLOBAL h) {
    HGLOBAL live = pin(h);
    return live ? LockResource(live) : NULL;
}
void PAGER_API WrSweep(void) {
    unsigned i;
    Block FAR *b, FAR *next;
    for (i = 0; i < HASH_BUCKETS; i++)
        for (b = buckets[i]; b; b = next) {
            next = b->next;
            if (!b->busy && (GlobalFlags(b->object) == GMEM_INVALID_HANDLE ||
                             ((b->stored || b->window) && !cookie(b, 0)))) {
                forget(b);
            }
        }
}
int PAGER_API WrStart(void) {
    typedef DWORD(WINAPI * FlagsProc)(void);
    FlagsProc flags = NULL;
#ifndef WIN1_PAGER
    flags = (FlagsProc)GetProcAddress(GetModuleHandle("KERNEL"), (LPCSTR)132L);
#endif
    if (enabled || starting)
        return enabled;
    starting = 1;
    protected_mode = flags && (flags() & 1);
    enabled = store_open();
#ifdef WIN1_PAGER
    if (enabled) {
        typedef int(WINAPI * Activate)(void);
        Activate activate = (Activate)kernel_api(101);
        enabled = activate && activate();
    }
#endif
    starting = 0;
    return enabled;
}
int FAR PASCAL LibMain(HINSTANCE instance, WORD data, WORD heap, LPSTR args) {
    (void)instance;
    (void)data;
    (void)heap;
    (void)args;
    loaded = 1;
    return 1;
}
int PAGER_API WEP(int reason) {
    unsigned i;
    Block FAR *b, FAR *next;
    (void)reason;
#ifdef WIN1_PAGER
    if (enabled) {
        typedef void(WINAPI * Deactivate)(void);
        Deactivate deactivate = (Deactivate)kernel_api(102);
        if (deactivate)
            deactivate();
    }
#endif
    loaded = enabled = 0;
    for (i = 0; i < HASH_BUCKETS; ++i) {
        for (b = buckets[i]; b; b = next) {
            next = b->next;
            if (b->owns)
                store_release(&b->extent, b->extent_bytes);
            if (b->persistent && b->window)
                GlobalFree(b->window);
            drop(b);
        }
    }
    discard_spare();
    store_close();
    return 1;
}
