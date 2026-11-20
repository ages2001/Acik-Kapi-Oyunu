#include "xoutcode.h"
/* Resolve the four symbols in the installed Windows/386 supervisor. */
static int bank_symbols(Ne *n, long start, long size, Dword *symbols) {
    static const char *names[]={"Cur_VMID","Cur_VM_Handle","Event_Chk","SoftInt2F"};
    Byte hdr[8];
    char name[80];
    unsigned j;
    if (!inside(start,size,n->size) || fseek(n->f,start,SEEK_SET)) return 0;
    while (size>0) {
        unsigned count=0;
        int c;
        if (size<9 || fread(hdr,1,8,n->f)!=8) return 0;
        size-=8;
        do {
            if (!size-- || (c=fgetc(n->f))==EOF) return 0;
            if (count+1<sizeof(name)) name[count++]=(char)c;
        } while (c);
        name[count]=0;
        for (j=0;j<4;++j) if (!strcmp(name,names[j])) {
            if (symbols[j+1]) return 0;
            symbols[j+1]=dword(hdr+4);
        }
    }
    return symbols[1] && symbols[2] && symbols[3] && symbols[4];
}
static int patch_xout(FILE *file) {
    Ne n;
    Byte h[60],descriptor[32],op[5],code[sizeof(bank_code)],actual[sizeof(bank_code)];
    Dword symbols[5]={0},base,va,memsz,rawsz,symsz,target;
    long table,total,data=-1,sym=-1,start,sympos,old,newlen,site,p,end;
    Segment body;
    unsigned i;
    if (!image_file(&n,file) || !readat(&n,0,h,sizeof(h)) || word(h)!=0x0206) return 0;
    table=(long)dword(h+52); total=(long)dword(h+56);
    if (table<32 || total<64 || total%32 || !inside(table,total,n.size)) return 0;
    for (p=table;p<table+total;p+=32) {
        if (!readat(&n,p,descriptor,32)) return 0;
        if (word(descriptor)==2) { if (data>=0) return 0; data=p; }
        if (word(descriptor)==3) { if (sym>=0) return 0; sym=p; }
    }
    if (!readat(&n,sym,descriptor,32)) return 0;
    sympos=(long)dword(descriptor+8); symsz=dword(descriptor+12);
    if (!bank_symbols(&n,sympos,(long)symsz,symbols) || !readat(&n,data,descriptor,32)) return 0;
    start=(long)dword(descriptor+8); rawsz=dword(descriptor+12); memsz=dword(descriptor+16);
    base=dword(descriptor+20);
    if (!inside(start,(long)rawsz,n.size) || rawsz>memsz || memsz>0x7fffffffUL ||
        sympos<start+(long)rawsz || base>0xffffffffUL-memsz ||
        symbols[4]<base || symbols[4]-base>=rawsz) return 0;
    va=symbols[4]; body.start=start+(long)(va-base); body.len=rawsz-(va-base);
    if (!match(&n,&body,"?? ?? ?? ?? ?? 8b 5d 1c 80 ff 43 0f 84 ?? ?? ?? ?? 66 81 fb 00 10",&site) ||
        site<0 || !readat(&n,body.start+site,op,5)) return 0;
    symbols[4]=va+(Dword)site+5;
    if (op[0]==0xe9) {
        target=symbols[4]+dword(op+1);
        if (target<base || !inside((long)(target-base),sizeof(code),(long)rawsz)) return 0;
    } else {
        if (op[0]!=0xb8 || dword(op+1)!=0x2f || memsz>0x7fffffffUL-16-sizeof(code) ||
            base+memsz>0xffffffffUL-16-sizeof(code) ||
            dword(h+8)!=rawsz || dword(h+12)!=memsz-rawsz) return 0;
        target=(base+memsz+15)&~15UL;
    }
    symbols[0]=target;
    memcpy(code,bank_code,sizeof(code));
    for (i=0;i<sizeof(bank_links)/sizeof(bank_links[0]);++i) {
        unsigned at=bank_links[i].off;
        Dword value=dword(code+at)+symbols[bank_links[i].target];
        if (bank_links[i].relative) value-=target+at;
        setdword(code+at,value);
    }
    if (op[0]==0xe9)
        return readat(&n,start+(long)(target-base),actual,sizeof(actual)) && !memcmp(actual,code,sizeof(code));
    old=start+(long)rawsz; newlen=(long)(target-base)+sizeof(code);
    if (!insert(&n,old,newlen-(long)rawsz) || !writeat(&n,start+(long)(target-base),code,sizeof(code)))
        return 0;
    for (p=table;p<table+total;p+=32) {
        if (!readat(&n,p,descriptor,32)) return 0;
        if (p==data) {setdword(descriptor+12,(Dword)newlen);setdword(descriptor+16,(Dword)newlen);}
        else {
            end=(long)dword(descriptor+8);
            if (end>=old) setdword(descriptor+8,(Dword)(end+newlen-rawsz));
            else if (dword(descriptor+12) && end+(long)dword(descriptor+12)>old) return 0;
        }
        if (!writeat(&n,p,descriptor,32)) return 0;
    }
    setdword(h+8,(Dword)newlen);setdword(h+12,0);
    op[0]=0xe9;setdword(op+1,target-symbols[4]);
    return writeat(&n,0,h,sizeof(h)) && writeat(&n,body.start+site,op,5);
}
