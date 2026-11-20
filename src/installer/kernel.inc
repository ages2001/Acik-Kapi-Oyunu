#include "pagercode.h"

static int retained_length(Ne *n,const Segment *s,Word value) {
    static const struct { const char *pattern; unsigned first,second; } patterns[]={
        {"33 d2 26 89 16 ?? ?? 89 16 ?? ?? b9 ?? ?? 83 3e ?? ?? 00 75 03 b9 ?? ?? 56 53 1e",12,22},
        {"33 d2 26 89 16 ?? ?? 89 16 ?? ?? b9 ?? ?? b9 ?? ?? fa 2e 8e 16 ?? ?? bc ?? ?? fb 0e 52 51 52 0e b8",12,15},
        {"1e 52 51 52 90 0e e8 ?? ?? 33 d2 b9 ?? ?? 0e 52 51 52 0e b8",12,0}
    };
    unsigned i;
    long pos,first=-1,second=-1;
    for (i=0;i<sizeof(patterns)/sizeof(patterns[0]);++i) {
        if (!match(n,s,patterns[i].pattern,&pos)) return 0;
        if (pos<0) continue;
        if (first>=0) return 0;
        first=pos+patterns[i].first;
        if (patterns[i].second) second=pos+patterns[i].second;
    }
    return first>=0 && writeword(n,s->start+first,value) &&
           (second<0 || writeword(n,s->start+second,value));
}
static int physical_count(Ne *n,const Segment *s,int installed,long helper,Byte *extra,unsigned *length) {
    static const Byte old[]={0x66,0xc1,0xe2,0x0c,0x79,3,0x66,0x33,0xd2};
    static const Byte stock[]={0x66,0x85,0xd2,0x78,0x0f,0x66,0xc1,0xe2,0x0c};
    static const Byte subtract[]={0x66,0x81,0xea,0,0,1,0,0x0f,0x87,3,0,0x66,0x33,0xd2};
    static const Byte epilogue[]={0x33,0xff,0xc9,0xc3};
    static const Byte clamp[]={0x66,0x85,0xd2,0x79,3,0x66,0x33,0xd2,0x66,0xc1,0xe2,0x0c,0xe9,0,0};
    long borrow,scale;
    Byte b[16],replacement[9],branch=0x73;
    if (!match(n,s,"66 81 ea 00 00 01 00 ?? 03 66 33 d2 66 8b c2",&borrow) || borrow<0 ||
        !match(n,s,"66 03 56 ec 66 2b 56 dc 66 8b 46 d0 ?? ?? ?? ?? ?? ?? ?? ?? ??",&scale) ||
        scale<0 || !readat(n,s->start+borrow+7,b,1) || b[0]!=(installed?0x73:0x79)) return 0;
    scale+=12;
    if (!readat(n,s->start+scale+9,b,sizeof(subtract))) return 0;
    if (!memcmp(b,subtract,sizeof(subtract))) memcpy(replacement,stock,9);
    else if (!memcmp(b,epilogue,sizeof(epilogue))) {
        memcpy(extra,clamp,sizeof(clamp)); *length=sizeof(clamp);
        setword(extra+13,(Word)(scale+9-helper-sizeof(clamp)));
        memset(replacement,0x90,9); replacement[0]=0xe9;setword(replacement+1,(Word)(helper-scale-3));
        if (installed && (!readat(n,s->start+helper,b,*length) || memcmp(b,extra,*length))) return 0;
    } else return 0;
    if (!readat(n,s->start+scale,b,9) || memcmp(b,installed?replacement:old,9)) return 0;
    return installed || (writeat(n,s->start+borrow+7,&branch,1) && writeat(n,s->start+scale,replacement,9));
}
static int kernel_service(Ne *n,int wide) {
    static const unsigned apis[]={15,16,17,18,19,20,21,22,23,24,111,112,191,192,197,198};
    struct {long field;Word previous,position;unsigned ordinal;} hooks[16];
    Byte tail[sizeof(page_dispatch)],stub[10],expected[sizeof(page_dispatch)];
    Segment s;
    unsigned count=0,i,stubs=0,index,j,extra_length=0;
    Byte extra[15];
    long start,at,dispatch,newlen,rel,end;
    Word offset,dest;
    int installed;
    index=export_segment(n,15);
    if (!segment(n,index,&s) || !s.start || (s.flags&9) || !(s.flags&0x100)) return 0;
    for (i=0;i<sizeof(apis)/sizeof(apis[0]);++i) {
        if (export_entry(n,apis[i],&at)!=(int)index) {
            if (apis[i]<=22) return 0;
            continue;
        }
        if (!readword(n,at,&offset) || offset>=s.len) return 0;
        hooks[count].field=at;hooks[count].ordinal=apis[i];hooks[count].previous=offset;
        hooks[count++].position=stubs; stubs+=apis[i]<128 ? 8 : 9;
    }
    if (!readat(n,s.start+hooks[0].previous,stub,8)) return 0;
    installed=stub[0]==0x68 && stub[3]==0x6a && stub[4]==15 && stub[5]==0xe9;
    start=installed ? hooks[0].previous : s.len;
    dispatch=start+stubs;newlen=dispatch+sizeof(tail);
    if (wide && !physical_count(n,&s,installed,newlen,extra,&extra_length)) return 0;
    newlen+=extra_length;
    if (newlen>65535 || (installed && newlen!=s.len)) return 0;
    memcpy(tail,page_dispatch,sizeof(tail));setword(tail+page_operand,(Word)(dispatch+page_name));
    if (installed && (!readat(n,s.start+dispatch,expected,sizeof(expected)) ||
                      memcmp(tail,expected,sizeof(tail)))) return 0;
    if (!installed) {
        rel=2+8L*(s.fixups+2L);
        if (s.fixups>65533U || !retained_length(n,&s,(Word)newlen) ||
            !stage_segment(n,&s,2,newlen,rel,(Word)newlen,&at) ||
            !writeat(n,s.start+dispatch,tail,sizeof(tail)) ||
            (extra_length && !writeat(n,s.start+dispatch+sizeof(tail),extra,extra_length)) ||
            !writeword(n,s.start+newlen,s.fixups+2)) return 0;
        end=s.start+newlen+2+8L*s.fixups;
        if (!add_fixup(n,&end,3,0,(Word)(dispatch+page_mod),255,47) ||
            !add_fixup(n,&end,3,0,(Word)(dispatch+page_proc),255,50)) return 0;
    }
    for (i=0;i<count;++i) {
        at=start+hooks[i].position;
        if (installed) {
            if ((long)hooks[i].previous!=at || !readword(n,s.start+at+1,&dest) || dest>=start) return 0;
        } else dest=hooks[i].previous;
        stub[0]=0x68;setword(stub+1,dest);j=3;
        if (hooks[i].ordinal<128) {stub[j++]=0x6a;stub[j++]=(Byte)hooks[i].ordinal;}
        else {stub[j++]=0x68;setword(stub+j,hooks[i].ordinal);j+=2;}
        stub[j++]=0xe9;setword(stub+j,(Word)(dispatch-at-(j+2)));j+=2;
        if (installed) {
            if (!readat(n,s.start+at,expected,j) || memcmp(expected,stub,j)) return 0;
        } else if (!writeat(n,s.start+at,stub,j) || !writeword(n,hooks[i].field,(Word)at)) return 0;
    }
    return 1;
}
