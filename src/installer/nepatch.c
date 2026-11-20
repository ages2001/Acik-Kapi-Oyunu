#include "nepatch.h"
#include "menucode.h"
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

typedef unsigned char Byte;
typedef uint16_t Word;
typedef uint32_t Dword;
typedef struct {
    FILE *f;
    long size, base, unit, segments, mods, names;
    unsigned count, imports;
    Byte h[64];
} Ne;

typedef struct {
    long entry, start, len, minimum;
    unsigned flags, fixups;
} Segment;

static Word word(const Byte *p) { return (Word)(p[0] | ((Word)p[1] << 8)); }
static Dword dword(const Byte *p) { return word(p) | ((Dword)word(p+2) << 16); }
static void setword(Byte *p, unsigned n) { p[0] = (Byte)n; p[1] = (Byte)(n >> 8); }
static void setdword(Byte *p, Dword n) { setword(p, (Word)n); setword(p+2, (Word)(n >> 16)); }
static int inside(long off, long length, long end) {
    return off >= 0 && length >= 0 && off <= end && length <= end - off;
}
static int readat(Ne *n, long at, void *p, unsigned bytes) {
    return inside(at, bytes, n->size) && !fseek(n->f, at, SEEK_SET) &&
           fread(p, 1, bytes, n->f) == bytes;
}
static int writeat(Ne *n, long at, const void *p, unsigned bytes) {
    if (at < 0 || at > n->size || at > LONG_MAX - bytes || fseek(n->f, at, SEEK_SET) ||
        fwrite(p, 1, bytes, n->f) != bytes) return 0;
    if (at + bytes > n->size) n->size = at + bytes;
    return 1;
}
static int readword(Ne *n, long at, Word *v) {
    Byte b[2];
    if (!readat(n, at, b, 2)) return 0;
    *v = word(b);
    return 1;
}
static int writeword(Ne *n, long at, unsigned v) {
    Byte b[2];
    setword(b, v);
    return writeat(n, at, b, 2);
}
static long relative(Ne *n, unsigned field) { return n->base + word(n->h+field); }

/* Match a length-prefixed NE identifier. Resident module names also carry ordinal zero. */
static int identifier(Ne *n, long at, const char *s, int resident) {
    Byte b[40];
    unsigned i, len = (unsigned)strlen(s);
    if (len + 3 > sizeof(b) || !readat(n, at, b, len + (resident ? 3 : 1)) || b[0] != len)
        return 0;
    for (i = 0; i < len; ++i)
        if (toupper(b[1+i]) != s[i]) return 0;
    return !resident || !word(b+len+1);
}
static int image_file(Ne *n, FILE *f) {
    memset(n,0,sizeof(*n)); n->f=f;
    return !fseek(f,0,SEEK_END) && (n->size=ftell(f))>=0;
}
static int ne_header(Ne *n, long at, int bound) {
    unsigned shift;
    n->base=at;
    if (at<64 || at>LONG_MAX-65536L || !readat(n,at,n->h,64) || memcmp(n->h,"NE",2) ||
        (!bound && n->h[0x36]!=2)) return 0;
    shift = word(n->h+0x32);
    if (shift > 15) return 0;
    n->unit = bound ? 16 : 1L << shift;
    n->count = word(n->h+0x1c);
    n->imports = word(n->h+0x1e);
    n->segments = relative(n,0x22);
    n->mods = relative(n,0x28);
    n->names = relative(n,0x2a);
    return n->segments >= n->base+64 && n->mods >= n->base+64 &&
           n->names >= n->base+64 && inside(n->segments, 8L*n->count, n->size) &&
           inside(n->mods, 2L*n->imports, n->size);
}
static int module_file(Ne *n, FILE *f, const char *module, int bound) {
    Byte mz[64];
    return image_file(n,f) && readat(n,0,mz,64) && !memcmp(mz,"MZ",2) &&
           ne_header(n,(long)dword(mz+60),bound) && n->count &&
           identifier(n,relative(n,0x26),module,1);
}
static int open_ne(Ne *n, FILE *f, const char *module) {
    return module_file(n,f,module,0);
}
int ne_windows_version(FILE *f, const char *name) {
    Ne n;
    return open_ne(&n, f, name) ? word(n.h+0x3e) : 0;
}
static int segment(Ne *n, unsigned index, Segment *s) {
    Byte b[8];
    Word nr = 0;
    if (!index || index > n->count) return 0;
    s->entry = n->segments + 8L*(index-1);
    if (!readat(n, s->entry, b, 8)) return 0;
    s->start = (long)word(b)*n->unit;
    s->len = word(b+2) ? (long)word(b+2) : 65536L;
    s->minimum = word(b+6) ? (long)word(b+6) : 65536L;
    s->flags = word(b+4);
    if (s->start && (!inside(s->start, s->len, n->size) ||
        ((s->flags & 0x100) && (!readword(n, s->start+s->len, &nr) ||
         !inside(s->start+s->len+2, 8L*nr, n->size))))) return 0;
    s->fixups = nr;
    return 1;
}
static int module_index(Ne *n, const char *name) {
    unsigned i, found = 0;
    Word off;
    for (i = 0; i < n->imports; ++i) {
        Byte len;
        if (!readword(n, n->mods+2L*i, &off) || !readat(n, n->names+off, &len, 1) ||
            !inside(n->names+off+1, len, n->size)) return -1;
        if (identifier(n, n->names+off, name, 0)) {
            if (found) return -1;
            found = i+1;
        }
    }
    return found;
}
static int export_entry(Ne *n, unsigned wanted, long *field) {
    long pos = relative(n,4), end = pos+word(n->h+6);
    unsigned first = 1, count, width, seg;
    Byte b[6];
    if (!inside(pos, end-pos, n->size)) return 0;
    while (inside(pos, 2, end) && readat(n, pos, b, 2) && b[0]) {
        count = b[0]; seg = b[1];
        width = !seg ? 0 : seg == 255 ? 6 : 3;
        pos += 2;
        if (!inside(pos, (long)count*width, end)) return 0;
        if (wanted >= first && wanted-first < count) {
            if (!seg || seg == 254 || !readat(n,pos+(wanted-first)*width,b,width)) return 0;
            if (field) *field=pos+(wanted-first)*width+(seg==255 ? 4 : 1);
            return seg == 255 ? b[3] : seg;
        }
        pos += count*width;
        if (first > 65535U-count) return 0;
        first += count;
    }
    return 0;
}

