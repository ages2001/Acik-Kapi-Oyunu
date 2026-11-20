/* Direct MMIO qualification on the dedicated QEMU board at PCI 00:06.0.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdint.h>
#ifdef WXP_BOARD16
#include <windows.h>
#include <i86.h>
#include <conio.h>
#include "../../../src/drivers/pci16.h"
typedef DWORD board_word;
typedef volatile DWORD FAR *board_regs;
typedef struct { board_regs regs; DWORD linear; WORD selector; } Mapping;

static DWORD map_linear(WORD hi, WORD lo);
#pragma aux map_linear = "mov ax,0800h" "xor si,si" "mov di,1000h" "int 31h" \
    "jnc ok" "xor bx,bx" "xor cx,cx" "ok:" "mov ax,cx" "mov dx,bx" \
    parm [bx] [cx] value [dx ax] modify [si di];
static void release_linear(WORD hi, WORD lo);
#pragma aux release_linear = "mov ax,0801h" "int 31h" parm [bx] [cx] modify [ax];
static WORD alloc_selector(void);
#pragma aux alloc_selector = "xor ax,ax" "mov cx,1" "int 31h" "jnc ok" "xor ax,ax" "ok:" value [ax] modify [cx];
static void free_selector(WORD selector);
#pragma aux free_selector = "mov ax,1" "int 31h" parm [bx] modify [ax];
static WORD selector_base(WORD selector, WORD hi, WORD lo);
#pragma aux selector_base = "mov ax,7" "int 31h" "sbb ax,ax" parm [bx] [cx] [dx] value [ax];
static WORD selector_limit(WORD selector);
#pragma aux selector_limit = "mov ax,8" "xor cx,cx" "mov dx,0fffh" "int 31h" "sbb ax,ax" \
    parm [bx] value [ax] modify [cx dx];

static void unmap(Mapping *m)
{
    if (m->selector) free_selector(m->selector);
    if (m->linear) release_linear(HIWORD(m->linear), LOWORD(m->linear));
}

static int map(Mapping *m, DWORD address)
{
    m->linear = map_linear(HIWORD(address), LOWORD(address));
    if (!m->linear) return 0;
    m->selector = alloc_selector();
    if (!m->selector || selector_base(m->selector, HIWORD(m->linear), LOWORD(m->linear)) ||
        selector_limit(m->selector)) return 0;
    m->regs = (board_regs)MK_FP(m->selector, 0);
    return 1;
}

static board_word cfg(unsigned offset, unsigned width, board_word value, int write)
{
    return pci_access(0, 6 << 3, offset, width, value, write);
}

static void emit(const char *text)
{
    while (*text) outp(0xe9, *text++);
}

static void board_delay(void)
{
    /* A PCI configuration read serializes the preceding MMIO accesses. */
    cfg(0, 4, 0, 0);
}
#elif defined(WXP_BOARD_VXD)
#include "../../../src/drivers/nt/pci31.h"
typedef uint32_t board_word;
typedef volatile uint32_t *board_regs;
typedef struct { board_regs regs; } Mapping;
extern uintptr_t wxp_map_vmm_page(uint32_t address);

static int map(Mapping *m, uint32_t address)
{
    uintptr_t linear = wxp_map_vmm_page(address);
    if (!linear || linear == UINT32_MAX) return 0;
    m->regs = (board_regs)linear;
    return 1;
}

static void unmap(Mapping *m)
{
    /* VMM owns the physical mappings for the lifetime of this static VxD. */
    (void)m;
}

static board_word cfg(unsigned offset, unsigned width, board_word value, int write)
{
    return wxp_pci31_cycle(0, 6 << 3, offset, width, value, write);
}

static void emit(const char *text)
{
    while (*text) __asm__ volatile("outb %0,$0xe9" : : "a"((unsigned char)*text++));
}

static void board_delay(void)
{
    cfg(0, 4, 0, 0);
}
#else
#include <ntddk.h>
#ifdef WXP_NT31
#include "../../../src/drivers/nt/pci31.h"
#endif
typedef ULONG board_word;
typedef volatile ULONG *board_regs;
typedef struct { board_regs regs; } Mapping;

static void unmap(Mapping *m)
{
    if (m->regs) MmUnmapIoSpace((PVOID)m->regs, 4096);
}

static int map(Mapping *m, ULONG address)
{
    PHYSICAL_ADDRESS pa;
    pa.QuadPart = address;
    m->regs = MmMapIoSpace(pa, 4096, MmNonCached);
    return m->regs != NULL;
}

static board_word cfg(unsigned offset, unsigned width, board_word value, int write)
{
#ifdef WXP_NT31
    return wxp_pci31_cycle(0, 6 << 3, offset, width, value, write);
#else
    ULONG done = write ? HalSetBusDataByOffset(PCIConfiguration, 0, 6, &value, offset, width) :
                         HalGetBusDataByOffset(PCIConfiguration, 0, 6, &value, offset, width);
    return done == width ? value : 0xffffffffUL;
#endif
}

