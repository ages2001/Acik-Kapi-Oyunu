/* Read the enhanced-mode WINRAM bank and exercise a 16 MiB reservation.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <conio.h>

static DWORD bank(WORD operation)
{
    WORD signature = 0;
    DWORD value = 0;
    _asm {
        push si
        push di
        mov ax, 0d74dh
        mov bx, 04258h
        mov cx, operation
        mov edx, 32
        mov esi, 512
        int 2fh
        mov signature, ax
        mov value, edx
        pop di
        pop si
    }
    return signature == 0x5842 ? value : 0xffffffffUL;
}

static void emit(const char *message)
{
    while (*message) outp(0xe9, *message++);
}

int PASCAL WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR args, int show)
{
    char line[160];
    DWORD size, before, held, after;
    (void)instance; (void)previous; (void)args; (void)show;
    if (!(GetWinFlags() & WF_ENHANCED)) {
        emit("WINRAM probe expected enhanced mode\n");
        return 1;
    }
    size = bank(0);
    before = bank(4);
    if (size == 0xffffffffUL || size < 512UL * 1024 || bank(5) == 0xffffffffUL) {
        emit("WINRAM enhanced bank was unavailable\n");
        return 1;
    }
    held = bank(4);
    if (bank(6) == 0xffffffffUL) return 1;
    after = bank(4);
    wsprintf(line, "WINRAM enhanced bank_KiB=%lu free_before=%lu free_reserved=%lu free_released=%lu\n",
             size, before, held, after);
    emit(line);
    if (held == 0xffffffffUL || before - held != 16UL * 1024 || after != before) return 1;
    emit("WINRAM enhanced reservation PASS\n");
    return 0;
}
