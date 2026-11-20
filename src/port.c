/* x86 NT 3.1 SCSI port ABI bridge. Built as position-independent i386 code. */
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef U32(__attribute__((stdcall)) * Find)(void *, void *, void *, char *, void *, U8 *);
#define NT __attribute__((stdcall))
enum { ISA = 1, PCI = 5, PCI_CONFIG = 4, FOUND = 1, NO_ADAPTER = 0 };
typedef struct {
    U32 low, high, length;
    U8 memory, pad[3];
} Range;
typedef struct {
    U32 length, bus, type, irq, vector, mode, transfer, breaks, channel, port;
    U32 width, speed, alignment, count;
    Range *ranges;
    void *reserved;
    U8 buses, initiator[8], scatter, master, cache, down, primary, secondary;
    U8 dma32, demand, map, physical, tagged, sense, multiple, receive, real;
    U8 control, targets, spare[2];
    U32 slot, irq2, vec2, mode2, channel2, port2, width2, speed2;
} Config;
typedef struct {
    U32 size, bus;
    void *hw_init, *start, *interrupt;
    Find find;
    void *reset, *dma, *state;
    U32 devsize, lusize, srbsize, ranges;
    void *reserved;
    U8 map, physical, tagged, sense, multiple, receive;
    U16 vendorlen;
    char *vendor;
    U16 pad, devlen;
    char *device;
} Init;
typedef struct {
    Init *src;
    void *context;
    U32 next;
} Scan;
_Static_assert(sizeof(Init) == 76 && sizeof(Config) == 124 && sizeof(Range) == 16, "x86 SCSI ABI");
extern U32 NT base_init(void *, void *, Init *, void *);
extern U32 NT base_read(void *, U32, U32, U32, void *, U32);
extern U32 NT base_write(void *, U32, U32, U32, void *, U32, U32);
extern void *NT base_map(void *, U32, U32, unsigned long long, U32, U8);
extern void *NT base_uncached(void *, Config *, U32);

static void copy(void *dst, const void *src, U32 n) {
    U8 *d = dst;
    const U8 *s = src;
    while (n--)
        *d++ = *s++;
}

/* Type 1 configuration uses a shared address latch. Preserve it across
   transactions and serialize CPUs as well as interrupt handlers. */