static int export_segment(Ne *n, unsigned wanted) { return export_entry(n,wanted,NULL); }

/* GetVersion is KERNEL ordinal 3 in the bound Executive releases. Resolve
   its entry and the CS-relative Windows version word used by the 2.x kernel. */
unsigned bound_windows_version(FILE *file) {
    Ne n;
    Segment s;
    long field;
    Word off,value;
    Byte code[17];
    if (!module_file(&n,file,"KERNEL",1) || !segment(&n,export_entry(&n,3,&field),&s) ||
        !s.start || !(s.flags&0x40) || !readword(&n,field,&off) || !inside(off,sizeof(code),s.len) ||
        !readat(&n,s.start+off,code,sizeof(code))) return 0;
    if (code[0]==0xb8 && word(code+1)==1 && code[3]==0xcb) return 0x0100;
    if (code[0]!=0x2e || code[1]!=0xa1 || memcmp(code+4,"\x86\xe0\x2e\x8a\x36",5) ||
        memcmp(code+11,"\x2e\x8a\x16",3) || code[16]!=0xcb ||
        word(code+9)==65535U || word(code+14)!=word(code+9)+1U || !inside(word(code+2),2,s.len) ||
        !readword(&n,s.start+word(code+2),&value)) return 0;
    return value;
}

/* Streaming instruction matcher. ?? marks an operand resolved or checked by the caller. */
static int hexval(int c) { return c <= '9' ? c-'0' : c-'a'+10; }
static int match(Ne *n, const Segment *s, const char *pattern, long *hit) {
    Byte want[64], mask[64], buf[1088];
    unsigned len = 0, got, overlap = 0, i, j;
    long pos = 0, start;
    *hit = -1;
    while (*pattern) {
        if (*pattern == ' ') { ++pattern; continue; }
        if (len == sizeof(want) || !pattern[1]) return 0;
        mask[len] = *pattern != '?';
        want[len++] = *pattern == '?' ? 0 : (Byte)((hexval(pattern[0])<<4) | hexval(pattern[1]));
        pattern += 2;
    }
    while (pos < s->len) {
        got = s->len-pos > 1024 ? 1024 : (unsigned)(s->len-pos);
        if (!readat(n,s->start+pos,buf+overlap,got)) return 0;
        start = pos-overlap;
        pos += got;
        got += overlap;
        for (i = 0; i+len <= got; ++i) {
            for (j = 0; j < len && (!mask[j] || want[j] == buf[i+j]); ++j) {}
            if (j != len) continue;
            if (*hit >= 0) return 0;
            *hit = start+i;
        }
        overlap = got < len-1 ? got : len-1;
        memmove(buf,buf+got-overlap,overlap);
    }
    return 1;
}

