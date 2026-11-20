#include "patch.h"

typedef struct {
    const byte *p;
    u32 size, nt, opt, sect, nsect, ex, exlen, names, ords, funcs, nn, nf;
} Image;
static word w(const byte *p) {
    return (word)(p[0] | (unsigned)p[1] << 8);
}
static u32 d(const byte *p) {
    return w(p) | (u32)w(p + 2) << 16;
}
static void putw(byte *p, u32 v) {
    p[0] = (byte)v;
    p[1] = (byte)(v >> 8);
}
static void put(byte *p, u32 v) {
    putw(p, v);
    putw(p + 2, v >> 16);
}
static int fits(u32 start, u32 n, u32 limit) {
    return start <= limit && n <= limit - start;
}
static u32 maximum(u32 a, u32 b) {
    return a > b ? a : b;
}
static u32 rva(const Image *im, u32 addr, u32 length) {
    u32 i;
    for (i = 0; i < im->nsect; i++) {
        const byte *s = im->p + im->sect + 40 * i;
        u32 va = d(s + 12), raw = d(s + 20), size = d(s + 16);
        if (addr >= va && fits(addr - va, length, size) && fits(raw, size, im->size))
            return raw + (addr - va);
    }
    return 0;
}
static const char *nameat(const Image *im, u32 addr) {
    u32 off = rva(im, addr, 1), n;
    if (!off)
        return 0;
    for (n = 0; n < 128 && fits(off, n + 1, im->size); n++)
        if (!im->p[off + n] && rva(im, addr, n + 1) == off)
            return (const char *)im->p + off;
    return 0;
}
static int headers(Image *im, const byte *p, u32 size, u32 limit) {
    u32 nt, opt, table, nsect;
    if (size < 256 || size > limit || w(p) != 0x5a4d)
        return 0;
    nt = d(p + 60);
    if (!fits(nt, 24 + 224, size) || d(p + nt) != 0x4550 || w(p + nt + 4) != 0x14c)
        return 0;
    opt = nt + 24;
    nsect = w(p + nt + 6);
    table = opt + w(p + nt + 20);
    if (w(p + nt + 20) < 224 || !nsect || nsect > 64 || !fits(table, 40 * nsect, size) ||
        w(p + opt) != 0x10b || w(p + opt + 68) != 1 || d(p + opt + 92) < 5)
        return 0;
    im->p = p;
    im->size = size;
    im->nt = nt;
    im->opt = opt;
    im->sect = table;
    im->nsect = nsect;
    return 1;
}
static int parse(Image *im, const byte *p, u32 size) {
    u32 i, x;
    const char *dll;
    if (!headers(im, p, size, MAX_PORT) || d(p + im->opt + 128) || d(p + im->opt + 132))
        return 0;
    im->ex = d(p + im->opt + 96);
    im->exlen = d(p + im->opt + 100);
    x = rva(im, im->ex, 40);
    if (!x || im->exlen < 40)
        return 0;
    dll = nameat(im, d(p + x + 12));
    if (!dll || (strcmp(dll, "SCSIPORT.SYS") && strcmp(dll, "scsiport.sys")))
        return 0;
    im->nf = d(p + x + 20);
    im->nn = d(p + x + 24);
    if (!im->nf || im->nf > 256 || !im->nn || im->nn > 256)
        return 0;
    im->funcs = rva(im, d(p + x + 28), 4 * im->nf);
    im->names = rva(im, d(p + x + 32), 4 * im->nn);
    im->ords = rva(im, d(p + x + 36), 2 * im->nn);
    if (!im->funcs || !im->names || !im->ords)
        return 0;
    for (i = 0; i < im->nn; i++)
        if (!nameat(im, d(p + im->names + 4 * i)) || w(p + im->ords + 2 * i) >= im->nf)
            return 0;
    for (i = 0; i < im->nf; i++) {
        u32 target = d(p + im->funcs + 4 * i);
        if (target && (!rva(im, target, 1) || (target >= im->ex && target - im->ex < im->exlen)))
            return 0;
    }
    return 1;
}

