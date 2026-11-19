/* Win16 backing storage. Each transfer belongs to a previously reserved extent. */
#include "pagestore.h"
#include <dos.h>
#include <string.h>

#define UNIT 32768UL
#define ARENA_UNITS 2048
enum Backend { OFFLINE, BANK, XMS, DPMI };
static enum Backend backend;
static int protected_mode;
static WORD transfer_selector, transfer_offset, linear_selector;
static DWORD bank_units, bank_hint, bank_free_units;
static void(FAR *xms_driver)(void);

typedef struct Arena Arena;
struct Arena {
    Arena FAR *next;
    HGLOBAL allocation;
    WORD handle, units, used, hint;
    BYTE bits[ARENA_UNITS / 8];
};
static Arena FAR *arenas;

typedef struct {
    DWORD di, si, bp, reserved, bx, dx, cx, ax;
    WORD flags, es, ds, fs, gs, ip, cs, sp, ss;
} RealCall;
static RealCall rm;
static struct {
    DWORD length;
    WORD source;
    DWORD source_offset;
    WORD destination;
    DWORD destination_offset;
} move_request;

static DWORD units_for(DWORD bytes) {
    return bytes ? 1 + (bytes - 1) / UNIT : 0;
}

/* DOSX preserves the full range operands on its reflected INT 2Fh path.
   The transfer ABI also accepts split 16-bit positions on older DPMI hosts. */
static int bank_range(WORD function, DWORD first, DWORD count) {
    WORD signature;
    _asm {
        push esi
        push edi
        mov ax,0d74dh
        mov bx,04258h
        mov cx,function
        mov edx,first
        mov esi,count
        int 2fh
        mov signature,ax
        pop edi
        pop esi
    }
    return signature == 0x5842;
}

static int bank_call(WORD function, DWORD pos, WORD count) {
    WORD signature, high, low, buf_segment, ofs;
    if (protected_mode) {
        WORD accepted = 0;
        memset(&rm, 0, sizeof(rm));
        rm.ax = 0xd74d;
        rm.bx = 0x4258;
        rm.cx = function;
        rm.dx = LOWORD(pos);
        rm.bp = HIWORD(pos);
        rm.si = count;
        _asm {
            push es
            push ds
            pop es
            mov di,offset rm
            mov ax,0300h
            mov bx,002fh
            xor cx,cx
            int 31h
            jc done
            mov accepted,1
        done:
            pop es
        }
        if (!accepted) return 0;
        signature = (WORD)rm.ax;
        high = (WORD)rm.cx;
        low = (WORD)rm.dx;
        buf_segment = (WORD)rm.si;
        ofs = (WORD)rm.di;
    } else {
        _asm {
            push esi
            push edi
            mov edx,pos
            mov ax,0d74dh
            mov bx,04258h
            mov cx,function
            mov si,count
            push ebp
            mov ebp,edx
            shr ebp,16
            int 2fh
            pop ebp
            mov signature,ax
            mov high,cx
            mov low,dx
            mov buf_segment,si
            mov ofs,di
            pop edi
            pop esi
        }
    }
    if (signature != 0x5842)
        return 0;
    if (!function) {
        bank_units = MAKELONG(low, high) / 32;
        transfer_selector = buf_segment;
        transfer_offset = ofs;
    } else if (function == 4) {
        bank_free_units = MAKELONG(low, high) / 32;
    }
    return 1;
}

static int xms_alloc(DWORD kib, WORD FAR *id) {
    WORD ok, object;
    _asm {
        mov edx,kib
        mov ah,89h
        call dword ptr xms_driver
        mov ok,ax
        mov object,dx
    }
    *id = object;
    return ok != 0;
}
static int xms_free(WORD object) {
    WORD ok;
    _asm {
        mov dx,object
        mov ah,0ah
        call dword ptr xms_driver
        mov ok,ax
    }
    return ok != 0;
}
static int xms_move(WORD handle, DWORD at, void FAR *ptr, WORD count, int write) {
    WORD ok;
    move_request.length = count;
    move_request.source = write ? 0 : handle;
    move_request.source_offset = write ? (DWORD)ptr : at;
    move_request.destination = write ? handle : 0;
    move_request.destination_offset = write ? at : (DWORD)ptr;
    _asm {
        mov si,offset move_request
        mov ah,0bh
        call dword ptr xms_driver
        mov ok,ax
    }
    return ok != 0;
}

static WORD allocate_selector(void) {
    WORD selector = 0;
    _asm {
        xor ax,ax
        mov cx,1
        int 31h
        jc failed
        mov selector,ax
    failed:
    }
    return selector;
}