/* Relocations identify the imported function even when linkers reorder imports and fixup chains. */
typedef struct { long entry, previous; Word next; } Link;
static int import_link(Ne *n, const Segment *s, unsigned kernel, unsigned ordinal,
                       const char *name, long site, Link *out) {
    unsigned i;
    Byte r[8];
    int found = 0;
    for (i = 0; i < s->fixups; ++i) {
        long entry = s->start+s->len+2+8L*i, prev = -1, remaining = s->len;
        Word curr, next;
        if (!readat(n,entry,r,8)) return 0;
        if (r[0] != 3 || word(r+4) != kernel ||
            !(r[1] == 1 ? word(r+6) == ordinal :
              r[1] == 2 && identifier(n,n->names+word(r+6),name,0))) continue;
        curr = word(r+2);
        while (curr != 0xffff) {
            if (!remaining-- || !inside(curr,4,s->len) || !readword(n,s->start+curr,&next)) return 0;
            if (curr == site) {
                if (found) return 0;
                out->entry = entry; out->previous = prev; out->next = next;
                found = 1;
            }
            prev = curr;
            curr = next;
        }
    }
    return found;
}
static int zeros(Ne *n, long at, long length) {
    static const Byte zero[128] = {0};
    while (length > 0) {
        unsigned part = length > (long)sizeof(zero) ? sizeof(zero) : (unsigned)length;
        if (!writeat(n, at, zero, part)) return 0;
        at += part; length -= part;
    }
    return 1;
}
static int transfer(Ne *n, long dst, long src, long len) {
    Byte b[1024];
    long off;
    int backward = dst > src && dst-src < len;
    while (len) {
        unsigned part = len > (long)sizeof(b) ? sizeof(b) : (unsigned)len;
        off = backward ? len-part : 0;
        if (!readat(n, src+off, b, part) || !writeat(n,dst+off,b,part)) return 0;
        if (!backward) { src += part; dst += part; }
        len -= part;
    }
    return 1;
}
static int insert(Ne *n, long at, long count) {
    long old = n->size;
    return inside(at,0,old) && count >= 0 && count <= LONG_MAX-old &&
           zeros(n,old,count) && transfer(n,at+count,at,old-at) && zeros(n,at,count);
}
static long aligned(long n, long unit) { return n + (unit - n%unit)%unit; }
static int write_segment(Ne *n, const Segment *s, long base, long len) {
    Byte b[8];
    if (base%n->unit || base/n->unit > 65535L || len <= 0 || len > 65536L) return 0;
    setword(b,(Word)(base/n->unit)); setword(b+2,(Word)len);
    setword(b+4,s->flags); setword(b+6,(Word)(len < s->minimum ? s->minimum : len));
    return writeat(n,s->entry,b,8);
}

/* Insert the dependency in the existing tables, retaining the target's import indices.
   All segment and resource offsets use their respective alignment units. */
