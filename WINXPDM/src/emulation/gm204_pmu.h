/* GM204 Falcon v5 ordinary instruction SRAM and processor control.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef WINXPDM_GM204_PMU_H
#define WINXPDM_GM204_PMU_H

#define GM204_PMU_BASE 0x10a000
#define GM204_PMU_DMEM_SIZE 0x6000
/* The stack pointer implements only the bits needed to span local data RAM. */
#define GM204_STACK_MASK 0x7ffc
#define GM204_IMEM_SIZE 0x8000
#define GM204_IMEM_PAGE 256
#define GM204_IMEM_PAGES (GM204_IMEM_SIZE / GM204_IMEM_PAGE)
#define GM204_VIRTUAL_PAGES 512

enum { FALCON_PAGE_EMPTY = 0, FALCON_PAGE_VALID = 1, FALCON_PAGE_BUSY = 2, FALCON_PAGE_PROTECTED = 4 };
enum { FALCON_READY, FALCON_FETCH_UNAVAILABLE, FALCON_UNSUPPORTED_INSN, FALCON_UNSUPPORTED_IO };

typedef struct {
    uint8_t code[GM204_IMEM_SIZE];
    uint16_t tag[GM204_IMEM_PAGES];
    uint8_t state[GM204_IMEM_PAGES];
    uint32_t index, next_tag, entry, pc, regs[16], irq, mailbox[2];
    uint32_t blocked, opcode, tlb_command, tlb_result;
    uint32_t sp, xcbase, xdbase, xtargets, dma_config[8];
    uint32_t last_dma_target;
    uint64_t last_dma_address;
    bool carry, overflow, negative, zero, running;
} Gm204Falcon;

typedef struct {
    uint64_t writes, pages, starts, instructions, exits, code_loads, data_loads, data_stores;
} Gm204FalconStats;

typedef bool (*Gm204MemoryTransfer)(void *opaque, unsigned target, uint64_t address,
                                    uint8_t *bytes, unsigned size, bool write);

static void gm204_code_write(Gm204Falcon *f, Gm204FalconStats *stats, uint32_t word)
{
    uint32_t addr = f->index & 0xfffc;
    unsigned page = addr / GM204_IMEM_PAGE;
    if (addr < sizeof(f->code)) {
        if (f->index & (1U << 28)) {
            /* Protected uploads have no ordinary instruction view in this model. */
            if (!(addr & 255)) {
                f->tag[page] = f->next_tag;
            }
            f->state[page] = FALCON_PAGE_PROTECTED;
        } else if (f->state[page] != FALCON_PAGE_PROTECTED) {
            if ((addr & 255) == 0) {
                f->tag[page] = f->next_tag;
                f->state[page] = FALCON_PAGE_BUSY;
            }
            stl_le_p(f->code + addr, word);
            stats->writes++;
            if ((addr & 255) == 252 && f->state[page] == FALCON_PAGE_BUSY) {
                f->state[page] = FALCON_PAGE_VALID;
                stats->pages++;
            }
        }
    }
    if (f->index & ((1U << 24) | (1U << 28))) {
        f->index = (f->index & ~0xfffcU) | ((addr + 4) & 0xfffc);
    }
}

static uint32_t gm204_code_read(Gm204Falcon *f)
{
    uint32_t addr = f->index & 0xfffc, word = 0;
    if (addr < sizeof(f->code)) {
        word = f->state[addr / GM204_IMEM_PAGE] == FALCON_PAGE_PROTECTED ?
               0xdead5ec1 : ldl_le_p(f->code + addr);
    }
    if (f->index & (1U << 25)) {
        f->index = (f->index & ~0xfffcU) | ((addr + 4) & 0xfffc);
    }
    return word;
}

/* Fetch through the virtual page tags used by the actual instruction port. */
static uint32_t gm204_virtual_tlb(const Gm204Falcon *f, uint32_t pc)
{
    uint32_t result = 0;
    unsigned hits = 0;
    for (unsigned i = 0; i < GM204_IMEM_PAGES; i++) {
        if (f->state[i] != FALCON_PAGE_EMPTY && f->tag[i] == pc / GM204_IMEM_PAGE) {
            hits++;
            result = (result & ~0xffU) | ((uint32_t)f->state[i] << 24) | i;
        }
    }
    if (!hits) return 1U << 31;
    return hits > 1 ? result | (1U << 30) : result;
}

