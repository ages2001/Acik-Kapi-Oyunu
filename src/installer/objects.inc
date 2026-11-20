/* Private NE and W3 object transformations used by the structural reader. */
static int rule_string(FILE *rules, char *name, unsigned capacity) {
    int size=fgetc(rules);
    if (size<0 || (unsigned)size>=capacity || fread(name,1,size,rules)!=(unsigned)size) return 0;
    name[size]=0;
    return 1;
}
static long disk_length(Ne *n, const Segment *s) {
    (void)n;
    return s->len+((s->flags & 0x100) ? 2+8L*s->fixups : 0);
}
/* Return the actual on-disk extent for a member of the embedded WIN386 LE driver. */
static int w3_extent(Ne *n, unsigned key, long *start, long *length) {
    Byte mz[64], h[152], d[16], page[4];
    long w3,le=-1,table,va;
    Dword raw, size;
    unsigned total,i;
    if (!readat(n,0,mz,64) || memcmp(mz,"MZ",2)) return 0;
    w3=(long)dword(mz+60);
    if (!readat(n,w3,h,16) || memcmp(h,"W3",2) || !word(h+4)) return 0;
    total=word(h+4);
    if (!inside(w3+16,16L*total,n->size)) return 0;
    for (i=0;i<total;++i) {
        if (!readat(n,w3+16+16L*i,d,16)) return 0;
        if (!memcmp(d,"WIN386  ",8)) {
            if (le>=0) return 0;
            le=(long)dword(d+8);
        }
    }
    if (!readat(n,le,h,sizeof(h)) || memcmp(h,"LE\0\0",4)) return 0;
    if (key==0xfffd) {
        long end=((long)word(mz+4)-1)*512+(word(mz+2) ? word(mz+2) : 512);
        *start=(long)word(mz+8)*16; *length=end-*start;
    } else if (key==0xfffe) {
        if (dword(h+0x44)>65535 || dword(h+0x40)>(Dword)(n->size-le)) return 0;
        *start=le+(long)dword(h+0x40); *length=24L*(long)dword(h+0x44);
    } else if (key==0xffff) {
        if (dword(h+0x70)>(Dword)(n->size-le) || dword(h+0x68)>dword(h+0x70)) return 0;
        *start=le+(long)dword(h+0x68); *length=(long)(dword(h+0x70)-dword(h+0x68));
    } else {
        if (!key || key>dword(h+0x14) || dword(h+0x48)>(Dword)(n->size-le)) return 0;
        table=le+(long)dword(h+0x48);
        if (!inside(table,4L*key,n->size) || !readat(n,table+4L*(key-1),page,4) || page[3]) return 0;
        raw=((Dword)page[0]<<16)|((Dword)page[1]<<8)|page[2];
        va=(long)dword(h+0x80); size=dword(h+0x28);
        if (va<=0 || va>n->size || !raw || !size || size>(Dword)LONG_MAX ||
            raw-1>(Dword)(n->size-va)/size) return 0;
        *start=va+(long)((raw-1)*size); *length=(long)size;
    }
    return *length>0 && inside(*start,*length,n->size);
}

/* A self-loading Win16 kernel must keep its startup segment at its original
   address. Widen the segment in place and rebase the downstream file offsets. */