int store_open(void) {
    typedef DWORD(WINAPI * FlagsProc)(void);
    FlagsProc getflags;
    DWORD flags;
    WORD selector, valid = 0;
    if (backend)
        return 1;
    getflags = NULL;
#ifndef WIN1_PAGER
    getflags = (FlagsProc)GetProcAddress(GetModuleHandle("KERNEL"), (LPCSTR)132L);
#endif
    flags = getflags ? getflags() : 0;
    protected_mode = (flags & 1) != 0;
    if (bank_call(0, 0, 0) && bank_units) {
        if (protected_mode) {
            selector = transfer_selector;
            _asm {
                mov bx,selector
                mov ax,2
                int 31h
                jc no_alias
                mov selector,ax
                mov valid,1
            no_alias:
            }
            if (!valid) return 0;
            transfer_selector = selector;
        }
        backend = BANK;
        bank_hint = bank_units;
        return 1;
    }
    if (protected_mode) {
        linear_selector = allocate_selector();
        if (!linear_selector)
            return 0;
        backend = DPMI;
        return 1;
    }
    _asm {
        push es
        mov ax,4300h
        int 2fh
        cmp al,80h
        jne no_driver
        mov ax,4310h
        int 2fh
        mov word ptr xms_driver,bx
        mov word ptr xms_driver+2,es
        xor ah,ah
        call dword ptr xms_driver
        cmp ax,0300h
        jb no_driver
        mov valid,1
    no_driver:
        pop es
    }
    if (valid) backend = XMS;
    return valid;
}

static int find_units(Arena FAR *a, WORD need, WORD FAR *first) {
    unsigned pos, run = 0;
    if ((unsigned)(a->units - a->used) < need)
        return 0;
    /* Search both sides of the hint, including a free run spanning it. */
    for (pos = a->hint; pos < a->units; ++pos) {
        if (a->bits[pos / 8] & (1u << (pos % 8)))
            run = 0;
        else if (++run == need) {
            *first = pos + 1 - need;
            return 1;
        }
    }
    run = 0;
    for (pos = 0; pos < a->units; ++pos) {
        if (a->bits[pos / 8] & (1u << (pos % 8)))
            run = 0;
        else if (++run == need) {
            *first = pos + 1 - need;
            return 1;
        }
    }
    return 0;
}
static int reserve_xms(PageExtent FAR *e, DWORD units) {
    Arena FAR *a;
    WORD first = 0, n, handle;
    unsigned i;
    HGLOBAL object;
    if (units > ARENA_UNITS)
        return 0;
    n = (WORD)units;
    for (a = arenas; a; a = a->next)
        if (find_units(a, n, &first))
            break;
    if (!a) {
        WORD capacity = arenas ? ARENA_UNITS : 128;
        if (capacity < n)
            capacity = n;
        while (!xms_alloc((DWORD)capacity * 32, &handle)) {
            if (capacity == n)
                return 0;
            capacity /= 2;
            if (capacity < n)
                capacity = n;
        }
        object = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT | GMEM_SHARE, sizeof(Arena));
        a = object ? (Arena FAR *)GlobalLock(object) : NULL;
        if (!a) {
            if (object)
                GlobalFree(object);
            xms_free(handle);
            return 0;
        }
        a->allocation = object;
        a->handle = handle;
        a->units = capacity;
        a->next = arenas;
        arenas = a;
    }
    for (i = first; i < first + n; ++i)
        a->bits[i / 8] |= 1u << (i % 8);
    a->used += n;
    a->hint = first + n;
    e->base = (DWORD)first * UNIT;
    e->handle = a->handle;
    return 1;
}
int store_reserve(PageExtent FAR *e, DWORD bytes) {
    DWORD units = units_for(bytes);
    DWORD first, tries, candidates;
    WORD hi = HIWORD(bytes), lo = LOWORD(bytes), hhi, hlo, ok = 0;
    if (!units || !store_open())
        return 0;
    if (backend == XMS)
        return reserve_xms(e, units);
    if (backend == BANK) {
        if (units > bank_units || !bank_call(4, 0, 0) || units > bank_free_units)
            return 0;
        candidates = bank_units - units + 1;
        first = bank_hint >= units ? bank_hint - units : candidates - 1;
        if (first >= candidates)
            first = candidates - 1;
        for (tries = 0; tries < candidates; ++tries) {
            if (bank_range(5, first, units)) {
                bank_hint = first;
                e->base = first * UNIT;
                e->handle = 0;
                return 1;
            }
            first = first ? first - 1 : candidates - 1;
        }
        return 0;
    }
    _asm {
        mov ax,0501h
        mov bx,hi
        mov cx,lo
        int 31h
        jc failed
        mov hi,bx
        mov lo,cx
        mov hhi,si
        mov hlo,di
        mov ok,1
    failed:
    }
    if (ok) {
        e->base = MAKELONG(lo, hi);
        e->handle = MAKELONG(hlo, hhi);
    }
    return ok;
}