/* Inspect the image that the service manager will actually load. */
const char *port_imports(const byte *port, u32 port_size, const byte *image, u32 image_size) {
    Image system, mini;
    u32 imports, desc, lookup, iat, i, j;
    const char *module, *name;
    int initializes = 0;
    if (!parse(&system, port, port_size))
        return "Cannot read exports from the installed x86 SCSI port.";
    if (!headers(&mini, image, image_size, MAX_PORT + EXTRA))
        return "Select an x86 native SCSI miniport image.";
    imports = d(image + mini.opt + 104);
    desc = rva(&mini, imports, 40);
    if (!desc || d(image + mini.opt + 108) < 40)
        return "Invalid miniport import directory.";
    module = nameat(&mini, d(image + desc + 12));
    if (!module || strlen(module) != 12)
        return "The miniport must use SCSIPORT.SYS.";
    for (i = 0; i < 12; i++) {
        unsigned c = (byte)module[i];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (c != (unsigned)"SCSIPORT.SYS"[i])
            return "The miniport must use SCSIPORT.SYS.";
    }
    u32 descriptors = 40;
    if (d(image + desc + 32)) {
        static const char kernel[] = "NTOSKRNL.EXE";
        u32 tab, slot, thunk, ordinal, hint;
        const char *debug;
        descriptors = 60;
        if (d(image + mini.opt + 108) < descriptors || !rva(&mini, imports, descriptors))
            return "Invalid kernel import descriptor.";
        module = nameat(&mini, d(image + desc + 32));
        if (!module || strlen(module) != sizeof(kernel) - 1)
            return "Unsupported kernel module.";
        for (i = 0; i < sizeof(kernel) - 1; ++i) {
            unsigned c = (byte)module[i];
            if (c >= 'a' && c <= 'z')
                c -= 32;
            if (c != (unsigned)kernel[i])
                return "Unsupported kernel module.";
        }
        tab = d(image + desc + 20);
        slot = d(image + desc + 36);
        thunk = rva(&mini, tab ? tab : slot, 8);
        ordinal = rva(&mini, slot, 8);
        if (!thunk || !ordinal || d(image + thunk + 4) || d(image + ordinal + 4))
            return "Unsupported kernel debug import table.";
        hint = d(image + thunk);
        if (hint > 0x7fffffffu || !rva(&mini, hint, 3) ||
            !(debug = nameat(&mini, hint + 2)) || strcmp(debug, "DbgPrint"))
            return "The additional kernel import must be DbgPrint.";
    }
    for (i = descriptors - 20; i < descriptors; ++i)
        if (image[desc + i])
            return "Unsupported additional import descriptor.";
    iat = d(image + desc + 16);
    lookup = d(image + desc);
    if (!lookup)
        lookup = iat;
    for (i = 0; i < 256; i++) {
        u32 off, entry;
        if (lookup > 0xffffffffu - 4 * i || iat > 0xffffffffu - 4 * i ||
            !(off = rva(&mini, lookup + 4 * i, 4)) || !rva(&mini, iat + 4 * i, 4))
            return "Invalid miniport import table.";
        entry = d(image + off);
        if (!entry)
            break;
        if (entry >= 0x80000000u || !rva(&mini, entry, 3) || !(name = nameat(&mini, entry + 2)))
            return "Invalid or ordinal SCSI import.";
        initializes |= !strcmp(name, "ScsiPortInitialize");
        for (j = 0; j < system.nn; j++)
            if (!strcmp(name, nameat(&system, d(port + system.names + 4 * j))))
                break;
        if (j == system.nn || !d(port + system.funcs + 4 * w(port + system.ords + 2 * j)))
            return "This driver needs a SCSI port export unavailable on this installation.";
    }
    if (!initializes || i == 256)
        return "The file does not register a supported SCSI miniport.";
    return 0;
}

/* NT 3.1 exports ScsiPortInitialize from a discardable initialization
   section. The export must stay resident for subsequent miniport starts. */
const char *bridge(const byte *p, u32 size, byte *out, u32 *output_size) {
    Image im;
    u32 i, r = 0, off = 0, sum = 0;
    if (!parse(&im, p, size))
        return "Unrecognized x86 SCSI port image.";
    for (i = 0; i < im.nn; i++) {
        if (!strcmp(nameat(&im, d(p + im.names + 4 * i)), "ScsiPortInitialize")) {
            r = d(p + im.funcs + 4 * w(p + im.ords + 2 * i));
            break;
        }
    }
    if (!r)
        return "The installed port has no ScsiPortInitialize export.";
    for (i = 0; i < im.nsect; i++) {
        u32 s = im.sect + 40 * i, va = d(p + s + 12), length = maximum(d(p + s + 8), d(p + s + 16));
        if (r >= va && r - va < length) {
            off = s;
            break;
        }
    }
    if (!off || !(d(p + off + 36) & 0x20000000u))
        return "The initialization export is outside executable code.";
    if (!memcmp(p + off, ".n3init", 7))
        return "The NT3SCPRT bridge is already installed.";
    memcpy(out, p, size);
    *output_size = size;
    if (!(d(p + off + 36) & 0x02000000u))
        return 0;
    memset(out + off, 0, 8);
    memcpy(out + off, ".n3init", 7);
    put(out + off + 36, d(p + off + 36) & ~0x02000000u);
    put(out + im.opt + 64, 0);
    for (i = 0; i < size; i += 2) {
        sum += out[i] + (i + 1 < size ? (u32)out[i + 1] << 8 : 0);
        sum = (sum & 65535) + (sum >> 16);
    }
    put(out + im.opt + 64, ((sum & 65535) + (sum >> 16)) + size);
    return 0;
}
