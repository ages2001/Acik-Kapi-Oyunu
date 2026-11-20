/* VMM is an LE driver inside a W3 bundle. The ordinary LE object and fixup
   tables describe the locked and initialization allocations independently. */
#include <stdlib.h>
#if defined(__WATCOMC__) && defined(__I86__)
#include <malloc.h>
#define VMM_FAR __far
#define vmm_alloc _fmalloc
#define vmm_free _ffree
#define vmm_copy _fmemcpy
#else
#define VMM_FAR
#define vmm_alloc malloc
#define vmm_free free
#define vmm_copy memcpy
#endif
typedef const Byte VMM_FAR *VmmReader;
typedef struct {
    long w3,member,le,objects,map,index,records,record_end,headers_end,data,boundary;
    unsigned pages,locked;
    Segment area[3];
    long shift[3];
} Vmm;
typedef struct { long at; unsigned length; } VmmWrite;
typedef struct { long at; Dword target; Byte type,object,seen; } VmmDrop;
typedef struct {
    VmmWrite writes[128];
    VmmDrop drops[64];
    unsigned count,dropped;
} VmmEdits;
static int read32(Ne *n,long at,long *v) {
    Byte b[4];
    if (!readat(n,at,b,4) || dword(b)>0x7fffffffUL) return 0;
    *v=(long)dword(b); return 1;
}
static int write32(Ne *n,long at,long v) {
    Byte b[4];setdword(b,(Dword)v);return writeat(n,at,b,4);
}
static int rule_read(FILE *r,Byte *b,unsigned count) { return fread(b,1,count,r)==count; }
static int vmm_header(Ne *n,Vmm *v) {
    Byte mz[64],w[16],le[156],o[48],page[4];
    unsigned i,count;
    long last,body;
    if (!readat(n,0,mz,64) || memcmp(mz,"MZ",2) || !read32(n,60,&v->w3) ||
        !readat(n,v->w3,w,16) || memcmp(w,"W3",2)) return 0;
    count=word(w+4);v->member=-1;
    if (!count || !inside(v->w3+16,16L*count,n->size)) return 0;
    for (i=0;i<count;++i) {
        long e=v->w3+16+16L*i;
        if (!readat(n,e,w,16)) return 0;
        if (!memcmp(w,"WIN386  ",8)) {
            if (v->member>=0 || dword(w+8)>(Dword)n->size) return 0;
            v->member=e;v->le=(long)dword(w+8);
        }
    }
    if (v->member<0 || !readat(n,v->le,le,sizeof(le)) || memcmp(le,"LE\0\0",4) ||
        dword(le+0x44)!=2 || dword(le+0x28)!=4096 || !dword(le+0x14) || dword(le+0x14)>128 ||
        !read32(n,v->member+12,&last) || !inside(v->le,last,n->size)) return 0;
    v->headers_end=v->le+last;
    v->objects=v->le+(long)dword(le+0x40);v->map=v->le+(long)dword(le+0x48);
    v->index=v->le+(long)dword(le+0x68);v->records=v->le+(long)dword(le+0x6c);
    v->record_end=v->le+(long)dword(le+0x70);v->data=(long)dword(le+0x80);
    v->pages=(unsigned)dword(le+0x14);
    if (v->objects<v->le || !inside(v->objects,48,v->map) || !readat(n,v->objects,o,48) ||
        !inside(v->map,4L*v->pages,v->index) || v->index+4L*(v->pages+1)!=v->records ||
        v->records>=v->record_end || v->record_end>v->headers_end || v->headers_end>v->data ||
        v->data>n->size || dword(o+8)!=0x2045 || dword(o+32)!=0x2015 ||
        dword(o+4)!=0 || dword(o+12)!=1 || !dword(o+16) || dword(o+16)>=v->pages) return 0;
    v->locked=(unsigned)dword(o+16);v->boundary=4096L*v->locked;
    if (dword(o+28)!=(Dword)v->boundary || dword(o+36)!=v->locked+1U ||
        dword(o+40)!=v->pages-v->locked || dword(le+0x84)!=v->locked) return 0;
    /* A contiguous run may be moved in the file. The growth transformation
       only handles runs whose in-object and physical page orders agree. */
    for (i=0;i<v->pages;++i) {
        if (!readat(n,v->map+4L*i,page,4) || page[0] || page[1] || page[2]!=i+1 || page[3]) return 0;
    }
    body=((long)word(mz+4)-1)*512+(word(mz+2)?word(mz+2):512);
    v->area[0].start=word(mz+8)*16L;v->area[0].len=body-v->area[0].start;
    v->area[1].start=v->data;v->area[1].len=(long)dword(o);
    v->area[2].start=v->data+v->boundary;v->area[2].len=(long)dword(o+24);
    return inside(v->area[0].start,v->area[0].len,v->w3) && v->area[1].len>0 &&
           v->area[1].len<=v->boundary && v->area[2].len>0 &&
           v->area[2].len<=4096L*(v->pages-v->locked) &&
           inside(v->area[2].start,v->area[2].len,n->size);
}
static int vmm_sections(Ne *n,Vmm *v,FILE *r,VmmEdits *ed) {
    char pattern[49];
    static const char hex[]="0123456789abcdef";
    Byte b[256],sum[32],expected[32];
    int blocks=fgetc(r),i,obj,len,j;
    long hit,pos;
    unsigned k,count,length;
    if (blocks<=0) return 0;
    for (i=0;i<blocks;++i) {
        if (!rule_read(r,b,6) || b[0]>2 || !b[5] || b[5]>16) return 0;
        obj=b[0];pos=(long)dword(b+1);len=b[5];
        if (!rule_read(r,b,len)) return 0;
        for (j=0;j<len;++j) {
            pattern[3*j]=hex[b[j]>>4];pattern[3*j+1]=hex[b[j]&15];pattern[3*j+2]=' ';
        }
        pattern[3*len-1]=0;
        if (!match(n,&v->area[obj],pattern,&hit) || hit<0) return 0;
        v->shift[obj]=hit-pos;
        if (!rule_read(r,b,2)) return 0;
        count=word(b);
        for (k=0;k<count;++k) {
            if (!rule_read(r,b,6) || !rule_read(r,expected,32)) return 0;
            pos=v->shift[obj]+(long)dword(b);length=word(b+4);
            if (!inside(pos,length,v->area[obj].len) ||
                !digest_at(n->f,v->area[obj].start+pos,length,sum) || memcmp(sum,expected,32)) return 0;
        }
        if (!rule_read(r,b,2)) return 0;
        count=word(b);
        for (k=0;k<count;++k) {
            if (!rule_read(r,b,6)) return 0;
            pos=v->shift[obj]+(long)dword(b);length=word(b+4);
            if (!inside(pos,length,v->area[obj].len)) return 0;
            if (obj) {
                if (ed->count>=sizeof(ed->writes)/sizeof(ed->writes[0])) return 0;
                ed->writes[ed->count].at=pos+(obj==2?v->boundary:0);
                ed->writes[ed->count++].length=length;
            }
            pos+=v->area[obj].start;
            while (length) {
                unsigned nbytes=length>sizeof(b)?sizeof(b):length;
                if (!rule_read(r,b,nbytes) || !writeat(n,pos,b,nbytes)) return 0;
                pos+=nbytes; length-=nbytes;
            }
        }
    }
    return 1;
}
/* Add a locked page ahead of initialization data without using translation
   storage or consuming any other bundled driver's bytes. */