static int add_dependency(Ne *n) {
    static const Byte name[] = {7,'W','R','M','P','A','G','E'};
    static const unsigned fields[] = {4,0x22,0x24,0x26,0x28,0x2a};
    long refs = n->mods+2L*n->imports, end = n->size, res = relative(n,0x24);
    long limit = relative(n,0x26), growth, runit = 1, pos, next, absolute;
    unsigned i, j;
    Word shift, kind, count, units, amount;
    int relocate_refs = n->mods >= n->names;
    int has = module_index(n,"WRMPAGE");
    if (has < 0) return 0;
    if (has) return 1;
    if ((!relocate_refs && refs > n->names) || n->imports >= 0xfffe || res > limit) return 0;
    /* Older linkers can leave padding between the reference and name tables. */
    for (i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i) {
        next = relative(n,fields[i]);
        if (next > n->names && next < end) end = next;
    }
    absolute = (long)dword(n->h+0x2c);
    if (absolute > n->names && absolute < end) end = absolute;
    for (i = 0; i < n->count; ++i) {
        Segment s;
        if (!segment(n,i+1,&s) || (s.start && s.start <= n->names)) return 0;
        if (s.start && s.start < end) end = s.start;
    }
    if (res != limit) {
        if (!readword(n,res,&shift) || shift > 15) return 0;
        runit = 1L << shift;
    }
    growth = aligned(sizeof(name)+2L+(relocate_refs ? 2L*n->imports : 0),
                     runit > n->unit ? runit : n->unit);
    if (end <= n->names || end-n->names > 65535L || growth > LONG_MAX-n->size) return 0;
    for (i = 0; i < n->count; ++i) {
        Segment s;
        if (!segment(n,i+1,&s)) return 0;
        if (!s.start) continue;
        if (s.start < end || (s.start+growth)/n->unit > 65535L ||
            !writeword(n,s.entry,(Word)((s.start+growth)/n->unit))) return 0;
    }
    if (res != limit) {
        pos = res+2;
        for (;;) {
            if (!inside(pos,2,limit) || !readword(n,pos,&kind)) return 0;
            if (!kind) break;
            if (!inside(pos,8,limit) || !readword(n,pos+2,&count)) return 0;
            pos += 8;
            if (!inside(pos,12L*count,limit)) return 0;
            for (j = 0; j < count; ++j, pos += 12) {
                if (!readword(n,pos,&units) || !readword(n,pos+2,&amount)) return 0;
                if (!units) continue;
                if ((long)units*runit < end || !inside((long)units*runit,(long)amount*runit,n->size) ||
                    units+growth/runit > 65535L ||
                    !writeword(n,pos,(Word)(units+growth/runit))) return 0;
            }
        }
    }
    if (absolute) {
        if (absolute < end || !inside(absolute,word(n->h+0x20),n->size)) return 0;
        setdword(n->h+0x2c,(Dword)(absolute+growth));
    }
    if (n->h[0x37] & 8) {
        long fast = (long)word(n->h+0x38)*n->unit;
        if (fast < end || (fast+growth)/n->unit > 65535L) return 0;
        setword(n->h+0x38,(Word)((fast+growth)/n->unit));
    }
    for (i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i) {
        unsigned field = fields[i];
        long off = word(n->h+field), at = n->base+off;
        if (at >= end) off += growth;
        else if (!relocate_refs && at >= refs && field != 0x28) off += 2;
        if (off > 65535L) return 0;
        setword(n->h+field,(Word)off);
    }
    setword(n->h+0x1e,n->imports+1);
    if (!insert(n,end,growth)) return 0;
    if (relocate_refs) {
        /* The name table precedes the module references. Append the name
           and a fresh reference table, preserving every existing name offset. */
        long table=end+sizeof(name);
        if (table-n->base>65535L || !transfer(n,table,n->mods+growth,2L*n->imports) ||
            !writeword(n,table+2L*n->imports,(Word)(end-n->names)) ||
            !writeat(n,end,name,sizeof(name))) return 0;
        setword(n->h+0x28,(Word)(table-n->base));
        return writeat(n,n->base,n->h,64);
    }
    return transfer(n,refs+2,refs,end-refs) &&
           writeword(n,refs,(Word)(end-n->names)) && writeat(n,end+2,name,sizeof(name)) &&
           writeat(n,n->base,n->h,64);
}

