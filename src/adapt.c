#include "adapt.h"
#include "blob.h"

#define MAX_API 128
#define BIAS 65536u
#define PAGE 4096u
#define HDR 1024u
typedef struct {
    const char *name;
    u32 offset;
} Symbol;
static const Symbol bridge31[] = {{"ScsiPortInitialize", OFF_port_init},
                                  {"ScsiPortGetBusData", OFF_port_read},
                                  {"ScsiPortGetDeviceBase", OFF_port_map},
                                  {"ScsiPortGetUncachedExtension", OFF_port_uncached},
                                  {"ScsiPortSetBusDataByOffset", OFF_port_write},
                                  {"ScsiPortValidateRange", OFF_port_validate}};
static const Symbol native[] = {{"ScsiPortInitialize", OFF_base_init},
                                {"ScsiPortGetBusData", OFF_base_read},
                                {"ScsiPortGetDeviceBase", OFF_base_map},
                                {"ScsiPortGetUncachedExtension", OFF_base_uncached},
                                {"ScsiPortSetBusDataByOffset", OFF_base_write}};
static u32 rd(const byte *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static word rw(const byte *p) { return (word)(p[0] | (u32)p[1] << 8); }
static void wr(byte *p, u32 n) {
    p[0] = (byte)n;
    p[1] = (byte)(n >> 8);
    p[2] = (byte)(n >> 16);
    p[3] = (byte)(n >> 24);
}
static void ww(byte *p, u32 n) {
    p[0] = (byte)n;
    p[1] = (byte)(n >> 8);
}
static u32 roundto(u32 n, u32 m) { return (n + m - 1) & ~(m - 1); }
static int has(u32 off, u32 len, u32 size) { return off <= size && len <= size - off; }
static int strbound(const byte *p, u32 n) {
    u32 i;
    for (i = 0; i < n && i < 120; i++)
        if (!p[i])
            return i != 0;
    return 0;
}
static u32 checksum(const byte *p, u32 n) {
    u32 i, sum = 0;
    for (i = 0; i < n; i += 2) {
        sum += p[i] + (i + 1 < n ? (u32)p[i + 1] << 8 : 0);
        sum = (sum & 65535) + (sum >> 16);
    }
    return ((sum & 65535) + (sum >> 16)) + n;
}
static void section(byte *p, const char *name, u32 vs, u32 va, u32 size, u32 pos, u32 flags) {
    memcpy(p, name, strlen(name));
    wr(p + 8, vs);
    wr(p + 12, va);
    wr(p + 16, size);
    wr(p + 20, pos);
    wr(p + 36, flags);
}
static int indexof(const char **names, u32 n, const char *s) {
    u32 i;
    for (i = 0; i < n; i++)
        if (!strcmp(names[i], s))
            return (int)i;
    return -1;
}
static int localapi(unsigned ver, const char *name, u32 *offset) {
    u32 i;
    if (ver == 50 && !strcmp(name, "ScsiPortInitialize")) {
        *offset = OFF_port_init35;
        return 1;
    }
    if (ver == 10)
        for (i = 0; i < sizeof(bridge31) / sizeof(*bridge31); i++)
            if (!strcmp(name, bridge31[i].name)) {
                *offset = bridge31[i].offset;
                return 1;
            }
    return 0;
}
const char *adapter(const byte *p, u32 n, unsigned ver, byte *out, u32 *used) {
    const char *imports[MAX_API], *link[MAX_API + 5], *name;
    u32 mapped[MAX_API], islocal[MAX_API];
    const byte *o, *s, *imp;
    byte *md, *h, *cs, *entry;
    u32 nt, sh, i, j, ns, img, entry0, impva, impbytes;
    u32 iat0, lookup, va, sz, raw, off, rl, rlsize, count = 0, nlink = 0, codeva, codepos, datava,
                                                    datapos;
    u32 metava, metapos, at, oft, iat, dll, ep, rel, blk, amount, limit, last, entrylen, flags;
    u32 debug_iat = 0, debug_link = 0;
    int contains_init = 0, k;
    if (ver != 10 && ver != 50)
        return "The carrier is for NT 3.1 or 3.50.";
    if (n < 256 || n > INPUT_MAX || rw(p) != 0x5a4d)
        return "Select an x86 SCSI miniport SYS file.";
    nt = rd(p + 60);
    if (!has(nt, 248, n) || rd(p + nt) != 0x4550 || rw(p + nt + 4) != 0x14c ||
        rw(p + nt + 20) < 224)
        return "The driver is not an x86 PE32 image.";
    o = p + nt + 24;
    ns = rw(p + nt + 6);
    sh = nt + 24 + rw(p + nt + 20);
    if (rw(o) != 0x10b || rw(o + 68) != 1 || !ns || ns > 32 || !has(sh, ns * 40, n) ||
        rd(o + 92) < 6)
        return "Unsupported native driver headers.";
    img = rd(o + 56);
    entry0 = rd(o + 16);
    if (rd(o + 28) < BIAS || (rd(o + 28) & 65535) || img > INPUT_MAX || !entry0 || entry0 >= img ||
        rd(o + 32) != rd(o + 36) || !rd(o + 32) || rd(o + 32) > 4096 ||
        (rd(o + 32) & (rd(o + 32) - 1)))
        return "This miniport needs a different section layout.";
    if (rd(o + 96 + 9 * 8) || rd(o + 96 + 13 * 8))
        return "TLS and delay-loaded kernel imports are unsupported.";
    /* Compact NT miniports map file positions at the same relative address.
       This lets the carrier retain every byte, including the original headers. */
    last = rd(o + 60);
    flags = 0;
    for (i = 0; i < ns; i++) {
        s = p + sh + 40 * i;
        va = rd(s + 12);
        sz = rd(s + 16);
        raw = rd(s + 20);
        if (va < last || (sz && va != raw) || !has(raw, sz, n) || !has(va, sz, img) ||
            !has(va, rd(s + 8), img))
            return "Unsupported miniport section mapping.";
        last = va + (sz > rd(s + 8) ? sz : rd(s + 8));
        if (entry0 >= va && entry0 - va < sz)
            flags = rd(s + 36);
    }
    if (!(flags & 0x20000000u))
        return "The miniport entry is outside executable code.";
    impva = rd(o + 104);
    impbytes = rd(o + 108);
    if (impbytes < 40 || !impva || !has(impva, 40, n))
        return "Missing SCSI import directory.";
    imp = p + impva;
    off = rd(imp + 12);
    if (!has(off, 13, n))
        return "Invalid import module name.";
    for (i = 0; i < 13; i++) {
        byte c = p[off + i];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (c != (byte) "SCSIPORT.SYS"[i])
            return "This adapter supports miniports that import only SCSIPORT.SYS.";
    }
    if (rd(imp + 32)) {
        static const char kernel[] = "NTOSKRNL.EXE";
        u32 module = rd(imp + 32), table = rd(imp + 20), hint;
        debug_iat = rd(imp + 36);
        if (!table)
            table = debug_iat;
        if (impbytes < 60 || !has(impva, 60, n) || !has(module, sizeof(kernel), n) || !table ||
            !debug_iat || !has(table, 8, n) || !has(debug_iat, 8, n))
            return "Invalid kernel debug import.";
        for (i = 0; i < sizeof(kernel); ++i) {
            byte c = p[module + i];
            if (c >= 'a' && c <= 'z')
                c -= 32;
            if (c != (byte)kernel[i])
                return "The second import module must be the native NT kernel.";
        }
        hint = rd(p + table);
        if (!has(hint, 2 + sizeof("DbgPrint"), n) ||
            memcmp(p + hint + 2, "DbgPrint", sizeof("DbgPrint")) || rd(p + table + 4) ||
            rd(p + debug_iat + 4))
            return "The additional kernel import must be DbgPrint.";
    }
    for (i = debug_iat ? 40 : 20; i < (debug_iat ? 60u : 40u); i++)
        if (imp[i])
            return "Unsupported additional import descriptor.";
    iat0 = rd(imp + 16);
    lookup = rd(imp);
    if (!lookup)
        lookup = iat0;
    for (;;) {
        if (count >= MAX_API || !has(lookup, count * 4 + 4, n) || !has(iat0, count * 4 + 4, n))
            return "Invalid SCSI import table.";
        off = rd(p + lookup + 4 * count);
        if (!off)
            break;
        if (!has(off, 3, n) || !strbound(p + off + 2, n - off - 2))
            return "Ordinal or invalid SCSI import.";
        name = (const char *)p + off + 2;
        if (strcmp(name, "ScsiDebugPrint") && (strlen(name) < 9 || memcmp(name, "ScsiPort", 8)))
            return "Unexpected storage API.";
        imports[count] = name;
        contains_init |= !strcmp(name, "ScsiPortInitialize");
        ++count;
    }
    if (!contains_init)
        return "The file does not register a SCSI miniport.";
    rl = rd(o + 136);
    rlsize = rd(o + 140);
    if (!rl || !has(rl, rlsize, n) || rlsize < 8 || rlsize > 65536)
        return "Unsupported relocation directory.";
    for (i = 0; i < rlsize; i += amount) {
        if (!has(i, 8, rlsize))
            return "Truncated relocation block.";
        blk = rd(p + rl + i);
        amount = rd(p + rl + i + 4);
        if ((blk & 4095) || amount < 8 || (amount & 1) || !has(i, amount, rlsize))
            return "Invalid relocation block.";
        for (j = 8; j < amount; j += 2) {
            u32 r = rw(p + rl + i + j);
            if ((r >> 12) != 0 && ((r >> 12) != 3 || !has(blk + (r & 4095), 4, img) || blk >= img))
                return "Only native x86 HIGHLOW relocations are supported.";
        }
    }
    for (i = 0; i < count; i++) {
        islocal[i] = localapi(ver, imports[i], mapped + i);
        if (!islocal[i]) {
            k = indexof(link, nlink, imports[i]);
            if (k < 0) {
                k = (int)nlink;
                link[nlink++] = imports[i];
            }
            mapped[i] = (u32)k;
        }
    }
    for (i = 0; i < (ver == 10 ? 4u : 5u); i++)
        if (indexof(link, nlink, native[i].name) < 0)
            link[nlink++] = native[i].name;
    limit = n + LINK_SPACE;
    memset(out, 0, limit);
    memcpy(out + HDR, p, n);
    codeva = BIAS + roundto(img > n ? img : n, PAGE);
    codepos = HDR + roundto(n, 512);
    datava = codeva + OFF_data_start;
    datapos = codepos + OFF_data_start;
    metava = roundto(codeva + sizeof(payload), PAGE);
    metapos = roundto(codepos + sizeof(payload), 512);
    if (metapos + 40 >= limit)
        return "The adapter is too large.";
    cs = out + codepos;
    memcpy(cs, payload, sizeof(payload));
    md = out + metapos;
    oft = debug_iat ? 60 : 40;
    iat = oft + 4 * (nlink + 1);
    dll = iat + 4 * (nlink + 1);
    at = dll + 14;
    memcpy(md + dll, "SCSIPORT.SYS", 13);
    wr(md, metava + oft);
    wr(md + 12, metava + dll);
    wr(md + 16, metava + iat);
    if (debug_iat) {
        u32 kname = at, thunk = roundto(kname + sizeof("ntoskrnl.exe"), 4);
        u32 hint = thunk + 16;
        debug_link = metava + thunk + 8;
        memcpy(md + kname, "ntoskrnl.exe", sizeof("ntoskrnl.exe"));
        memcpy(md + hint + 2, "DbgPrint", sizeof("DbgPrint"));
        wr(md + 20, metava + thunk);
        wr(md + 32, metava + kname);
        /* Early NT resolves names from FirstThunk as well as the lookup table. */
        wr(md + 36, debug_link);
        wr(md + thunk + 8, metava + hint);
        wr(md + thunk, metava + hint);
        at = hint + 2 + sizeof("DbgPrint");
    }
    for (i = 0; i < nlink; i++) {
        at = roundto(at, 2);
        wr(md + oft + 4 * i, metava + at);
        wr(md + iat + 4 * i, metava + at);
        j = (u32)strlen(link[i]) + 1;
        memcpy(md + at + 2, link[i], j);
        at += j + 2;
    }
    for (i = 0; i < (ver == 10 ? 4u : 5u); i++) {
        off = native[i].offset;
        k = indexof(link, nlink, native[i].name);
        /* call next; pop eax; jmp dword ptr [eax + displacement] */
        wr(cs + off + 8, metava + iat + 4 * (u32)k - (codeva + off + 5));
    }
    ep = roundto(at, 4);
    entry = md + ep;
    /* Link only the named SCSI imports. The NT image loader maps and
       relocates the embedded machine code and resolves the native IAT. */
    entry[0] = 0xe8;
    entry[5] = 0x59;
    entry[6] = 0x81;
    entry[7] = 0xe9;
    wr(entry + 8, metava + ep + 5);
    entrylen = 12;
    for (i = 0; i < count + (debug_iat != 0); i++) {
        int debug = i == count;
        int local = !debug && islocal[i];
        byte *e = entry + entrylen;
        e[0] = local ? 0x8d : 0x8b;
        e[1] = 0x81;
        wr(e + 2, debug ? debug_link : (local ? codeva + mapped[i] : metava + iat + 4 * mapped[i]));
        e[6] = 0x89;
        e[7] = 0x81;
        wr(e + 8, BIAS + (debug ? debug_iat : iat0 + 4 * i));
        entrylen += 12;
    }
    entry[entrylen] = 0xe9;
    wr(entry + entrylen + 1, BIAS + entry0 - (metava + ep + entrylen + 5));
    rel = roundto(ep + entrylen + 5, 4);
    if (!has(metapos + rel, rlsize, limit - 512))
        return "The adapter relocation data is too large.";
    memcpy(md + rel, p + rl, rlsize);
    for (i = 0; i < rlsize; i += rd(md + rel + i + 4))
        wr(md + rel + i, rd(md + rel + i) + BIAS);
    at = rel + rlsize;
    *used = metapos + roundto(at, 512);
    memcpy(out, "MZ", 2);
    wr(out + 60, 128);
    memcpy(out + 128, "PE", 2);
    ww(out + 132, 0x14c);
    ww(out + 134, 5);
    ww(out + 148, 224);
    ww(out + 150, 0x102);
    h = out + 152;
    ww(h, 0x10b);
    h[2] = 2;
    wr(h + 4, OFF_data_start + roundto(n, 512));
    wr(h + 8, roundto(at, 512));
    wr(h + 12, BIAS - PAGE);
    wr(h + 16, metava + ep);
    wr(h + 20, BIAS);
    wr(h + 24, datava);
    wr(h + 28, rd(o + 28) - BIAS);
    wr(h + 32, PAGE);
    wr(h + 36, 512);
    ww(h + 40, 3);
    ww(h + 42, 10);
    ww(h + 48, 1);
    wr(h + 56, roundto(metava + at, PAGE));
    wr(h + 60, HDR);
    ww(h + 68, 1);
    wr(h + 72, 0x100000);
    wr(h + 76, PAGE);
    wr(h + 80, 0x100000);
    wr(h + 84, PAGE);
    wr(h + 92, 16);
    wr(h + 104, metava);
    wr(h + 108, debug_iat ? 60 : 40);
    wr(h + 136, metava + rel);
    wr(h + 140, rlsize);
    h += 224;
    section(h, ".pad", BIAS - PAGE, PAGE, 0, 0, 0xc8000080);
    section(h + 40, ".mini", codeva - BIAS, BIAS, roundto(n, 512), HDR, 0xe8000020);
    section(h + 80, ".n3code", OFF_data_start, codeva, OFF_data_start, codepos, 0x68000020);
    section(h + 120, ".n3data", sizeof(payload) - OFF_data_start, datava, metapos - datapos,
            datapos, 0xc8000040);
    section(h + 160, ".n3link", at, metava, roundto(at, 512), metapos, 0x68000020);
    wr(out + 152 + 64, checksum(out, *used));
    return 0;
}