static int map_linear(DWORD base) {
    WORD hi = HIWORD(base), lo = LOWORD(base), ok = 0;
    _asm {
        mov bx,linear_selector
        mov cx,hi
        mov dx,lo
        mov ax,7
        int 31h
        jc failed
        mov bx,linear_selector
        xor cx,cx
        mov dx,07fffh
        mov ax,8
        int 31h
        jc failed
        mov ok,1
    failed:
    }
    return ok;
}

int store_transfer(const PageExtent FAR *e, DWORD offset, void FAR *buffer, DWORD bytes,
                   int writing) {
    WORD buf_segment = FP_SEG(buffer), ofs = FP_OFF(buffer), part, even, tail;
    BYTE FAR *ptr;
    DWORD at = e->base + offset;
    while (bytes) {
        DWORD span = 0x10000UL - ofs;
        part = (WORD)(bytes > UNIT ? UNIT : bytes);
        if (part > span)
            part = (WORD)span;
        ptr = (BYTE FAR *)MK_FP(buf_segment, ofs);
        if (backend == DPMI) {
            if (!map_linear(at))
                return 0;
            if (writing)
                _fmemcpy(MK_FP(linear_selector, 0), ptr, part);
            else
                _fmemcpy(ptr, MK_FP(linear_selector, 0), part);
        } else if (backend == BANK) {
            void FAR *bounce = MK_FP(transfer_selector, transfer_offset);
            WORD padded = part + (part & 1);
            if (writing) {
                _fmemcpy(bounce, ptr, part);
                if (part & 1)
                    ((BYTE FAR *)bounce)[part] = 0;
            }
            if (!bank_call(writing ? 2 : 1, at, padded))
                return 0;
            if (!writing)
                _fmemcpy(ptr, bounce, part);
        } else {
            even = part & ~1u;
            if (even && !xms_move((WORD)e->handle, at, ptr, even, writing))
                return 0;
            if (part & 1) {
                /* Preserve the neighbour when a segment ends on an odd byte. */
                if (!xms_move((WORD)e->handle, at + even, (void FAR *)&tail, 2, 0))
                    return 0;
                if (writing) {
                    tail = (tail & 0xff00u) | ptr[even];
                    if (!xms_move((WORD)e->handle, at + even, (void FAR *)&tail, 2, 1))
                        return 0;
                } else
                    ptr[even] = (BYTE)tail;
            }
        }
        at += part;
        bytes -= part;
        ofs += part;
        if (!ofs && bytes)
            buf_segment += protected_mode ? 8 : 0x1000;
    }
    return 1;
}

int store_release(const PageExtent FAR *e, DWORD bytes) {
    DWORD units = units_for(bytes);
    WORD hi = HIWORD(e->handle), lo = LOWORD(e->handle), ok = 0;
    if (backend == BANK)
        return bank_range(6, e->base / UNIT, units);
    if (backend == XMS) {
        Arena FAR *a = arenas;
        Arena FAR *previous = NULL;
        WORD first = (WORD)(e->base / UNIT), i;
        while (a && a->handle != (WORD)e->handle) {
            previous = a;
            a = a->next;
        }
        if (!a || units > a->units || first > a->units - units)
            return 0;
        if (a->used == units) {
            HGLOBAL allocation = a->allocation;
            if (!xms_free(a->handle))
                return 0;
            if (previous)
                previous->next = a->next;
            else
                arenas = a->next;
            GlobalUnlock(allocation);
            GlobalFree(allocation);
        } else {
            for (i = first; i < first + (WORD)units; ++i)
                a->bits[i / 8] &= ~(1u << (i % 8));
            a->used -= (WORD)units;
            a->hint = first;
        }
        return 1;
    }
    _asm {
        mov ax,0502h
        mov si,hi
        mov di,lo
        int 31h
        jc failed
        mov ok,1
    failed:
    }
    return ok;
}
void store_close(void) {
    if (backend == DPMI && linear_selector) {
        _asm {mov ax,1
              mov bx,linear_selector
              int 31h}
        linear_selector = 0;
    }
    backend = OFFLINE;
}