static int user30(Ne *n, const Segment *s, unsigned kernel) {
    long hit;
    Word size, recorded;
    Link alloc;
    if (!match(n,s,"b8 42 00 50 b8 ?? ?? 50 9a ?? ?? ?? ?? 8b f0 0b f6 74 ?? 8b de 8b 3f c7 45 06 ?? ?? c7 45 02 ff ff c7 45 04 ff ff",&hit) ||
        hit < 0 || !readword(n,s->start+hit+5,&size) || !readword(n,s->start+hit+26,&recorded) ||
        size != recorded || (size != 42 && size != 16) ||
        !import_link(n,s,kernel,5,"LOCALALLOC",hit+9,&alloc)) return 0;
    return size == 16 || (writeword(n,s->start+hit+5,16) && writeword(n,s->start+hit+26,16));
}
static int user9x(Ne *n, unsigned kernel) {
    unsigned i;
    long site = -1, off;
    Byte bytes[32];
    Link alloc;
    for (i = 0; i < n->count; ++i) {
        Segment s;
        if (!segment(n,i+1,&s)) return 0;
        if (!s.start || (s.flags & 9)) continue;
        if (!match(n,&s,"89 46 ?? 89 56 ?? 0b d0 75 08 66 c7 46 ?? ?? ?? ?? ?? ff 76 ?? 6a 00 68 00 10 66 ff 76 ?? 6a 00 6a 02 9a",&off)) return 0;
        if (off < 0) continue;
        if (site >= 0 || !readat(n,s.start+off,bytes,sizeof(bytes)) ||
            bytes[2] != bytes[13] || bytes[2] != bytes[29] || (Byte)(bytes[2]+2) != bytes[5] ||
            !import_link(n,&s,kernel,208,"K208",off+35,&alloc)) return 0;
        site = s.start+off+14;
    }
    if (site < 0 || !readat(n,site,bytes,4)) return 0;
    if (dword(bytes) == 0x01fffc00UL) return 1;
    if (dword(bytes) != 0xffffffffUL) return 0;
    setdword(bytes,0x01fffc00UL);
    return writeat(n,site,bytes,4);
}
static int new_site(Ne *n, long at, long dest, unsigned space) {
    Byte b[12];
    if (space > sizeof(b)) return 0;
    memset(b,0x90,space); b[0] = 0xe8; setword(b+1,(Word)(dest-at-3));
    return writeat(n,at,b,space);
}
static int add_fixup(Ne *n, long *pos, unsigned type, unsigned kind, unsigned off,
                     unsigned target, unsigned ordinal) {
    Byte r[8];
    r[0] = (Byte)type; r[1] = (Byte)kind;
    setword(r+2,off); setword(r+4,target); setword(r+6,ordinal);
    if (!writeat(n,*pos,r,8)) return 0;
    *pos += 8;
    return 1;
}
static void menu_bytes(Byte *code, Word root) {
    unsigned i;
    memcpy(code,menu_code,sizeof(menu_code));
    for (i = 0; i < sizeof(menu_roots)/sizeof(menu_roots[0]); ++i)
        setword(code+menu_roots[i],root);
}
static int menu_done(Ne *n, const Segment *s, const Segment *data, unsigned kernel) {
    long call, release;
    Word to_alloc, to_free;
    Byte code[sizeof(menu_code)], actual[sizeof(menu_code)];
    Link link;
    unsigned i;
    if (!match(n,s,"e8 ?? ?? 90 90 90 90 90 90 8b f0 0b f6",&call) || call < 0 ||
        !match(n,s,"c7 45 04 00 00 e8 ?? ?? 90 90 90 b8 01 00",&release) || release < 0 ||
        !readword(n,s->start+call+1,&to_alloc) || !readword(n,s->start+release+6,&to_free))
        return 0;
    call = (Word)(call+3+to_alloc);
    release = (Word)(release+8+to_free);
    if (release != call+menu_free || !inside(call,sizeof(code),s->len) ||
        data->len < 2 || !readat(n,s->start+call,actual,sizeof(actual))) return 0;
    menu_bytes(code,(Word)(data->len-2));
    if (memcmp(code,actual,sizeof(code))) return 0;
    for (i = 0; i < sizeof(menu_fixups)/sizeof(menu_fixups[0]); ++i) {
        unsigned ordinal = menu_fixups[i].ordinal;
        if (ordinal && !import_link(n,s,kernel,ordinal,ordinal == 5 ? "LOCALALLOC" : "LOCALFREE",
                                    call+menu_fixups[i].off,&link)) return 0;
    }
    return 1;
}
static int user31(Ne *n, const Segment *s, unsigned kernel) {
    unsigned dgroup = word(n->h+0x0e), i, j;
    Segment data;
    long alloc, release, root, dest, org, length, pos, start, old;
    Link links[2];
    Byte code[sizeof(menu_code)], r[8];
    if (!segment(n,dgroup,&data) || !(data.flags & 1) || !data.start || (data.flags & 8) ||
        !(s->flags & 0x100) || !match(n,s,"6a 40 6a 12 9a ?? ?? ?? ?? 8b f0 0b f6",&alloc))
        return 0;
    if (alloc < 0) return menu_done(n,s,&data,kernel);
    if (!match(n,s,"c7 45 04 00 00 57 9a ?? ?? ?? ?? b8 01 00",&release) || release < 0)
        return 0;
    release += 5;
    if (!import_link(n,s,kernel,5,"LOCALALLOC",alloc+5,&links[0]) ||
        !import_link(n,s,kernel,7,"LOCALFREE",release+2,&links[1])) return 0;
    root = aligned(data.minimum > data.len ? data.minimum : data.len,2);
    org = aligned(s->len,16);
    length = org+sizeof(code);
    if (root > 65534L || length > 65535L || n->size > LONG_MAX-262144L) return 0;
    dest = aligned(n->size,n->unit);
    if (!zeros(n,n->size,dest-n->size) || !transfer(n,dest,data.start,data.len) ||
        !zeros(n,dest+data.len,root+2-data.len) ||
        ((data.flags & 0x100) && !transfer(n,dest+root+2,data.start+data.len,2+8L*data.fixups)) ||
        !write_segment(n,&data,dest,root+2)) return 0;
    dest = aligned(n->size,n->unit);
    menu_bytes(code,(Word)root);
    if (!zeros(n,n->size,dest-n->size) || !transfer(n,dest,s->start,s->len) ||
        !zeros(n,dest+s->len,org-s->len) || !writeat(n,dest+org,code,sizeof(code)) ||
        !writeword(n,dest+length,0)) return 0;
    start = pos = dest+length+2;
    for (i = 0; i < s->fixups; ++i) {
        int omit = 0;
        old = s->start+s->len+2+8L*i;
        if (!readat(n,old,r,8)) return 0;
        for (j = 0; j < 2; ++j) {
            if (links[j].entry != old) continue;
            if (links[j].previous >= 0) {
                if (!writeword(n,dest+links[j].previous,links[j].next)) return 0;
            } else if (links[j].next == 0xffff) omit = 1;
            else setword(r+2,links[j].next);
        }
        if (!omit) {
            if (!writeat(n,pos,r,8)) return 0;
            pos += 8;
        }
    }
    for (i = 0; i < sizeof(menu_fixups)/sizeof(menu_fixups[0]); ++i) {
        unsigned ord = menu_fixups[i].ordinal;
        if (!add_fixup(n,&pos,ord ? 3 : 2,ord ? 1 : 0,(Word)(org+menu_fixups[i].off),
                       ord ? kernel : dgroup,ord)) return 0;
    }
    if ((pos-start)/8 > 65535L || !writeword(n,dest+length,(Word)((pos-start)/8)) ||
        !new_site(n,dest+alloc,dest+org,9) || !new_site(n,dest+release,dest+org+menu_free,6) ||
        !write_segment(n,s,dest,length)) return 0;
    n->h[0x37] &= ~8;
    memset(n->h+0x38,0,4);
    return writeat(n,n->base,n->h,sizeof(n->h));
}
const char *patch_ne_user(FILE *stage, int win9x) {
    Ne n;
    Segment s;
    int kernel, menus;
    if (!open_ne(&n,stage,"USER")) return "Invalid Windows USER module or segment table";
    kernel = module_index(&n,"KERNEL");
    if (kernel <= 0) return "Invalid or ambiguous USER kernel import";
    if (win9x)
        return user9x(&n,(unsigned)kernel) ? NULL : "Cannot identify the unique USER shared heap allocation";
    menus = export_segment(&n,151); /* Windows USER.CreateMenu */
    if (!segment(&n,menus,&s) || !s.start || (s.flags & 9))
        return "Invalid USER.CreateMenu code segment";
    if (!user30(&n,&s,(unsigned)kernel) && !user31(&n,&s,(unsigned)kernel))
        return "Cannot identify compatible USER menu allocation and release code";
    if (!add_dependency(&n)) return "Cannot extend the USER module import table";
    return NULL;
}