static volatile U32 pci_lock;
static inline U32 enter(void) {
    U32 flags, busy;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags)::"memory");
    do {
        busy = 1;
        __asm__ volatile("xchgl %0,%1" : "+r"(busy), "+m"(pci_lock)::"memory");
    } while (busy);
    return flags;
}
static inline void leave(U32 flags) {
    __asm__ volatile("" ::: "memory");
    pci_lock = 0;
    __asm__ volatile("pushl %0; popfl" ::"r"(flags) : "memory", "cc");
}
static inline U32 in32(U16 port) {
    U32 v;
    __asm__ volatile("inl %w1,%0" : "=a"(v) : "d"(port));
    return v;
}
static inline void out32(U16 port, U32 v) {
    __asm__ volatile("outl %0,%w1" ::"a"(v), "d"(port));
}
static inline U16 in16(U16 port) {
    U16 v;
    __asm__ volatile("inw %w1,%0" : "=a"(v) : "d"(port));
    return v;
}
static inline void out16(U16 port, U16 v) {
    __asm__ volatile("outw %0,%w1" ::"a"(v), "d"(port));
}
static inline U8 in8(U16 port) {
    U8 v;
    __asm__ volatile("inb %w1,%0" : "=a"(v) : "d"(port));
    return v;
}
static inline void out8(U16 port, U8 v) {
    __asm__ volatile("outb %0,%w1" ::"a"(v), "d"(port));
}
static U32 address(U32 bus, U32 slot) {
    return 0x80000000u | bus << 16 | (slot & 31) << 11 | (slot >> 5) << 8;
}
static U32 config_io(U32 bus, U32 slot, void *buffer, U32 off, U32 len, int write) {
    U8 *p = buffer;
    U32 i = 0, base, flags, saved, id;
    if (bus > 255 || slot > 255 || off >= 256 || !buffer || !len)
        return 0;
    if (len > 256 - off)
        len = 256 - off;
    base = address(bus, slot);
    flags = enter();
    saved = in32(0xcf8);
    out32(0xcf8, base);
    id = in32(0xcfc);
    if (in32(0xcf8) != base || (id & 0xffff) == 0xffff || !(id & 0xffff)) {
        if (!write && !off && len >= 2) {
            p[0] = p[1] = 255;
            i = 2;
        }
    } else
        while (i < len) {
            U32 offset = off + i, remaining = len - i;
            U16 data = (U16)(0xcfc + (offset & 3));
            out32(0xcf8, base + (offset & ~3u));
            if (!(offset & 3) && remaining >= 4) {
                if (write)
                    out32(data, *(U32 *)(p + i));
                else
                    *(U32 *)(p + i) = in32(data);
                i += 4;
            } else if (!(offset & 1) && remaining >= 2) {
                if (write)
                    out16(data, *(U16 *)(p + i));
                else
                    *(U16 *)(p + i) = in16(data);
                i += 2;
            } else {
                if (write)
                    out8(data, p[i]);
                else
                    p[i] = in8(data);
                ++i;
            }
        }
    out32(0xcf8, saved);
    leave(flags);
    return i;
}
U32 NT port_read(void *dev, U32 kind, U32 bus, U32 slot, void *p, U32 len) {
    if (kind == PCI_CONFIG)
        return config_io(bus, slot, p, 0, len, 0);
    return base_read(dev, kind, bus, slot, p, len);
}
U32 NT port_write(void *dev, U32 kind, U32 bus, U32 slot, void *p, U32 off, U32 len) {
    (void)dev;
    return kind == PCI_CONFIG ? config_io(bus, slot, p, off, len, 1) : 0;
}
U8 NT port_validate(void *ext, U32 type, U32 bus, unsigned long long start, U32 length, U8 memory) {
    (void)ext;
    (void)type;
    (void)bus;
    (void)start;
    (void)length;
    (void)memory;
    /* This advisory routine returns TRUE in the NT 3.51 x86 port. */
    return 1;
}
void *NT port_map(void *dev, U32 type, U32 bus, unsigned long long start, U32 len, U8 io) {
    return base_map(dev, type == PCI ? ISA : type, type == PCI ? 0 : bus, start, len, io);
}
void *NT port_uncached(void *dev, Config *cfg, U32 n) {
    Config local;
    if (cfg->type != PCI)
        return base_uncached(dev, cfg, n);
    if (cfg->length < 64 || cfg->length > sizeof(local))
        return (void *)0;
    local = (Config){0};
    copy(&local, cfg, cfg->length);
    local.type = ISA;
    local.bus = 0;
    return base_uncached(dev, &local, n);
}
static int hexid(const char *s, U32 n, U16 id) {
    U32 v = 0, i;
    if (n != 4 || !s)
        return 0;
    for (i = 0; i < 4; i++) {
        U32 c = (U8)s[i];
        if (c >= '0' && c <= '9')
            c -= '0';
        else {
            c |= 32;
            if (c < 'a' || c > 'f')
                return 0;
            c = c - 'a' + 10;
        }
        v = v * 16 + c;
    }
    return v == id;
}
/* Size assigned endpoint BARs with decode disabled. Restore every probed BAR
   and the original command register before calling the miniport. */
