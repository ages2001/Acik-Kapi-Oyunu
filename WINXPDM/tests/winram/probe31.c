/* Verify enhanced Windows 3.1 memory with the Setup-personalized WINRAM catalog.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <i86.h>
#include <string.h>

#define BLOCKS 2048
#define BLOCK_BYTES 61440UL
#define PAGES_PER_BLOCK 15
static HGLOBAL blocks[BLOCKS];

static void emit(const char *text)
{
    while (*text) {
        unsigned char value = *text++;
        _asm { mov al, value }
        _asm { out 0e9h, al }
    }
}

static void number(DWORD value)
{
    char digits[11];
    unsigned i = sizeof(digits) - 1;
    digits[i] = 0;
    do { digits[--i] = (char)('0' + value % 10); value /= 10; } while (value);
    emit(digits + i);
}

static DWORD physical_pages(void)
{
    static DWORD state[12];
    union REGS in, out;
    struct SREGS segments;
    DWORD FAR *address = state;
    memset(&in, 0, sizeof(in));
    segread(&segments);
    segments.es = FP_SEG(address);
    in.w.di = FP_OFF(address);
    in.w.ax = 0x0500;
    int86x(0x31, &in, &out, &segments);
    return out.w.cflag ? 0xffffffffUL : state[6];
}

static BOOL check_pages(WORD block, BOOL write)
{
    BYTE FAR *memory = GlobalLock(blocks[block]);
    WORD page;
    BOOL ok = TRUE;
    if (!memory) return FALSE;
    for (page = 0; page < PAGES_PER_BLOCK; ++page) {
        WORD pos = page * 4096U;
        BYTE marker = (BYTE)(block ^ (block >> 8) ^ page ^ 0xa7);
        if (write) memory[pos] = marker;
        else if (memory[pos] != marker) ok = FALSE;
    }
    GlobalUnlock(blocks[block]);
    return ok;
}

int PASCAL WinMain(HINSTANCE inst, HINSTANCE previous, LPSTR args, int show)
{
    DWORD physical = physical_pages();
    WORD i, count = 0;
    BOOL ok = physical != 0xffffffffUL && physical >= 230400UL;
    (void)inst; (void)previous; (void)args; (void)show;
    emit("DPMI physical pages="); number(physical); emit("\n");
    for (i = 0; i < BLOCKS; ++i) {
        blocks[i] = GlobalAlloc(GMEM_MOVEABLE, BLOCK_BYTES);
        if (!blocks[i]) { ok = FALSE; break; }
        ++count;
        if (!check_pages(i, TRUE)) { ok = FALSE; break; }
    }
    for (i = 0; i < count; ++i) {
        if (!check_pages(i, FALSE)) ok = FALSE;
        if (GlobalFree(blocks[i])) ok = FALSE;
    }
    emit("Win16 read/write/release KiB="); number((DWORD)count * 60);
    emit(ok ? " PASS\n" : " FAIL\n");
    if (WinExec("C:\\WINDOWS\\GPUCHECK.EXE", SW_SHOW) < 32) emit("GPUCHECK launch failed\n");
    return ok ? 0 : 1;
}