static int pe_range(Ne *n, long table, unsigned count, Dword rva, long *offset) {
    unsigned i;
    Byte s[40];
    for (i=0; i<count; ++i) {
        Dword va, size, raw;
        if (!readat(n,table+40L*i,s,40)) return 0;
        va=dword(s+12); size=dword(s+16); raw=dword(s+20);
        if (rva>=va && rva-va<size && raw<=(Dword)n->size && size<=(Dword)n->size-raw) {
            *offset=(long)(raw+(rva-va));
            return 1;
        }
    }
    return 0;
}
/* Resource-directory entry offsets are relative to the start of .rsrc. */
static int resource_child(Ne *n,long root,long size,long dir,int wanted,int subdir,long *child) {
    Byte b[16];
    unsigned long i,count;
    if (!inside(dir,16,size) || !readat(n,root+dir,b,16)) return 0;
    count=(long)word(b+12)+word(b+14);
    if (count>(unsigned long)(size-dir-16)/8) return 0;
    for (i=0;i<count;++i) {
        Dword key,value;
        if (!readat(n,root+dir+16+(long)i*8,b,8)) return 0;
        key=dword(b); value=dword(b+4);
        if (wanted>=0 && key!=(Dword)wanted) continue;
        if (!!(value&0x80000000UL)!=subdir) return 0;
        *child=(long)(value&0x7fffffffUL);
        return inside(*child,16,size);
    }
    return 0;
}
int pe_product_version(FILE *file, unsigned *minor, unsigned *build) {
    static const char key[]="VS_VERSION_INFO";
    Ne n;
    Byte b[144];
    long h,sections,root,size,dir,at;
    unsigned i,count;
    if (!image_file(&n,file) || !readat(&n,0,b,64) || memcmp(b,"MZ",2)) return 0;
    h=(long)dword(b+60);
    if (!readat(&n,h,b,sizeof(b)) || memcmp(b,"PE\0\0",4) || word(b+4)!=0x14c ||
        word(b+24)!=0x10b || word(b+20)<120 || dword(b+116)<3) return 0;
    sections=h+24+word(b+20); count=word(b+6); size=(long)dword(b+140);
    if (!pe_range(&n,sections,count,dword(b+136),&root) || !inside(root,size,n.size) ||
        !resource_child(&n,root,size,0,16,1,&dir) ||
        !resource_child(&n,root,size,dir,-1,1,&dir) ||
        !resource_child(&n,root,size,dir,-1,0,&dir) || !readat(&n,root+dir,b,16) ||
        dword(b+4)<92 || !pe_range(&n,sections,count,dword(b),&at) || !readat(&n,at,b,92) ||
        word(b)<92 || word(b+2)<52) return 0;
    for (i=0;i<sizeof(key);++i) if (word(b+6+2*i)!=(unsigned)key[i]) return 0;
    if (dword(b+40)!=0xfeef04bdUL || dword(b+44)!=0x00010000UL || word(b+50)!=4) return 0;
    *minor=word(b+48); *build=word(b+52);
    return 1;
}
static int pe_checksum(Ne *n, long field) {
    Byte b[1024];
    Dword sum=0;
    long off;
    unsigned i, size;
    if (!writeword(n,field,0) || !writeword(n,field+2,0)) return 0;
    for (off=0; off<n->size; off+=size) {
        size=n->size-off>(long)sizeof(b) ? sizeof(b) : (unsigned)(n->size-off);
        if (!readat(n,off,b,size)) return 0;
        for (i=0; i+1<size; i+=2) sum+=word(b+i);
        if (i<size) sum+=b[i];
        sum=(sum & 0xffff)+(sum>>16);
    }
    sum=(sum & 0xffff)+(sum>>16);
    setdword(b,sum+(Dword)n->size);
    return writeat(n,field,b,4);
}
const char *patch_pe_memory(FILE *f) {
    static const unsigned fields[] = {3,12,19,28,39,46};
    Ne n;
    Byte b[152], section[40], name[13];
    unsigned i, count;
    long h, table, at, pos=-1;
    int changed=0;
    n.f=f;
    if (fseek(f,0,SEEK_END) || (n.size=ftell(f))<0 || !readat(&n,0,b,64) ||
        memcmp(b,"MZ",2) || (h=(long)dword(b+60))<64 || !readat(&n,h,b,144) ||
        memcmp(b,"PE\0\0",4) || word(b+4)!=0x14c || word(b+24)!=0x10b ||
        word(b+20)<120 || dword(b+116)<1) return "Invalid Win32 kernel header";
    count=word(b+6); table=h+24+word(b+20);
    if (!inside(table,40L*count,n.size) || !pe_range(&n,table,count,dword(b+120),&at) ||
        !readat(&n,at,b,40) || !pe_range(&n,table,count,dword(b+12),&at) ||
        !readat(&n,at,name,13)) return "Invalid Win32 kernel exports";
    for (i=0; i<12; ++i) name[i]=(Byte)toupper(name[i]);
    if (memcmp(name,"KERNEL32.DLL",13)) return "Unexpected Win32 module name";
    for (i=0; i<count; ++i) {
        Segment s;
        if (!readat(&n,table+40L*i,section,40)) return "Invalid PE section table";
        if (!(dword(section+36) & 0x20000000UL)) continue;
        s.start=(long)dword(section+20); s.len=(long)dword(section+16);
        if (!inside(s.start,s.len,n.size) ||
            !match(&n,&s,"81 7d ?? ?? ?? ?? ?? 72 07 c7 45 ?? ?? ?? ?? ?? 81 7d ?? ?? ?? ?? ?? 72 07 c7 45 ?? ?? ?? ?? ?? 8b 45 ?? 03 45 ?? 3d ?? ?? ?? ?? 72 ?? b8 ?? ?? ?? ?? 2b 45 ?? 89 45 ??",&at))
            return "Invalid or ambiguous memory-status code";
        if (at<0) continue;
        if (pos>=0) return "Multiple memory-status code locations";
        pos=s.start+at;
    }
    if (pos<0 || !readat(&n,pos,b,55) || b[2]!=b[11] || b[18]!=b[27] ||
        b[2]==b[18] || (b[34]!=b[2] && b[37]!=b[2]) || b[52]!=b[2])
        return "Cannot identify the memory-status page limits";
    for (i=0; i<sizeof(fields)/sizeof(fields[0]); ++i) {
        Dword original=0x80000UL-(i&1), updated=0x100000UL-(i&1), v=dword(b+fields[i]);
        if (v!=original && v!=updated) return "Unexpected Win32 page limit";
        if (v==original) changed=1;
        setdword(b+fields[i],updated);
    }
    if (!changed) return NULL;
    return writeat(&n,pos,b,55) && pe_checksum(&n,h+24+64) ? NULL : "Cannot update Win32 kernel";
}