static int grow_kernel(Ne *n, const Segment *current, long oldsize, long newsize) {
    long cut=current->start+oldsize, res=relative(n,0x24), rn=relative(n,0x26);
    long unit=1,growth,at,nr;
    unsigned i,j;
    Word shift,count,type,sector;
    Byte mz[6];
    if (res>rn || newsize<oldsize) return 0;
    if (res!=rn) {
        if (!readword(n,res,&shift) || shift>15) return 0;
        unit=1L<<shift;
    }
    growth=aligned(newsize-oldsize,unit>n->unit ? unit : n->unit);
    if (growth>LONG_MAX-n->size || !inside(cut,0,n->size)) return 0;
    for (i=0;i<n->count;++i) {
        Segment s;
        if (!segment(n,i+1,&s)) return 0;
        if (s.start>=cut) {
            if ((s.start+growth)/n->unit>65535L ||
                !writeword(n,s.entry,(Word)((s.start+growth)/n->unit))) return 0;
        } else if (s.start && s.entry!=current->entry && s.start+disk_length(n,&s)>cut) return 0;
    }
    at=res+2;
    while (res!=rn) {
        if (!inside(at,2,rn) || !readword(n,at,&type)) return 0;
        if (!type) break;
        if (!inside(at,8,rn) || !readword(n,at+2,&count)) return 0;
        at+=8;
        if (!inside(at,12L*count,rn)) return 0;
        for (j=0;j<count;++j,at+=12) {
            if (!readword(n,at,&sector)) return 0;
            if ((long)sector*unit>=cut &&
                (sector+growth/unit>65535L || !writeword(n,at,(Word)(sector+growth/unit)))) return 0;
        }
    }
    nr=(long)dword(n->h+0x2c);
    if (nr>=cut) setdword(n->h+0x2c,(Dword)(nr+growth));
    if (n->h[0x37]&8) {
        long first=(long)word(n->h+0x38)*n->unit, pages=word(n->h+0x3a);
        if (first>=cut && (first+growth)/n->unit<=65535L) setword(n->h+0x38,(Word)((first+growth)/n->unit));
        else if (first<cut && first+pages*n->unit>=cut && pages+growth/n->unit<=65535L)
            setword(n->h+0x3a,(Word)(pages+growth/n->unit));
        else if (first>=cut) return 0;
    }
    if (!readat(n,0,mz,6)) return 0;
    at=((long)word(mz+4)-1)*512+(word(mz+2) ? word(mz+2) : 512);
    if (at<cut || at>n->size || at+growth>65535L*512) return 0;
    at+=growth; setword(mz+2,(Word)(at%512)); setword(mz+4,(Word)((at+511)/512));
    return writeat(n,0,mz,6) && writeat(n,n->base,n->h,64) && insert(n,cut,growth);
}

#include "bound.inc"
#include "xout.inc"

static int stage_segment(Ne *n, Segment *s, unsigned mode, long len, long rel,
                         Word minimum, long *where) {
    long oldrel=disk_length(n,s)-s->len, total=len+rel, start=s->start;
    long keep=rel<oldrel ? rel : oldrel;
    Byte field[2];
    if (len<=0 || len>65536L || rel<0 || rel>0x80002L || n->size>LONG_MAX-0x100000L) return 0;
    if (mode==0) {
        if (len!=s->len || rel!=oldrel) return 0;
    } else if (mode==1) {
        start=aligned(n->size,n->unit);
        if (!zeros(n,n->size,start-n->size) || !zeros(n,start,total) ||
            !transfer(n,start,s->start,s->len<len?s->len:len) ||
            !transfer(n,start+len,s->start+s->len,keep)) return 0;
    } else if (mode==2 || mode==3) {
        if (len<s->len || !(mode==3 ? grow_bound(n,s,s->len+oldrel,total) :
                                      grow_kernel(n,s,s->len+oldrel,total)) ||
            !transfer(n,start+len,start+s->len,keep) ||
            !zeros(n,start+s->len,len-s->len) || !zeros(n,start+len+keep,rel-keep)) return 0;
    } else return 0;
    if (!write_segment(n,s,start,len)) return 0;
    setword(field,minimum);
    if (!writeat(n,s->entry+6,field,2)) return 0;
    *where=start;
    return 1;
}

#include "kernel.inc"
#include "vmm.inc"

/* This runs only on a private stage. A caller trying another ABI must start
   with a fresh copy. Every emitted code or fixup block is verified on disk. */
