/* A normal Win16 application. Redirect the QEMU debug port to a file to
   collect results. Build with Open Watcom and a windows1 or windows target. */
#include <windows.h>
#include <dos.h>
#include <conio.h>

#ifndef BLOCK_KIB
#define BLOCK_KIB 32UL
#endif
#ifndef BLOCK_COUNT
#define BLOCK_COUNT 1024
#endif
#define BLOCK_BYTES (BLOCK_KIB * 1024UL)

static HGLOBAL handles[BLOCK_COUNT];
static WORD segment_step;

static void message(const char *s) {
    while (*s)
        outp(0xe9, *s++);
}
static void number(DWORD value) {
    char digits[12];
    unsigned n = 0;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (n)
        outp(0xe9, digits[--n]);
}
static void field(const char *name, DWORD value) {
    message(name);
    number(value);
    message("\n");
}
static DWORD FAR *at(void FAR *base, DWORD offset) {
    DWORD absolute = FP_OFF(base) + offset;
    WORD seg = FP_SEG(base) + (WORD)(absolute >> 16) * segment_step;
    return (DWORD FAR *)MK_FP(seg, (WORD)absolute);
}
static int check_block(HGLOBAL handle, unsigned index, int writing) {
    void FAR *first = GlobalLock(handle);
    void FAR *second;
    DWORD page;
    int ok;
    if (!first)
        return 0;
    second = GlobalLock(handle);
    ok = first == second && GlobalSize(handle) >= BLOCK_BYTES &&
         LOWORD(GlobalHandle(FP_SEG(first))) == handle;
    if (second)
        GlobalUnlock(handle);
    for (page = 0; page < BLOCK_BYTES && ok; page += 4096) {
        DWORD value = 0x6d000000UL ^ ((DWORD)index * 0x00201f7bUL) ^ (page >> 12);
        DWORD FAR *head = at(first, page);
        DWORD FAR *tail = at(first, page + 4092);
        if (writing) {
            *head = value;
            *tail = ~value;
        } else {
            ok = *head == value && *tail == ~value;
        }
    }
    GlobalUnlock(handle);
    return ok;
}
int PASCAL WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    unsigned n = 0, i, written = 0, verified = 0, released = 0;
    typedef DWORD(WINAPI * FlagsProc)(void);
    (void)instance;
    (void)previous;
    (void)command;
    (void)show;
    segment_step = 0x1000;
    if (LOBYTE(LOWORD(GetVersion())) >= 3) {
        FlagsProc flags = (FlagsProc)GetProcAddress(GetModuleHandle("KERNEL"), (LPCSTR)132L);
        if (flags && (flags() & 1))
            segment_step = 8;
    }
    field("Block KiB=", BLOCK_KIB);
    field("Requested blocks=", BLOCK_COUNT);
    while (n < BLOCK_COUNT) {
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, BLOCK_BYTES);
        if (!h) {
            message("Allocator refused the next block\n");
            break;
        }
        handles[n++] = h;
        if (!check_block(h, n - 1, 1)) {
            field("Write or handle check failed at block=", n - 1);
            break;
        }
        ++written;
        if (!(n % 256))
            field("Written KiB=", (DWORD)n * BLOCK_KIB);
    }
    field("Simultaneously held KiB=", (DWORD)n * BLOCK_KIB);
    for (i = 0; i < n; ++i) {
        if (i < written && check_block(handles[i], i, 0))
            ++verified;
        else
            field("Read or handle check failed at block=", i);
        if (!GlobalFree(handles[i]))
            ++released;
    }
    field("Written blocks=", written);
    field("Verified blocks=", verified);
    field("Released blocks=", released);
    message(n && n == verified && n == released ? "Integrity PASS\n" : "Integrity FAIL\n");
    message("GLOBALMEM END\n");
    return verified != n || released != n;
}