static int update_panel_strings(Ne *n) {
    static const struct { unsigned id; const char *text; } formats[] = {
        {1910,"%lu KiB RAM"}, {2703,"%1!lu! KiB of RAM"}, {2704,"%1!lu! KiB of RAM"},
        {2705,"Very low - %1!lu! KiB of RAM"}
    };
    long table=relative(n,0x24), end=relative(n,0x26), p=table+2, unit;
    unsigned i,j,k,seen=0;
    Word shift,kind,count;
    Byte fields[12], str[256], len;
    if (!readword(n,table,&shift) || shift>15) return 0;
    unit=1L<<shift;
    for (;;) {
        if (!inside(p,2,end) || !readword(n,p,&kind)) return 0;
        if (!kind) break;
        if (!inside(p,8,end) || !readword(n,p+2,&count)) return 0;
        p+=8;
        if (!inside(p,12L*count,end)) return 0;
        for (i=0;i<count;++i,p+=12) {
            long at,limit;
            unsigned group,base;
            if (kind!=0x8006) continue;
            if (!readat(n,p,fields,12)) return 0;
            group=word(fields+6);
            if (!(group&0x8000) || (group&0x7fff)==0) continue;
            group&=0x7fff;
            if (group>4096) continue;
            base=(group-1)*16;
            for (k=0;k<4;++k) if (formats[k].id>=base && formats[k].id-base<16) break;
            if (k==4) continue;
            at=(long)word(fields)*unit; limit=at+(long)word(fields+2)*unit;
            if (!inside(at,limit-at,n->size)) return 0;
            for (j=0;j<16;++j) {
                if (!inside(at,1,limit) || !readat(n,at,&len,1) || !inside(++at,len,limit))
                    return 0;
                for (k=0;k<4;++k) if (formats[k].id==base+j) {
                    unsigned width=(unsigned)strlen(formats[k].text);
                    if ((seen&(1U<<k)) || width>len || !readat(n,at,str,len)) return 0;
                    str[len]=0;
                    /* Require the numbered/printf substitution that the native page supplies. */
                    if (k ? !strstr((char *)str,"%1!") :
                            (!strstr((char *)str,"%u") && !strstr((char *)str,"%lu"))) return 0;
                    memset(str,' ',len); memcpy(str,formats[k].text,width);
                    if (!writeat(n,at,str,len)) return 0;
                    seen|=1U<<k;
                }
                at+=len;
            }
        }
    }
    return seen==15;
}
const char *patch_ne_panel(FILE *f) {
    static const struct { const char *context,*old,*updated; unsigned before,len; } sites[] = {
        {"89 46 f8 c7 46 fa 00 00 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ff 36 ?? ?? 68 76 07",
         "66 8b 46 fc 66 c1 e8 0a 66 89 46 fc", "90 90 90 90 90 90 90 90 90 90 90 90",8,12},
        {"ff 76 ?? ff 76 fc 8d 86 ?? ?? 16 50 8d 46 ?? 16 50 9a",
         "f8","fe",2,1},
        {"89 46 f6 89 56 f8 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 66 8b 46 f6 66 c1 e8 0a",
         "80 e4 03 b9 64 00 2b d2 f7 f1 89 46 fa c7 46 fc 00 00",
         "66 8b 46 f6 66 89 46 fa 90 90 90 90 90 90 90 90 90 90",6,18},
        {"66 ff 76 fa 66 ff 76 ?? 56 9a ?? ?? ?? ?? 83 c4 0a","f6","fa",7,1}
    };
    Ne n;
    Segment s;
    unsigned i,j;
    long hit;
    if (!open_ne(&n,f,"SYSDM") || !segment(&n,export_segment(&n,1),&s) || !s.start || (s.flags&9))
        return "Invalid System Properties module";
    for (i=0;i<sizeof(sites)/sizeof(sites[0]);++i) {
        Byte b[24],target[24],old[24];
        const char *p=sites[i].old, *q=sites[i].updated;
        if (!match(&n,&s,sites[i].context,&hit) || hit<0 ||
            !readat(&n,s.start+hit+sites[i].before,b,sites[i].len))
            return "Cannot identify a unique System Properties memory display";
        for (j=0;j<sites[i].len;++j) {
            old[j]=(Byte)((hexval(p[0])<<4)|hexval(p[1]));
            target[j]=(Byte)((hexval(q[0])<<4)|hexval(q[1]));
            if (j+1<sites[i].len) {p+=3;q+=3;}
        }
        if (memcmp(b,old,sites[i].len) && memcmp(b,target,sites[i].len))
            return "Incompatible System Properties memory calculation";
        if (!writeat(&n,s.start+hit+sites[i].before,target,sites[i].len))
            return "Cannot update System Properties";
    }
    return update_panel_strings(&n) ? NULL : "Incompatible System Properties RAM string resources";
}

#include "objects.inc"
