/* Early Windows binds multiple NE modules into one BIN. Segment and resource
   entries marked PRELOAD refer to the BIN; the others refer to its OVL. */
static int bound_menu_code(Ne *n, const Segment *s, int early, int required, long entry) {
    long at;
    Word allocation, recorded;
    unsigned old=early?28:30, compact=early?12:14;
    if (!match(n,s,"b8 42 00 50 b8 ?? ?? 50 9a ?? ?? ?? ?? 0b c0 74 ?? 50 50 50 9a ?? ?? ?? ?? 8b d8 c7 47 04 ?? ?? c7 47 02 ff ff 9a",&at))
        return 0;
    if (at<0) return !required;
    if (entry>=0 && (at<entry || at-entry>64)) return 0;
    if (!readword(n,s->start+at+5,&allocation) || !readword(n,s->start+at+30,&recorded) ||
        allocation!=recorded || (allocation!=old && allocation!=compact)) return 0;
    return writeword(n,s->start+at+5,compact) && writeword(n,s->start+at+30,compact);
}
static int bound_user(Ne *n,int early) {
    Ne m=*n;
    long next, field;
    Word entry;
    Segment s;
    while (!identifier(&m,relative(&m,0x26),"USER",1)) {
        next=16L*word(m.h+8);
        if (next<=m.base || !ne_header(&m,next,1)) return 0;
    }
    return segment(&m,export_entry(&m,151,&field),&s) && (s.flags&0x40) &&
           s.start && readword(&m,field,&entry) && entry<s.len &&
           bound_menu_code(&m,&s,early,1,entry);
}
static int grow_bound(Ne *n,const Segment *current,long oldsize,long newsize) {
    Ne m=*n;
    long cut=current->start+oldsize,growth=aligned(newsize-oldsize,16);
    int found_user=0;
    Byte marker[2],mz[6];
    if (newsize<oldsize || growth>LONG_MAX-n->size) return 0;
    for (;;) {
        long next=16L*word(m.h+8), at=relative(&m,0x24), end=relative(&m,0x26);
        Word sector,flags,shift,kind,count;
        unsigned i,j;
        if (identifier(&m,end,"USER",1)) found_user=1;
        for (i=0;i<m.count;++i) {
            long field=m.segments+8L*i;
            if (!readword(&m,field,&sector) || !readword(&m,field+4,&flags)) return 0;
            if ((flags&0x40) && 16L*sector>=cut &&
                (sector+growth/16>65535L || !writeword(&m,field,(Word)(sector+growth/16)))) return 0;
        }
        if (at!=end) {
            long unit;
            if (at>end || !readword(&m,at,&shift) || shift>15) return 0;
            unit=1L<<shift; at+=2;
            for (;;) {
                if (!inside(at,2,end) || !readword(&m,at,&kind)) return 0;
                if (!kind) break;
                if (!inside(at,8,end) || !readword(&m,at+2,&count)) return 0;
                at+=8;
                if (!inside(at,12L*count,end)) return 0;
                for (j=0;j<count;++j,at+=12) {
                    if (!readword(&m,at,&sector) || !readword(&m,at+4,&flags)) return 0;
                    if ((flags&0x40) && unit*sector>=cut && (growth%unit ||
                        sector+growth/unit>65535L || !writeword(&m,at,(Word)(sector+growth/unit)))) return 0;
                }
            }
        }
        if (next>=cut) {
            if ((next+growth)/16>65535L || !writeword(&m,m.base+8,(Word)((next+growth)/16))) return 0;
            if (m.base==n->base) setword(n->h+8,(Word)((next+growth)/16));
        }
        if (!next || next==n->size) break;
        if (next<=m.base || !readat(&m,next,marker,2)) return 0;
        if (memcmp(marker,"NE",2)) {
            if (marker[0] || marker[1]) return 0;
            break;
        }
        if (!ne_header(&m,next,1)) return 0;
    }
    if (!found_user || !readat(n,0,mz,6)) return 0;
    {
        long end=((long)word(mz+4)-1)*512+(word(mz+2)?word(mz+2):512);
        if (end<cut || end>n->size || end+growth>65535L*512) return 0;
        end+=growth; setword(mz+2,(Word)(end%512)); setword(mz+4,(Word)((end+511)/512));
    }
    return writeat(n,0,mz,6) && insert(n,cut,growth);
}
static int extra_entries(Ne *n, FILE *rules, int count) {
    unsigned j,first=1;
    Byte b[6],zero[10],inserted[10];
    long pos=relative(n,4), start=pos, end=pos+word(n->h+6), at;
    if (!count) return 1;
    if (count!=2) return 0;
    while (inside(pos,2,end) && readat(n,pos,b,2) && b[0]) {
        first+=b[0]; pos+=2L+b[0]*(b[1]==255?6:b[1]?3:0);
    }
    if (!inside(pos,1,end)) return 0;
    inserted[0]=(Byte)count; inserted[1]=1; inserted[8]=0;
    for (j=0;j<(unsigned)count;++j) {
        Word ord;
        if (fread(b,1,5,rules)!=5 || b[2]!=1) return 0;
        ord=word(b);
        if (ord<first) {
            Word offset;
            if (export_entry(n,ord,&at)!=1 || !readword(n,at,&offset) || offset!=word(b+3)) return 0;
        } else if (ord!=first+j) return 0;
        inserted[2+3*j]=1; memcpy(inserted+3+3*j,b+3,2);
        if (j+1==(unsigned)count && ord<first) return 1;
    }
    if (!readat(n,pos,zero,9)) return 0;
    for (j=0;j<9;++j) if (zero[j]) return 0;
    if (pos+9-start>65535L || !writeat(n,pos,inserted,9)) return 0;
    setword(n->h+6,(Word)(pos+9-start));
    return 1;
}