static int vmm_add_page(Ne *n,Vmm *v) {
    static const unsigned relative_fields[]={0x58,0x5c,0x68,0x6c,0x70,0x78,0x30,0x38};
    static const unsigned file_fields[]={0x80,0x88,0x98};
    Byte b[8],page[4]={0,0,0,0};
    long pos,member,e,ent=v->map+4L*v->locked,cut=v->area[2].start;
    unsigned i,j;
    Word members;
    if (!inside(v->headers_end,8,v->data) || !readat(n,v->headers_end,b,8) ||
        memcmp(b,"\0\0\0\0\0\0\0\0",8) || !readword(n,v->w3+4,&members)) return 0;
    if (!transfer(n,v->records+8,v->records,v->headers_end-v->records) ||
        !transfer(n,ent+4,ent,v->index-ent)) return 0;
    for (i=v->locked;i<=v->pages;++i) {
        page[2]=(Byte)(i+1);
        if (!writeat(n,v->map+4L*i,page,4)) return 0;
    }
    for (i=0;i<sizeof(relative_fields)/sizeof(relative_fields[0]);++i) {
        e=v->le+relative_fields[i];
        if (!read32(n,e,&pos) || !write32(n,e,pos+(i>=3 && i<=5?8:4))) return 0;
    }
    if (!write32(n,v->le+0x14,v->pages+1) || !write32(n,v->le+0x84,v->locked+1) ||
        !write32(n,v->objects+16,v->locked+1) || !write32(n,v->objects+36,v->locked+2) ||
        !write32(n,v->objects+28,v->boundary+4096) ||
        !write32(n,v->member+12,v->headers_end-v->le+8)) return 0;
    for (i=0;i<members;++i) {
        e=v->w3+16+16L*i;
        if (!read32(n,e+8,&member) || !readat(n,member,b,4) || memcmp(b,"LE\0\0",4)) return 0;
        for (j=0;j<sizeof(file_fields)/sizeof(file_fields[0]);++j) {
            long at=member+file_fields[j];
            if (!read32(n,at,&pos) || pos>n->size ||
                (pos>=cut && !write32(n,at,pos+4096))) return 0;
        }
        if (member>=cut && !write32(n,e+8,member+4096)) return 0;
    }
    if (!insert(n,cut,4096)) return 0;
    v->index+=4;v->records+=8;v->record_end+=8;v->area[2].start+=4096;
    return 1;
}
static int vmm_source(VmmEdits *ed,Vmm *v,long at,unsigned type,unsigned obj,Dword dest) {
    unsigned i;
    for (i=0;i<ed->dropped;++i) {
        VmmDrop *d=&ed->drops[i];
        if (d->at!=at) continue;
        if (d->type!=type || d->object!=obj ||
            (obj==2 ? dest>=(Dword)v->area[2].len : d->target!=dest)) return -1;
        d->seen=1;return 0;
    }
    for (i=0;i<ed->count;++i) {
        VmmWrite *w=&ed->writes[i];
        if (w->at<at+4 && at<w->at+w->length) return -1;
    }
    return 1;
}
static int vmm_records(Ne *n,Vmm *v,VmmEdits *ed,const Byte VMM_FAR *raw,const long *idx,
                       long grow,long *added,unsigned additions,long state) {
    unsigned page,i;
    long out=v->records;
    Byte b[524],locals[510],ref[5];
    for (page=0;page<v->pages+(grow?1U:0U);++page) {
        if (!write32(n,v->index+4L*page,out-v->records)) return 0;
        if (!grow || page!=v->locked) {
            unsigned oldpage=page-(grow && page>v->locked);
            long at=idx[oldpage],end=idx[oldpage+1];
            while (at<end) {
                unsigned type,flags,tlen,num,k,keep=0,o;
                VmmReader target,source;
                if (!inside(at,3,end)) return 0;
                type=raw[at++];flags=raw[at++];tlen=flags?5:3;
                if (((type&~32U)!=7 && (type&~32U)!=8) || (flags && flags!=16)) return 0;
                if (type&32) {
                    num=raw[at++];
                    if (!num || !inside(at,tlen+2L*num,end)) return 0;
                    target=raw+at;source=target+tlen;at+=tlen+2L*num;
                } else {
                    if (!inside(at,2+tlen,end)) return 0;
                    num=1;source=raw+at;target=source+2;at+=tlen+2;
                }
                vmm_copy(ref,target,tlen);
                if (ref[0]<1 || ref[0]>2) return 0;
                for (k=0;k<num;++k) {
                    long local=(int16_t)(source[2*k]|((Word)source[2*k+1]<<8));
                    int action=vmm_source(ed,v,4096L*oldpage+local,type&~32U,ref[0],
                                          flags?dword(ref+1):word(ref+1));
                    if (action<0) return 0;
                    if (action) {setword(locals+2*keep,(Word)local);++keep;}
                }
                if (!keep) continue;
                b[0]=(Byte)((type&~32U)|(keep>1?32:0));b[1]=(Byte)flags;
                if (keep==1) {
                    memcpy(b+2,locals,2);memcpy(b+4,ref,tlen);o=4+tlen;
                } else {
                    b[2]=(Byte)keep;memcpy(b+3,ref,tlen);memcpy(b+3+tlen,locals,2*keep);
                    o=3+tlen+2*keep;
                }
                if (!inside(out,o,v->record_end) || !writeat(n,out,b,o)) return 0;
                out+=o;
            }
        }
        for (i=0;i<additions;++i) {
            if ((unsigned)(added[i]/4096)!=page) continue;
            b[0]=7;b[1]=(Byte)(i?16:0);setword(b+2,(Word)(added[i]%4096));b[4]=1;
            if (i) setdword(b+5,(Dword)state);else setword(b+5,0);
            if (!inside(out,i?9:7,v->record_end) || !writeat(n,out,b,i?9:7)) return 0;
            out+=i?9:7;
        }
    }
    for (i=0;i<ed->dropped;++i) if (!ed->drops[i].seen) return 0;
    return write32(n,v->index+4L*page,out-v->records) && zeros(n,out,v->record_end-out);
}
static int patch_vmm(FILE *stage,FILE *rules) {
    Vmm v;
    VmmEdits ed;
    Ne n;
    Byte b[28],code[512],data[128];
    Byte VMM_FAR *raw=NULL;
    long added[9],idx[129],start,grow,state,fix,store,entry,fatal,at,capacity;
    unsigned i,j,links,resume,service,size,slots;
    int ok=0;
    memset(&v,0,sizeof(v)); memset(&ed,0,sizeof(ed));
    if (!image_file(&n,stage) || !vmm_header(&n,&v) || !vmm_sections(&n,&v,rules,&ed) ||
        !rule_read(rules,b,2)) return 0;
    ed.dropped=word(b);
    if (ed.dropped>sizeof(ed.drops)/sizeof(ed.drops[0])) return 0;
    for (i=0;i<ed.dropped;++i) {
        VmmDrop *d=&ed.drops[i];
        if (!rule_read(rules,b,11) || (b[0]!=1 && b[0]!=2)) return 0;
        d->at=v.shift[b[0]]+(long)dword(b+1)+(b[0]==2?v.boundary:0);
        d->type=b[5];d->object=b[6];d->target=dword(b+7);
    }
    start=aligned(v.area[1].len,4);
    if (!rule_read(rules,b,1)) return 0;
    links=b[0];
    for (i=0;i<links;++i) {
        if (!rule_read(rules,b,6)) return 0;
        at=v.shift[1]+(long)dword(b);
        if (!inside(at,5,v.area[1].len) || !readat(&n,v.data+at,data,1) ||
            (data[0]!=0xe8 && data[0]!=0xe9) || word(b+4)>=sizeof(code) ||
            !write32(&n,v.data+at+1,start+word(b+4)-at-5)) return 0;
    }
    if (!rule_read(rules,b,24)) return 0;
    fix=v.shift[2]+(long)dword(b);store=v.shift[2]+(long)dword(b+4);
    entry=v.shift[1]+(long)dword(b+8);fatal=v.shift[2]+(long)dword(b+12);
    resume=word(b+16);service=word(b+18);size=word(b+20);slots=word(b+22);
    if (!size || size>sizeof(code) || !inside(resume,4,size) || !inside(service,4,size) ||
        slots>8 || !inside(fix,4,v.area[2].len) || !inside(store,4,v.area[2].len) ||
        !inside(entry,1,v.area[1].len) || !inside(fatal,6,v.area[2].len)) return 0;
    grow=start+size>v.boundary-4 ? 4096 : 0;state=v.boundary+grow-4;
    added[0]=v.boundary+grow+fix;
    for (i=0;i<slots;++i) {
        if (!rule_read(rules,b,2) || !inside(word(b),4,size)) return 0;
        added[i+1]=start+word(b);
    }
    if (!rule_read(rules,code,size) || !readat(&n,v.area[2].start+fatal,data,6) ||
        data[0]!=0xcd || data[1]!=0x20) return 0;
    memcpy(code+service,data+2,4);setdword(code+resume,(Dword)(entry-start-resume-4));
    for (i=0;i<slots;++i) setdword(code+(unsigned)(added[i+1]-start),(Dword)state);
    capacity=v.record_end-v.records;
    if (capacity<=0 || capacity>32767) return 0;
    for (i=0;i<=v.pages;++i) {
        if (!read32(&n,v.index+4L*i,&idx[i]) || idx[i]>capacity ||
            (i?idx[i]<idx[i-1]:idx[i]!=0)) return 0;
    }
    raw=vmm_alloc((unsigned)capacity);
    if (!raw) goto done;
    for (at=0;at<capacity;at+=j) {
        j=capacity-at>(long)sizeof(data)?sizeof(data):(unsigned)(capacity-at);
        if (!readat(&n,v.records+at,data,j)) goto done;
        vmm_copy(raw+(unsigned)at,data,j);
    }
    if (grow && !vmm_add_page(&n,&v)) goto done;
    for (at=v.area[1].len;at<state+4;at+=j) {
        j=state+4-at>(long)sizeof(data)?sizeof(data):(unsigned)(state+4-at);
        if (!readat(&n,v.data+at,data,j)) goto done;
        for (i=0;i<j;++i) if (data[i]) goto done;
    }
    if (!vmm_records(&n,&v,&ed,raw,idx,grow,added,slots+1,state) ||
        !write32(&n,v.objects,v.boundary+grow) || !write32(&n,v.area[2].start+store,state) ||
        !writeat(&n,v.data+start,code,size)) goto done;
    ok=1;
done:
    if (raw) vmm_free(raw);
    return ok;
}