static void gm204_tlb_command(Gm204Falcon *f, uint32_t value)
{
    unsigned param = value & 0xffffff;
    f->tlb_command = value;
    switch (value >> 24) {
    case 1:
        if (param < GM204_IMEM_PAGES && f->state[param] != FALCON_PAGE_PROTECTED) {
            f->tag[param] = 0;
            f->state[param] = FALCON_PAGE_EMPTY;
        }
        break;
    case 2:
        f->tlb_result = param < GM204_IMEM_PAGES ?
            ((uint32_t)f->state[param] << 24) | (f->tag[param] << 8) : 0;
        break;
    case 3:
        f->tlb_result = gm204_virtual_tlb(f, param % (GM204_VIRTUAL_PAGES * GM204_IMEM_PAGE));
        break;
    }
}

static bool gm204_fetch(const Gm204Falcon *f, uint32_t pc, uint8_t *bytes, unsigned size)
{
    while (size) {
        uint32_t match = gm204_virtual_tlb(f, pc);
        unsigned offset = pc & 255;
        unsigned n = MIN(size, GM204_IMEM_PAGE - offset);
        if (match >> 24 != FALCON_PAGE_VALID) {
            return false;
        }
        memcpy(bytes, f->code + (match & 255) * GM204_IMEM_PAGE + offset, n);
        bytes += n;
        pc += n;
        size -= n;
    }
    return true;
}

/* Lengths of the ordinary v5 instruction forms handled below. */
static unsigned gm204_insn_size(uint8_t op)
{
    switch (op >> 4) {
    case 0x0: return 2;
    case 0x4: return 3;
    case 0x8: return 4;
    case 0xd: return 5;
    }
    switch (op) {
    case 0x90: case 0x94: case 0x95: case 0x98: case 0xb5: case 0xb6:
    case 0xbc: case 0xf4: case 0xfa: case 0xfd: case 0xfe: return 3;
    case 0x3e: case 0x7e: case 0xb3: return 4;
    case 0xb8: return 5;
    default: return 2;
    }
}

static void gm204_flags(Gm204Falcon *f, uint32_t value, bool carry, bool overflow)
{
    f->carry = carry;
    f->overflow = overflow;
    f->negative = (int32_t)value < 0;
    f->zero = value == 0;
}

static uint32_t gm204_add(Gm204Falcon *f, uint32_t a, uint32_t b)
{
    uint32_t sum = a + b;
    gm204_flags(f, sum, sum < a, (~(a ^ b) & (a ^ sum)) >> 31);
    return sum;
}

static uint32_t gm204_shift(Gm204Falcon *f, uint32_t value, unsigned count, bool right)
{
    count &= 31;
    uint32_t result = right ? value >> count : value << count;
    bool carry = count && (value >> (right ? count - 1 : 32 - count) & 1);
    gm204_flags(f, result, carry, false);
    return result;
}

static bool gm204_stack_push(Gm204Falcon *f, uint32_t *dmem, uint32_t value)
{
    uint32_t addr = (f->sp - 4) & GM204_STACK_MASK;
    if (addr >= GM204_PMU_DMEM_SIZE) {
        return false;
    }
    dmem[addr / 4] = value;
    f->sp = addr;
    return true;
}

static bool gm204_stack_pop(Gm204Falcon *f, const uint32_t *dmem, uint32_t *value)
{
    if (f->sp >= GM204_PMU_DMEM_SIZE) {
        return false;
    }
    *value = dmem[f->sp / 4];
    f->sp = (f->sp + 4) & GM204_STACK_MASK;
    return true;
}

static bool gm204_move_special(Gm204Falcon *f, unsigned reg, uint32_t *value, bool write)
{
    uint32_t *slot;
    switch (reg) {
    case 4: slot = &f->sp; break;
    case 5:
        if (write) return false;
        *value = f->pc;
        return true;
    case 6: slot = &f->xcbase; break;
    case 7: slot = &f->xdbase; break;
    case 11: slot = &f->xtargets; break;
    default: return false;
    }
    if (write) {
        *slot = reg == 4 ? *value & GM204_STACK_MASK : *value;
    } else {
        *value = *slot;
    }
    return true;
}