int patch_objects(FILE *stage, FILE *rules) {
    Ne n;
    int type=fgetc(rules), units, imports, clear_fast=0, i,j, already;
    char name[32], dep[32];
    Byte fields[16], sums[64], actual[32], bytes[256];
    if (type=='O') {
        Segment all;
        int early=fgetc(rules), required=fgetc(rules);
        if (early<0 || required<0 || !image_file(&n,stage)) return 0;
        all.start=0; all.len=n.size;
        return bound_menu_code(&n,&all,early,required,-1L);
    }
    if (type=='V') return patch_vmm(stage,rules);
    if (type=='X') return patch_xout(stage);
    if (type=='K') {
        int wide=fgetc(rules);
        return wide>=0 && open_ne(&n,stage,"KERNEL") && kernel_service(&n,wide);
    }
    if (type=='N' || type=='B') {
        if (!rule_string(rules,name,sizeof(name)) || !module_file(&n,stage,name,type=='B') ||
            fread(fields,1,4,rules)!=4 || word(fields)!=word(n.h+14)) return 0;
        clear_fast=fields[2]; imports=fields[3];
        if (n.imports!=(unsigned)imports) return 0;
        for (i=0;i<imports;++i)
            if (!rule_string(rules,dep,sizeof(dep)) || module_index(&n,dep)!=i+1) return 0;
    } else if (type=='W') {
        memset(&n,0,sizeof(n)); n.f=stage;
        if (fseek(stage,0,SEEK_END) || (n.size=ftell(stage))<0) return 0;
    } else return 0;
    units=fgetc(rules);
    if (units<=0) return 0;
    for (i=0;i<units;++i) {
        Word key, edits, mode=0, minimum=0, newmin=0;
        long start,size,len=0,rel=0;
        Segment s;
        if (fread(fields,1,2,rules)!=2) return 0;
        key=word(fields);
        if (type=='N' || type=='B') {
            int idx=key ? export_segment(&n,key) : word(n.h+14);
            if (fread(fields,1,13,rules)!=13 || !segment(&n,idx,&s) || !s.start ||
                s.flags!=word(fields+1)) return 0;
            mode=fields[0]; minimum=word(fields+3); newmin=word(fields+5);
            len=word(fields+7); if (!len) len=65536L;
            rel=(long)dword(fields+9);
            start=s.start; size=disk_length(&n,&s);
        } else if (!w3_extent(&n,key,&start,&size)) return 0;
        if (fread(sums,1,64,rules)!=64 || !digest_at(stage,start,size,actual)) return 0;
        already=!memcmp(actual,sums+32,32);
        if (!already && memcmp(actual,sums,32)) return 0;
        if (type=='N' || type=='B') {
            if ((Word)s.minimum!=(already ? newmin : minimum)) return 0;
            if (!already && !stage_segment(&n,&s,mode,len,rel,newmin,&start)) return 0;
            size=len+rel;
        }
        if (fread(fields,1,2,rules)!=2) return 0;
        edits=word(fields);
        for (j=0;j<edits;++j) {
            long pos;
            unsigned left,part;
            if (fread(fields,1,6,rules)!=6) return 0;
            pos=(long)dword(fields); left=word(fields+4);
            if (!inside(pos,left,size)) return 0;
            while (left) {
                part=left>sizeof(bytes)?sizeof(bytes):left;
                if (fread(bytes,1,part,rules)!=part || (!already && !writeat(&n,start+pos,bytes,part)))
                    return 0;
                left-=part; pos+=part;
            }
        }
        if (!already && (!digest_at(stage,start,size,actual) || memcmp(actual,sums+32,32))) return 0;
    }
    if (type=='N' || type=='B') {
        Word exports;
        if (fread(fields,1,2,rules)!=2) return 0;
        exports=word(fields);
        for (j=0;j<exports;++j) {
            long at;
            Word cur;
            if (fread(fields,1,6,rules)!=6 || !export_entry(&n,word(fields),&at) ||
                !readword(&n,at,&cur) || (cur!=word(fields+2) && cur!=word(fields+4)) ||
                !writeword(&n,at,word(fields+4))) return 0;
        }
        if (type=='B') {
            int early=fgetc(rules), count=fgetc(rules);
            if (early<0 || count<0 || !extra_entries(&n,rules,count) || !bound_user(&n,early))
                return 0;
        }
        if (clear_fast) { n.h[0x37]&=~8; memset(n.h+0x38,0,4); }
        if (!writeat(&n,n.base,n.h,64)) return 0;
    }
    return 1;
}