static void emit(const char *text)
{
    while (*text) __asm__ volatile("outb %0,$0xe9" : : "a"((UCHAR)*text++));
}

static void board_delay(void)
{
    KeStallExecutionProcessor(10);
}
#endif

#ifdef WXP_BOARD16
/* A 16-bit C volatile DWORD can compile to two bus transfers. These GPU
   registers require a single 32-bit transaction even in enhanced mode. */
static board_word board_read(board_regs pointer);
#pragma aux board_read = "mov eax,dword ptr es:[di]" "mov edx,eax" "shr edx,16" \
    parm [es di] value [dx ax];
static void board_write(board_regs pointer, board_word value);
#pragma aux board_write = "movzx eax,ax" "movzx edx,dx" "shl edx,16" "or eax,edx" \
    "mov dword ptr es:[di],eax" parm [es di] [dx ax] modify [ax dx];
#else
#define board_read(p) (*(p))
#define board_write(p, v) (*(p) = (v))
#endif
#include "board_ops.h"
#include "board_aux.h"

static board_word saved_page[256];

static void number(board_word value)
{
    char text[9];
    unsigned i;
    for (i = 0; i < 8; ++i) {
        text[7-i] = "0123456789abcdef"[value & 15];
        value >>= 4;
    }
    text[8] = 0;
    emit(text);
}

static int run(void)
{
    Mapping mem[6] = {{0}};
    board_word id = cfg(0, 4, 0, 0), ctl = cfg(4, 2, 0, 0), fb, regs, high;
    unsigned i;
    int nv = (uint16_t)id == 0x10de, ok = 0;
    emit("WINXPDM vendor PCI MMIO id="); number(id); emit("\n");
    if (!nv && (uint16_t)id != 0x1002) return 0;
    fb = cfg(nv ? 0x14 : 0x10, 4, 0, 0) & ~15UL;
    high = cfg(nv ? 0x18 : 0x14, 4, 0, 0);
    regs = cfg(nv ? 0x10 : 0x24, 4, 0, 0) & ~15UL;
    if (high || !fb || !regs || fb > 0xffbfffffUL || regs > 0xff8fffffUL) {
        emit("32-bit firmware BAR configuration unavailable\n");
        return 0;
    }
    emit("BAR registers="); number(regs); emit(" framebuffer="); number(fb); emit("\n");
    if (!(ctl & 2)) cfg(4, 2, ctl | 2, 1);
    if (nv) {
        if (!map(&mem[0], regs + 0x1000) || !map(&mem[1], regs + 0x700000) ||
            !map(&mem[2], fb + 0x100000) || !map(&mem[3], regs + 0x610000)) goto done;
        emit("GeForce windows mapped\n");
        ok = board_pramin(mem[0].regs + 0x700 / 4, mem[1].regs, mem[2].regs, mem[3].regs + 6);
        if (ok) emit("GeForce PRAMIN, BAR1 bidirectional readback and display counter PASS\n");
        if (ok && (id == 0x13c010deUL || id == 0x13c210deUL)) {
            ok = map(&mem[5], regs + 0x10a000UL) && board_falcon(mem[5].regs);
            if (ok) emit("GM204 Falcon: two inputs returned input plus seven, EXIT and page release PASS\n");
        }
    } else {
        if (!map(&mem[0], regs + 0x4000) || !map(&mem[1], fb + 0x400000)) goto done;
        emit("Radeon windows mapped\n");
        ok = board_sdma(mem[0].regs + 0xb80 / 4, mem[1].regs, saved_page);
        if (ok) emit("Radeon SDMA7 circular queue, fill, unaligned copy, 64-bit fence and pointer writeback PASS\n");
    }
    if (ok && map(&mem[4], regs + (nv ? 0xd000UL : 0x12000UL))) {
        ok = board_two_sinks(mem[4].regs + (nv ? 0x930/4 : 0xdc8/4), nv);
        if (ok) emit("Native AUX: two DPCD caps, four EDID blocks, two timings and absent sink PASS\n");
    } else ok = 0;
done:
    for (i = 0; i < sizeof(mem) / sizeof(*mem); ++i) unmap(&mem[i]);
    if (!(ctl & 2)) cfg(4, 2, ctl, 1);
    return ok;
}

#ifdef WXP_BOARD16
int PASCAL WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR command, int show)
{
    int ok;
    (void)instance; (void)prev; (void)command; (void)show;
    ok = run();
    emit(ok ? "Vendor board test PASS\n" : "Vendor board test FAIL\n");
    return !ok;
}
#elif defined(WXP_BOARD_VXD)
void wxp_board_device_init(void)
{
    emit("WINXPDM VMM Device_Init\n");
    emit(run() ? "Vendor board test PASS\n" : "Vendor board test FAIL\n");
}
#else
NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    int ok;
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(path);
    ok = run();
    emit(ok ? "Vendor board test PASS\n" : "Vendor board test FAIL\n");
    return ok ? STATUS_SUCCESS : STATUS_NO_SUCH_DEVICE;
}
#endif