/* An ordinary XCLD always copies one page. Data transfer sizes are 4..256 bytes.
 * Physical local and PCI RAM targets are selected by the board's FBIF registers. */
static bool gm204_transfer(Gm204Falcon *f, Gm204FalconStats *stats, uint32_t *dmem,
                           unsigned op, uint32_t external_offset, uint32_t descriptor,
                           Gm204MemoryTransfer transfer, void *opaque)
{
    bool code = op == 4, store = op == 6;
    unsigned local = descriptor & 0xffff;
    unsigned shift = code ? 6 : descriptor >> 16;
    unsigned port = (f->xtargets >> (code ? 0 : store ? 12 : 8)) & 7;
    uint64_t address = ((uint64_t)(code ? f->xcbase : f->xdbase) << 8) + external_offset;
    uint8_t bytes[GM204_IMEM_PAGE];
    f->last_dma_target = f->dma_config[port];
    f->last_dma_address = address;
    if (shift > 6) return false;
    unsigned size = 4U << shift;
    unsigned page = local / GM204_IMEM_PAGE;
    if ((local | address) & (size - 1)) return false;
    if (code) {
        if (page >= GM204_IMEM_PAGES || f->state[page] == FALCON_PAGE_PROTECTED ||
            external_offset / GM204_IMEM_PAGE >= GM204_VIRTUAL_PAGES) return false;
    } else if (local > GM204_PMU_DMEM_SIZE - size) {
        return false;
    }
    if (store) {
        for (unsigned i = 0; i < size; i += 4) stl_le_p(bytes + i, dmem[(local + i) / 4]);
    }
    if (!transfer(opaque, f->last_dma_target, address, bytes, size, store)) return false;
    if (code) {
        memcpy(f->code + local, bytes, size);
        f->tag[page] = external_offset / GM204_IMEM_PAGE;
        f->state[page] = FALCON_PAGE_VALID;
        stats->code_loads++;
    } else if (store) {
        stats->data_stores++;
    } else {
        for (unsigned i = 0; i < size; i += 4) dmem[(local + i) / 4] = ldl_le_p(bytes + i);
        stats->data_loads++;
    }
    return true;
}