static void resources(Config *c, const U8 *header) {
    U32 i, j = 0, mask, probe = ~0u, upper, flags, saved, base = address(c->bus, c->slot);
    U16 command;
    c->irq = c->vector = header[0x3c];
    c->mode = 0;
    flags = enter();
    saved = in32(0xcf8);
    out32(0xcf8, base + 4);
    command = in16(0xcfc);
    out16(0xcfc, command & ~3);
    for (i = 0; i < 6 && j < c->count; i++) {
        U32 pos = 16 + 4 * i, bar = *(const U32 *)(header + pos);
        int io = bar & 1, wide = !io && (bar & 6) == 4;
        if (wide && i == 5)
            break;
        upper = wide ? *(const U32 *)(header + pos + 4) : 0;
        if (!bar || upper) {
            if (wide)
                ++i;
            continue;
        }
        out32(0xcf8, base + pos);
        out32(0xcfc, probe);
        mask = in32(0xcfc);
        out32(0xcfc, bar);
        mask &= io ? ~3u : ~15u;
        if (mask) {
            c->ranges[j].low = bar & (io ? ~3u : ~15u);
            c->ranges[j].high = 0;
            c->ranges[j].length = 0u - mask;
            c->ranges[j].memory = !io;
            ++j;
        }
        if (wide)
            ++i;
    }
    out32(0xcf8, base + 4);
    out16(0xcfc, command);
    out32(0xcf8, saved);
    leave(flags);
}
static U32 NT find_adapter(void *dev, void *arg, void *specific, char *params, void *raw,
                           U8 *again) {
    Scan *scan = arg;
    Init *s = scan->src;
    Config *old = raw, c = {0};
    U32 n = old->length, rc;
    int pci = s->bus == PCI, match = pci && (s->vendorlen || s->devlen);
    if (n < 64 || n > sizeof(c))
        return NO_ADAPTER;
    copy(&c, old, n);
    c.length = sizeof(c);
    c.targets = 8;
    if (pci) {
        c.type = PCI;
        c.bus = 0;
    }
    do {
        if (match) {
            U8 hdr[64];
            U32 slot;
            /* PCI BIOS bus zero is available on the supported legacy PC model. */
            for (slot = scan->next; slot < 256; slot++) {
                if (config_io(0, slot, hdr, 0, sizeof(hdr), 0) != sizeof(hdr))
                    continue;
                if ((hdr[14] & 127) != 0)
                    continue;
                if (s->vendorlen && !hexid(s->vendor, s->vendorlen, *(U16 *)hdr))
                    continue;
                if (s->devlen && !hexid(s->device, s->devlen, *(U16 *)(hdr + 2)))
                    continue;
                c.slot = slot;
                resources(&c, hdr);
                break;
            }
            scan->next = slot + 1;
            if (slot == 256) {
                *again = 0;
                return NO_ADAPTER;
            }
        }
        rc = s->find(dev, scan->context, specific, params, &c, again);
    } while (match && rc == NO_ADAPTER);
    if (match && rc == FOUND) {
        U16 command;
        if (c.master && config_io(c.bus, c.slot, &command, 4, 2, 0) == 2 && !(command & 4)) {
            command |= 4;
            config_io(c.bus, c.slot, &command, 4, 2, 1);
        }
        *again = scan->next < 256;
    }
    c.length = n;
    if (pci) {
        c.type = old->type;
        c.bus = old->bus;
    }
    copy(old, &c, n);
    return rc;
}
U32 NT port_init(void *driver, void *path, Init *src, void *context) {
    Init local;
    Scan scan;
    if (!src || src->size != sizeof(Init))
        return base_init(driver, path, src, context);
    local = *src;
    scan.src = src;
    scan.context = context;
    scan.next = 0;
    local.size = 64;
    local.vendorlen = 0;
    local.find = find_adapter;
    if (src->bus == PCI)
        local.bus = ISA;
    return base_init(driver, path, &local, &scan);
}

/* NT 3.50 supplies the PCI resources. Enable bus mastering after the
   miniport claims its matching storage endpoint and requests master DMA. */
static U32 NT find35(void *dev, void *arg, void *specific, char *params, void *raw, U8 *again) {
    Scan *s = arg;
    Config *c = raw;
    U32 rc = s->src->find(dev, s->context, specific, params, c, again);
    U8 header[16];
    U16 command;
    if (rc == FOUND && c->length >= 96 && c->master && c->type == PCI &&
        base_read(dev, PCI_CONFIG, c->bus, c->slot, header, sizeof(header)) == sizeof(header) &&
        header[11] == 1 && hexid(s->src->vendor, s->src->vendorlen, *(U16 *)header) &&
        (!s->src->devlen || hexid(s->src->device, s->src->devlen, *(U16 *)(header + 2)))) {
        command = *(U16 *)(header + 4);
        if (!(command & 4)) {
            command |= 4;
            base_write(dev, PCI_CONFIG, c->bus, c->slot, &command, 4, 2);
        }
    }
    return rc;
}
U32 NT port_init35(void *driver, void *path, Init *src, void *context) {
    Init local;
    Scan scan;
    if (!src || src->size != sizeof(Init) || src->bus != PCI || !src->vendorlen)
        return base_init(driver, path, src, context);
    local = *src;
    scan.src = src;
    scan.context = context;
    scan.next = 0;
    local.find = find35;
    return base_init(driver, path, &local, &scan);
}