static unsigned gm204_step(Gm204Falcon *f, Gm204FalconStats *stats, uint32_t *dmem,
                           Gm204MemoryTransfer transfer, void *opaque)
{
    uint8_t b[6] = {0};
    if (!gm204_fetch(f, f->pc, b, 2)) {
        return FALCON_FETCH_UNAVAILABLE;
    }
    unsigned op = b[0], size = gm204_insn_size(op);
    unsigned r1 = b[1] >> 4, r2 = b[1] & 15;
    uint32_t next = f->pc + size;
    if (size > 2 && !gm204_fetch(f, f->pc + 2, b + 2, size - 2)) {
        return FALCON_FETCH_UNAVAILABLE;
    }
    f->opcode = op;
    uint32_t a = f->regs[r1], v = f->regs[r2];
    switch (op >> 4) {
    case 0: case 4: case 8: case 13:
        f->regs[op & 15] = sextract32(ldl_le_p(b + 1), 0, (size - 1) * 8);
        f->pc = next;
        return FALCON_READY;
    }
    switch (op) {
    case 0x3e: case 0x7e: /* absolute branch and call */
        if (op == 0x7e && !gm204_stack_push(f, dmem, next)) {
            return FALCON_UNSUPPORTED_IO;
        }
        next = ldl_le_p(b + 1) & 0xffffff;
        break;
    case 0x90: case 0xb8:
        if (op == 0xb8 && b[4] != 0) return FALCON_UNSUPPORTED_INSN;
        f->regs[r2] = gm204_add(f, a, op == 0x90 ? b[2] : lduw_le_p(b + 2));
        break;
    case 0xbc:
        if ((b[2] & 15) != 0) return FALCON_UNSUPPORTED_INSN;
        f->regs[b[2] >> 4] = gm204_add(f, a, v);
        break;
    case 0x94: case 0x95:
        f->regs[r2] = gm204_shift(f, a, b[2], op == 0x95);
        break;
    case 0xb6:
        if (r2 != 4 && r2 != 5) return FALCON_UNSUPPORTED_INSN;
        f->regs[r1] = gm204_shift(f, a, b[2], r2 == 5);
        break;
    case 0x98: case 0xa0: case 0xb5: case 0xbf: {
        uint32_t addr = a + 4U * b[2];
        if (addr >= GM204_PMU_DMEM_SIZE) return FALCON_UNSUPPORTED_IO;
        if (op == 0x98 || op == 0xbf) {
            f->regs[r2] = dmem[addr / 4];
        } else {
            /* An unaligned dword store keeps only its low byte or halfword. */
            if (addr & 1) v = (v & 0xff) << (8 * (addr & 3));
            else if (addr & 2) v <<= 16;
            dmem[addr / 4] = v;
        }
        break;
    }
    case 0xa6:
        gm204_flags(f, a - v, a < v, ((a ^ v) & (a ^ (a - v))) >> 31);
        break;
    case 0xb2:
        f->regs[r2] = a;
        break;
    case 0xb3:
        if (r2 != 0 && r2 != 4) return FALCON_UNSUPPORTED_INSN;
        if ((a == b[2]) == (r2 == 0)) next = f->pc + (int8_t)b[3];
        break;
    case 0xbd:
        if (r2 != 4) return FALCON_UNSUPPORTED_INSN;
        f->regs[r1] = 0;
        break;
    case 0xf4: {
        bool take;
        switch (b[1]) {
        case 0x08: take = f->carry; break;
        case 0x0b: take = f->zero; break;
        case 0x0e: take = true; break;
        case 0x18: take = !f->carry; break;
        case 0x1b: take = !f->zero; break;
        default: return FALCON_UNSUPPORTED_INSN;
        }
        if (take) next = f->pc + (int8_t)b[2];
        break;
    }
    case 0xf8:
        if (b[1] == 0) {
            if (!gm204_stack_pop(f, dmem, &next)) return FALCON_UNSUPPORTED_IO;
        } else if (b[1] == 2) {
            f->running = false;
            f->irq |= 1U << 4;
            stats->exits++;
        } else if (b[1] != 3 && b[1] != 7) {
            return FALCON_UNSUPPORTED_INSN;
        }
        /* A wait can retire when the transfer queues are empty. */
        break;
    case 0xf9:
        /* Single push and the r0-only encoding of mpush. */
        if (r2 != 0 && b[1] != 2) return FALCON_UNSUPPORTED_INSN;
        if (!gm204_stack_push(f, dmem, a)) return FALCON_UNSUPPORTED_IO;
        break;
    case 0xfa:
        if (b[2] < 4 || b[2] > 6) return FALCON_UNSUPPORTED_INSN;
        if (!gm204_transfer(f, stats, dmem, b[2], a, v, transfer, opaque)) {
            return FALCON_UNSUPPORTED_IO;
        }
        break;
    case 0xfc:
        if (r2 != 0) return FALCON_UNSUPPORTED_INSN;
        if (!gm204_stack_pop(f, dmem, &f->regs[r1])) return FALCON_UNSUPPORTED_IO;
        break;
    case 0xfd:
        switch (b[2]) {
        case 4: a &= v; break;
        case 5: a |= v; break;
        case 6: a ^= v; break;
        default: return FALCON_UNSUPPORTED_INSN;
        }
        f->regs[r1] = a;
        gm204_flags(f, a, false, false);
        break;
    case 0xfe:
        if (b[2] == 0 && gm204_move_special(f, r2, &f->regs[r1], true)) break;
        if (b[2] == 1 && gm204_move_special(f, r1, &f->regs[r2], false)) break;
        return FALCON_UNSUPPORTED_INSN;
    default:
        return FALCON_UNSUPPORTED_INSN;
    }
    f->pc = next;
    return FALCON_READY;
}

/* A blocked instruction keeps CPUCTL running. Only an executed EXIT halts the core. */
static void gm204_execute(Gm204Falcon *f, Gm204FalconStats *stats, uint32_t *dmem,
                          Gm204MemoryTransfer transfer, void *opaque)
{
    for (unsigned budget = 0; f->running && !f->blocked && budget < 64; ++budget) {
        f->blocked = gm204_step(f, stats, dmem, transfer, opaque);
        if (!f->blocked) stats->instructions++;
    }
}
#endif
