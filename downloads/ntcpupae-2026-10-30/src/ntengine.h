/* ntengine.h - pure-C NT kernel patch engine (CPU clamp / 0x9A / PAE).
 * Operates on globals gbuf/gsz. Needs: BYTE,WORD,DWORD, memcpy,
 * VirtualAlloc, MEM_COMMIT, MEM_RESERVE, PAGE_READWRITE from the includer. */
#ifndef NTENGINE_H
#define NTENGINE_H
/* ======================= PE engine (on gbuf/gsz) ======================= */
static BYTE* gbuf; static DWORD gsz;
static DWORD rd32(long o){ return gbuf[o]|(gbuf[o+1]<<8)|(gbuf[o+2]<<16)|((DWORD)gbuf[o+3]<<24); }
static void  wr32(long o,DWORD v){ gbuf[o]=v&0xff;gbuf[o+1]=(v>>8)&0xff;gbuf[o+2]=(v>>16)&0xff;gbuf[o+3]=(v>>24)&0xff; }
static DWORD pe_off(void){ return rd32(0x3c); }
static int   num_secs(void){ DWORD pe=pe_off(); return gbuf[pe+6]|(gbuf[pe+7]<<8); }
static long  sec_tab(void){ DWORD pe=pe_off(); DWORD optsz=gbuf[pe+0x14]|(gbuf[pe+0x15]<<8); return pe+0x18+optsz; }
static DWORD image_base(void){ return rd32(pe_off()+0x18+0x1c); }
static long rva2fo(DWORD rva){
    long st=sec_tab(); int n=num_secs();
    for(int i=0;i<n;i++){ long s=st+i*40;
        DWORD va=rd32(s+12), vsz=rd32(s+8), rawp=rd32(s+20), rsz=rd32(s+16);
        DWORD span=vsz>rsz?vsz:rsz;
        if(rva>=va && rva<va+span){ DWORD off=rva-va; if(off<rsz) return rawp+off; return -1; }
    }
    return -1;
}
static DWORD fo2va(long fo){
    long st=sec_tab(); int n=num_secs(); DWORD ib=image_base();
    for(int i=0;i<n;i++){ long s=st+i*40;
        DWORD va=rd32(s+12), rawp=rd32(s+20), rsz=rd32(s+16);
        if(fo>=(long)rawp && fo<(long)(rawp+rsz)) return ib+va+(DWORD)(fo-rawp);
    }
    return 0;
}
static DWORD export_va(const char* want){
    DWORD pe=pe_off();
    DWORD exprva=rd32(pe+0x18+0x60);
    if(!exprva) return 0;
    long e=rva2fo(exprva); if(e<0) return 0;
    DWORD nNames=rd32(e+0x18);
    DWORD aNames=rd32(e+0x20), aOrd=rd32(e+0x24), aFunc=rd32(e+0x1c);
    long foNames=rva2fo(aNames), foOrd=rva2fo(aOrd), foFunc=rva2fo(aFunc);
    if(foNames<0||foOrd<0||foFunc<0) return 0;
    for(DWORD i=0;i<nNames;i++){
        DWORD nameRva=rd32(foNames+i*4); long fn=rva2fo(nameRva); if(fn<0) continue;
        const char* s=(const char*)(gbuf+fn); int k=0; while(want[k]&&s[k]&&want[k]==s[k])k++;
        if(want[k]==0&&s[k]==0){
            WORD ord=gbuf[foOrd+i*2]|(gbuf[foOrd+i*2+1]<<8);
            DWORD frva=rd32(foFunc+ord*4);
            return image_base()+frva;
        }
    }
    return 0;
}
static int range_referenced(DWORD lo,DWORD hi); /* fwd: tells free padding from referenced data/code */
/* A zero-run in an executable, non-discardable section that NO code references. The reference test is
 * essential: the NT 3.5x/4.0 MPS HAL keeps its 16-bit AP-startup real-mode trampoline as a large zero-
 * padded block inside .text, and that block IS referenced (the copy routine loads a pointer to its end).
 * Treating it as a free cave overwrites code the APs execute in real mode -> STOP 0x0000000A. Requiring
 * the run to be unreferenced skips the trampoline (and any jump table / data) and keeps only true slack. */
/* Caves already carved in the current file; find_cave() records each hand-out and refuses to
 * overlap them, so two carves in one zero-run (the KeBugCheckEx 0x9A stub AND the PFN guard, both
 * in .text) never collide - a prior stub ending in 'jmp rel32' to a near target has a 0x00 last
 * byte that a naive scan would start the next cave on, silently corrupting it. */
#define MAXUSED 16
static DWORD used_lo[MAXUSED], used_hi[MAXUSED]; static int n_used=0;
static void mark_used(long fo,long len){ if(n_used<MAXUSED){ used_lo[n_used]=(DWORD)fo; used_hi[n_used]=(DWORD)(fo+len); n_used++; } }
static long find_cave(long need){
    long st=sec_tab(); int n=num_secs();
    for(int i=0;i<n;i++){ long s=st+i*40;
        DWORD ch=rd32(s+36);
        if(!(ch&0x20000000)) continue;          /* executable only */
        if(ch&0x02000000) continue;             /* skip discardable */
        DWORD vsz=rd32(s+8), rawp=rd32(s+20), rsz=rd32(s+16), va=rd32(s+12);
        DWORD lim=(vsz&&vsz<rsz)?vsz:rsz;
        DWORD o=0;
        while(o<lim){
            if(gbuf[rawp+o]==0){
                DWORD k=o; while(k<lim&&gbuf[rawp+k]==0) k++;   /* [o,k) all zero */
                /* Carve around already-used caves, preserving the ORIGINAL whole-(sub)segment
                 * reference test: n_used==0 -> byte-for-byte the old behavior (so the HAL path
                 * still falls through to grow_last_section_cave as before). */
                { DWORD seg=rawp+o, end=rawp+k;
                  while(seg+(DWORD)need<=end){
                    int jumped=0;
                    for(int u=0;u<n_used;u++) if(used_lo[u]<=seg && seg<used_hi[u]){ seg=used_hi[u]; jumped=1; break; }
                    if(jumped) continue;
                    DWORD segend=end;
                    for(int u=0;u<n_used;u++) if(used_lo[u]>seg && used_lo[u]<segend) segend=used_lo[u];
                    if(segend-seg>=(DWORD)need){
                        DWORD vlo=image_base()+va+(seg-rawp), vhi=image_base()+va+(segend-rawp);
                        if(!range_referenced(vlo,vhi)){ mark_used((long)seg,need); return (long)seg; }
                    }
                    seg=segend;
                  }
                }
                o=k;
            } else o++;
        }
    }
    return -1;
}
static DWORD alignup(DWORD x,DWORD a){ return (x+a-1)/a*a; }
/* In-image cave that does NOT add a section: use the zero slack between a code
 * section's VirtualSize and its raw/next-section boundary, growing VirtualSize so
 * the bytes get mapped. Keeps section count + SizeOfImage identical to the stock
 * image, so old osloaders (NT 3.5x) that reject an appended section still load it. */
static long find_slack_cave(long need){
    long st=sec_tab(); int n=num_secs();
    for(int i=0;i<n;i++){ long s=st+i*40;
        DWORD ch=rd32(s+36);
        if(!(ch&0x20000000)) continue;       /* executable only */
        if(ch&0x02000000) continue;          /* skip discardable */
        DWORD vs=rd32(s+8), va=rd32(s+12), rs=rd32(s+16), rp=rd32(s+20);
        if(vs==0 || rs<=vs) continue;        /* need raw slack past VirtualSize */
        DWORD nxt=0xFFFFFFFFu;               /* next section VA (virtual bound) */
        for(int j=0;j<n;j++){ DWORD v=rd32(st+j*40+12); if(v>va && v<nxt) nxt=v; }
        if(nxt==0xFFFFFFFFu) nxt=va+rs;
        DWORD virt_gap=nxt-(va+vs), raw_slack=rs-vs;
        DWORD usable=raw_slack<virt_gap?raw_slack:virt_gap;
        if(usable<(DWORD)need) continue;
        long fo=(long)(rp+vs);
        int ok=1; for(long k=0;k<need;k++){ if(gbuf[fo+k]!=0){ ok=0; break; } }
        if(!ok) continue;
        wr32(s+8, vs+(DWORD)need);           /* grow VirtualSize so the cave maps */
        return fo;
    }
    return -1;
}
static long add_section_cave(DWORD need){
    DWORD pe=pe_off(); WORD nsec=gbuf[pe+6]|(gbuf[pe+7]<<8);
    long st=sec_tab();
    DWORD soh=rd32(pe+0x18+0x3c);
    long newhdr=st+(long)nsec*40;
    if((DWORD)(newhdr+40)>soh) return -1;
    DWORD secA=rd32(pe+0x18+0x20), fileA=rd32(pe+0x18+0x24);
    if(!secA||!fileA) return -1;
    long last=st+(long)(nsec-1)*40;
    DWORD lva=rd32(last+12), lvs=rd32(last+8), lrs=rd32(last+16);
    DWORD newva=alignup(lva+(lvs?lvs:lrs),secA);
    DWORD pad=(fileA-((DWORD)gsz%fileA))%fileA;
    DWORD newraw=(DWORD)gsz+pad;
    DWORD rsize=alignup(need,fileA);
    DWORD add=pad+rsize;
    BYTE* nb=(BYTE*)VirtualAlloc(0,gsz+add,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!nb) return -1;
    memcpy(nb,gbuf,gsz);
    for(DWORD i=gsz;i<gsz+add;i++) nb[i]=0;
    gbuf=nb;
    long h=newhdr;
    const char nm[8]={'.','c','l','i','f','t',0,0};
    for(int i=0;i<8;i++) gbuf[h+i]=nm[i];
    wr32(h+8,need); wr32(h+12,newva); wr32(h+16,rsize); wr32(h+20,newraw);
    wr32(h+24,0); wr32(h+28,0); wr32(h+32,0); wr32(h+36,0x60000020);
    gbuf[pe+6]=(nsec+1)&0xff; gbuf[pe+7]=((nsec+1)>>8)&0xff;
    wr32(pe+0x18+0x38, alignup(newva+need,secA));
    gsz+=add;
    return (long)newraw;
}
/* Grow the file's physically-last section in place to hold the cave, then mark it present + R/W/X and
 * non-discardable. Needs no free section-header slot (unlike add_section_cave) and no end-of-.text slack
 * (unlike find_slack_cave), so it is the reliable home when a HAL's .text is fully packed and its .data
 * holds only live globals (NT 3.51). The appended bytes sit after any trailing debug blob; since every
 * section here maps raw==RVA, fo2va resolves the cave once the section's raw/virtual sizes are grown. */
static long grow_last_section_cave(DWORD need){
    DWORD pe=pe_off(); int nsec=num_secs(); long st=sec_tab();
    DWORD secA=rd32(pe+0x18+0x20), fileA=rd32(pe+0x18+0x24);
    if(!secA||!fileA) return -1;
    long last=-1; DWORD lastrp=0;
    for(int i=0;i<nsec;i++){ long s=st+i*40; DWORD rp=rd32(s+20); if(last<0||rp>=lastrp){ lastrp=rp; last=s; } }
    if(last<0) return -1;
    DWORD lrp=rd32(last+20), lva=rd32(last+12), lch=rd32(last+36);
    long cave_fo=(long)gsz;                    /* append at true EOF (past any trailing debug data) */
    DWORD addr=alignup(need,fileA);
    BYTE* nb=(BYTE*)VirtualAlloc(0,gsz+addr,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!nb) return -1;
    memcpy(nb,gbuf,gsz); for(DWORD i=gsz;i<gsz+addr;i++) nb[i]=0; gbuf=nb;
    DWORD neweof=gsz+addr;
    wr32(last+16,neweof-lrp);                   /* SizeOfRawData covers start..new EOF */
    DWORD new_vs=alignup(neweof-lva,secA);
    wr32(last+8,new_vs);                        /* VirtualSize maps the cave */
    DWORD ch=(lch & ~0x02000000u) | 0x20000000u|0x40000000u|0x80000000u|0x00000040u; /* -DISCARD +X+R+W+init */
    wr32(last+36,ch);
    DWORD newimg=alignup(lva+new_vs,secA);
    if(newimg>rd32(pe+0x18+0x38)) wr32(pe+0x18+0x38,newimg);
    gsz=neweof;
    return cave_fo;
}
/* Neutralise every base-relocation fixup whose target RVA falls in [rva, rva+len). When we overwrite an
 * instruction that had an absolute operand (e.g. the MP-parser finalize 'mov [count],eax', whose operand
 * carries a HIGHLOW fixup) with our 5-byte relative jmp, the stale fixup would otherwise be applied by the
 * loader to the middle of our jmp's rel32 -- shifting the jump target by the image's relocation delta and
 * sending it into unmapped space (STOP 0x0000000A at the garbled target). Setting the fixup type nibble to
 * 0 (IMAGE_REL_BASED_ABSOLUTE) makes the loader skip it. HALs that load at their preferred base never
 * applied the fixup, which is why this only bites once the image is relocated (NT 3.51/4.0). */
static int reloc_zap(DWORD rva,DWORD len){
    DWORD pe=pe_off();
    DWORD rr=rd32(pe+0x18+0x60+5*8), rs=rd32(pe+0x18+0x60+5*8+4);
    if(!rr||!rs) return 0;
    long base=rva2fo(rr); if(base<0) return 0;
    long off=base; long end=base+(long)rs; int n=0;
    while(off+8<=end){
        DWORD pg=rd32(off), blk=rd32(off+4);
        if(blk<8||blk>0x4000) break;
        int cnt=(int)(blk-8)/2;
        for(int i=0;i<cnt;i++){
            long eo=off+8+i*2; WORD e=gbuf[eo]|(gbuf[eo+1]<<8);
            int typ=e>>12; DWORD tgt=pg+(e&0xfff);
            if(typ!=0 && tgt>=rva && tgt<rva+len){ gbuf[eo+1]&=0x0f; n++; }  /* type nibble -> 0 */
        }
        off+=blk;
    }
    return n;
}
static DWORD pe_checksum_fix(void){
    DWORD pe=pe_off(); long c=pe+0x18+0x40;
    gbuf[c]=gbuf[c+1]=gbuf[c+2]=gbuf[c+3]=0;
    DWORD sum=0;
    for(long i=0;i+1<(long)gsz;i+=2){ sum+=gbuf[i]|(gbuf[i+1]<<8); sum=(sum&0xffff)+(sum>>16); }
    if(gsz&1){ sum+=gbuf[gsz-1]; sum=(sum&0xffff)+(sum>>16); }
    sum=(sum&0xffff)+(sum>>16);
    DWORD cks=sum+gsz;
    gbuf[c]=cks&0xff; gbuf[c+1]=(cks>>8)&0xff; gbuf[c+2]=(cks>>16)&0xff; gbuf[c+3]=(cks>>24)&0xff;
    return cks;
}

/* ---------- CPU clamp (jbeByte = 0x76 to find unpatched, 0x90 to detect patched) ---------- */
static long find_s1(DWORD* Xout, BYTE jbe){
    for(long i=0;i+10<=(long)gsz;i++){
        if(gbuf[i]==0xC7&&gbuf[i+1]==0x05&&gbuf[i+6]==0x20&&gbuf[i+7]==0&&gbuf[i+8]==0&&gbuf[i+9]==0){
            DWORD X=rd32(i+2);
            if(i>=2&&gbuf[i-2]==jbe){
                long lo=i-2-0x18; if(lo<0)lo=0;
                for(long k=lo;k<=i-2-7;k++){
                    if(gbuf[k]==0x83&&gbuf[k+1]==0x3D&&rd32(k+2)==X&&gbuf[k+6]==0x20){ if(Xout)*Xout=X; return i-2; }
                }
            }
        }
    }
    return -1;
}
static long find_s2(DWORD* Xout, BYTE jbe){
    for(long i=0;i+16<=(long)gsz;i++){
        if(gbuf[i]!=0x6A||gbuf[i+1]!=0x20) continue;
        BYTE pop=gbuf[i+2];
        if(pop<0x58||pop>0x5F||pop==0x5C) continue;
        int reg=pop-0x58;
        BYTE modrm=(BYTE)(0x05|(reg<<3));
        long k=i+3;
        if(gbuf[k]!=0x39||gbuf[k+1]!=modrm) continue;
        DWORD X=rd32(k+2);
        long j=k+6;
        if(gbuf[j]!=jbe) continue;
        long st=j+2;
        if(reg==0 && gbuf[st]==0xA3 && rd32(st+1)==X){ if(Xout)*Xout=X; return j; }
        if(gbuf[st]==0x89 && gbuf[st+1]==modrm && rd32(st+2)==X){ if(Xout)*Xout=X; return j; }
    }
    return -1;
}
/* 0 = n/a, 1 = can patch, 2 = already patched */
static int cpu_status(void){
    if(find_s1(0,0x76)>=0 || find_s2(0,0x76)>=0) return 1;
    if(find_s1(0,0x90)>=0 || find_s2(0,0x90)>=0) return 2;
    return 0;
}
static int cpu_apply(void){
    DWORD X=0; long jbe;
    if((jbe=find_s1(&X,0x76))>=0){ gbuf[jbe]=0x90; gbuf[jbe+1]=0x90; return 1; }
    if((jbe=find_s2(&X,0x76))>=0){ gbuf[jbe]=0x90; gbuf[jbe+1]=0x90; return 1; }
    return 0;
}

/* ---------- 0x9A license fix ---------- */
static int call_is(long callfo, DWORD target){
    if(callfo+5>(long)gsz || gbuf[callfo]!=0xE8) return 0;
    DWORD va=fo2va(callfo); if(!va) return 0;
    int rel=(int)rd32(callfo+1);
    return ((va+5+(DWORD)rel)&0xffffffff)==target;
}
static void neutralize_call(long callfo){
    gbuf[callfo]=0x83; gbuf[callfo+1]=0xC4; gbuf[callfo+2]=0x14; gbuf[callfo+3]=0x90; gbuf[callfo+4]=0x90;
}
static int neutralize_9a(void){
    DWORD kbx=export_va("KeBugCheckEx");
    DWORD kbc=export_va("KeBugCheck");
    if(!kbx) return -1;
    int count=0; const DWORD WINDOW=0x900;
    for(long j=0;j+10<=(long)gsz;j++){
        if(gbuf[j]==0x68&&gbuf[j+1]==0x9A&&gbuf[j+2]==0&&gbuf[j+3]==0&&gbuf[j+4]==0){
            if(call_is(j+5,kbx)||(kbc&&call_is(j+5,kbc))){ neutralize_call(j+5); count++; }
        }
    }
    static const BYTE movop[6]={0xB8,0xB9,0xBA,0xBB,0xBE,0xBF};
    static const BYTE pshop[6]={0x50,0x51,0x52,0x53,0x56,0x57};
    for(int r=0;r<6;r++){
        for(long j=0;j+5<=(long)gsz;j++){
            if(gbuf[j]==movop[r]&&gbuf[j+1]==0x9A&&gbuf[j+2]==0&&gbuf[j+3]==0&&gbuf[j+4]==0){
                long end=j+WINDOW; if(end>(long)gsz-2) end=(long)gsz-2;
                for(long k=j+5;k<end;k++){
                    if(gbuf[k]==pshop[r]&&gbuf[k+1]==0xE8&&(call_is(k+1,kbx)||(kbc&&call_is(k+1,kbc)))){
                        neutralize_call(k+1); count++;
                    }
                }
            }
        }
    }
    return count;
}
static int ilen(const BYTE* p){
    int i=0;
    while(p[i]==0x64||p[i]==0x65||p[i]==0x66||p[i]==0x67||p[i]==0xF0||p[i]==0xF2||p[i]==0xF3) i++;
    BYTE op=p[i++]; int imm=0,useM=0;
    if(op>=0x50&&op<=0x5F){}
    else if(op==0x90||op==0xCC||op==0xC3||op==0xC9||op==0xF8||op==0xFC||op==0xFA||op==0xFB){}
    else if(op==0x6A){i+=1;}
    else if(op==0x68){i+=4;}
    else if(op==0xEB){i+=1;}
    else if(op==0xE8||op==0xE9){i+=4;}
    else if(op>=0xB8&&op<=0xBF){i+=4;}
    else if(op>=0xB0&&op<=0xB7){i+=1;}
    else if(op==0xA0||op==0xA1||op==0xA2||op==0xA3){i+=4;}
    else if(op==0x88||op==0x89||op==0x8A||op==0x8B||op==0x01||op==0x03||op==0x09||op==0x0B||op==0x21||op==0x23||op==0x29||op==0x2B||op==0x31||op==0x33||op==0x39||op==0x3B||op==0x85||op==0x84||op==0x8D||op==0x63){useM=1;imm=0;}
    else if(op==0x80||op==0x82||op==0x6B||op==0xC0||op==0xC1||op==0xC6){useM=1;imm=1;}
    else if(op==0x81||op==0x69||op==0xC7){useM=1;imm=4;}
    else if(op==0x83){useM=1;imm=1;}
    else if(op==0xFF||op==0xFE||op==0x8F||op==0xD1||op==0xD3){useM=1;imm=0;}
    else if(op==0x0F){ BYTE o2=p[i++]; if(o2>=0x80&&o2<=0x8F)i+=4;
        else if((o2>=0x90&&o2<=0x9F)||o2==0xB6||o2==0xB7||o2==0xBE||o2==0xBF||o2==0xAF||(o2>=0x40&&o2<=0x4F)){useM=1;imm=0;}
        else return 0; }
    else return 0;
    if(useM){ BYTE m=p[i++]; int mod=(m>>6)&3,rm=m&7; if(mod!=3&&rm==4)i++; if(mod==0){if(rm==5)i+=4;} else if(mod==1)i++; else if(mod==2)i+=4; i+=imm; }
    return i;
}
static int prologue_cut(const BYTE* p,int need){
    int total=0,guard=0;
    while(total<need){
        int k=total; while(p[k]==0x64||p[k]==0x65||p[k]==0x66||p[k]==0x67||p[k]==0xF0||p[k]==0xF2||p[k]==0xF3)k++;
        BYTE o=p[k];
        if(o==0xE8||o==0xE9||o==0xEB) return 0;
        if(o==0x0F){ BYTE o2=p[k+1]; if(o2>=0x80&&o2<=0x8F) return 0; }
        int l=ilen(p+total); if(l<=0) return 0;
        total+=l; if(++guard>10) return 0;
    }
    return total;
}
static int entry_hook_9a(void){
    DWORD kbx=export_va("KeBugCheckEx");
    if(!kbx) return 0;
    long kfo=rva2fo(kbx-image_base());
    if(kfo<0) return 0;
    int cut=prologue_cut(gbuf+kfo,5);
    if(cut<5||cut>24) return 0;
    long cave=find_slack_cave(13+cut+5);              /* prefer: no new section */
    if(cave<0) cave=find_cave(13+cut+5);              /* then: zero-run in code  */
    if(cave<0) cave=add_section_cave((DWORD)(13+cut+5)); /* last resort: append  */
    if(cave<0) return 0;
    mark_used(cave,13+cut+5);                         /* reserve so the PFN guard can't reuse it */
    DWORD cave_va=fo2va(cave), cont_va=kbx+(DWORD)cut;
    gbuf[cave+0]=0x81;gbuf[cave+1]=0x7C;gbuf[cave+2]=0x24;gbuf[cave+3]=0x04;gbuf[cave+4]=0x9A;gbuf[cave+5]=0;gbuf[cave+6]=0;gbuf[cave+7]=0;
    gbuf[cave+8]=0x75;gbuf[cave+9]=0x03;
    gbuf[cave+10]=0xC2;gbuf[cave+11]=0x14;gbuf[cave+12]=0x00;
    for(int t=0;t<cut;t++) gbuf[cave+13+t]=gbuf[kfo+t];
    { long j=cave+13+cut; DWORD rel=cont_va-(cave_va+(DWORD)(13+cut)+5);
      gbuf[j]=0xE9;gbuf[j+1]=rel&0xff;gbuf[j+2]=(rel>>8)&0xff;gbuf[j+3]=(rel>>16)&0xff;gbuf[j+4]=(rel>>24)&0xff; }
    { DWORD rel=cave_va-(kbx+5);
      gbuf[kfo]=0xE9;gbuf[kfo+1]=rel&0xff;gbuf[kfo+2]=(rel>>8)&0xff;gbuf[kfo+3]=(rel>>16)&0xff;gbuf[kfo+4]=(rel>>24)&0xff; }
    for(int t=5;t<cut;t++) gbuf[kfo+t]=0x90;
    return 1;
}
/* MiBuildPfnDatabase unmapped-PFN-entry guard (NT 3.50 / 3.51).
 * On a machine that reports >=~5-6 logical CPUs, the boot-time PFN-database walk reaches a reserved
 * physical page whose own PFN-database entry page is NOT mapped (sparse PFN db) and faults -> STOP
 * 0x50 PAGE_FAULT_IN_NONPAGED_AREA (verified on a real i7-3770: entry page 0xFFBB9000, PFN 0xDD755).
 * Guard the first entry access ('add edi,[PfnBase] ; mov ebx,[edi+0x14]'): if the entry's page is not
 * present in the self-map at 0xC0000000, skip this PFN (jump to the loop's next-PFN label). */
static const int SIG_PFN[11]={0x03,0x3d,-1,-1,-1,-1,0x8b,0x5f,0x14,0x33,0xd8};
static long find_sig(const int*p,int len,int*cnt);    /* fwd: defined later in this header */
/* NT 3.1 MP kernel sparse-PFN guard. Exact observed build only.
 * Retain PE layout: old loaders reject some appended-section kernels.
 * The two page endpoints cover every field accessed in a 24-byte PFN record.
 * Validate the unique code pattern and its existing next-entry branch first. */
static int kernel_pfn_guard_nt31(void){
    static const int pattern[18]={0x03,0x1d,-1,-1,-1,-1,0x8b,0x44,0x24,0x18,0xc1,0xe0,0x0c,0x8b,0x6b,0x14,0x33,0xe8};
    static const BYTE stub[68]={0x50,0x51,0x8d,0x43,0x04,0xe8,0x1f,0x00,0x00,0x00,0x74,0x16,0x8d,0x43,0x17,0xe8,0x15,0x00,0x00,0x00,0x74,0x0c,0x59,0x58,0x8b,0x6b,0x14,0x31,0xc5,0xe9,0x91,0xf4,0x05,0x00,0x59,0x58,0xe9,0xb7,0xf4,0x05,0x00,0x89,0xc1,0xc1,0xe9,0x16,0xf6,0x04,0x8d,0x00,0x00,0x30,0xc0,0x01,0x74,0x0b,0xc1,0xe8,0x0c,0xf6,0x04,0x85,0x00,0x00,0x00,0xc0,0x01,0xc3};
    if(rd32(pe_off()+8)!=0x2c51c08au || image_base()!=0x80100000u) return 0;
    int count=0; long a=find_sig(pattern,18,&count),t=-1;
    if(a<0 || count!=1) return -1;
    long site=a+13;
    for(long j=site-1;j>site-0x100 && j>=0;j--)
        if(gbuf[j]==0xf6 && gbuf[j+1]==0x07 && gbuf[j+2]==0x01 && gbuf[j+3]==0x74){t=j;break;}
    if(t<0) return -1;
    DWORD skip=fo2va(t+5)+(DWORD)(long)(signed char)gbuf[t+4];
    long next=rva2fo(skip-image_base());
    if(next<0 || gbuf[next]!=0x83 || gbuf[next+1]!=0xc7 || gbuf[next+2]!=4 || gbuf[next+3]!=0x4a) return -1;
    long cave=find_cave(sizeof(stub));
    if(cave<0) return -1;
    DWORD cv=fo2va(cave),sv=fo2va(site);
    memcpy(gbuf+cave,stub,sizeof(stub));
    wr32(cave+29+1,(sv+5)-(cv+29+5));
    wr32(cave+36+1,skip-(cv+36+5));
    gbuf[site]=0xe9;wr32(site+1,cv-(sv+5));
    return 1;
}
static int kernel_pfn_guard(void){
    int nt31=kernel_pfn_guard_nt31(); if(nt31) return nt31;
    int c; long a=find_sig(SIG_PFN,11,&c);
    if(a<0||c!=1) return 0;
    long fsite=a+6;
    DWORD fsite_va=fo2va(fsite), cont_va=fsite_va+5;
    long t=-1;
    for(long j=fsite-1;j>fsite-0x100 && j>=0;j--)
        if(gbuf[j]==0xf6&&gbuf[j+1]==0x02&&gbuf[j+2]==0x01&&gbuf[j+3]==0x74){ t=j; break; }
    if(t<0) return 0;
    DWORD skip_va=fo2va(t+5)+(DWORD)(long)(signed char)gbuf[t+4];
    long cave=find_cave(33);
    if(cave<0) cave=add_section_cave(33);                  /* NT3.50: no .text slack -> append a section */
    if(cave<0) return 0;
    DWORD cv=fo2va(cave); long o=cave;
    gbuf[o++]=0x50;
    gbuf[o++]=0x8d;gbuf[o++]=0x47;gbuf[o++]=0x14;
    gbuf[o++]=0xc1;gbuf[o++]=0xe8;gbuf[o++]=0x0c;
    gbuf[o++]=0x8b;gbuf[o++]=0x04;gbuf[o++]=0x85;wr32(o,0xC0000000);o+=4;
    gbuf[o++]=0xa8;gbuf[o++]=0x01;
    gbuf[o++]=0x58;
    gbuf[o++]=0x0f;gbuf[o++]=0x84;wr32(o,skip_va-(cv+(DWORD)(o+4-cave)));o+=4;
    gbuf[o++]=0x8b;gbuf[o++]=0x5f;gbuf[o++]=0x14;
    gbuf[o++]=0x33;gbuf[o++]=0xd8;
    gbuf[o++]=0xe9;wr32(o,cont_va-(cv+(DWORD)(o+4-cave)));o+=4;
    gbuf[fsite]=0xe9; wr32(fsite+1, cv-(fsite_va+5));
    return 1;
}
/* 0 = n/a (no export), 1 = can patch, 2 = already (entry is a jmp) */
static int lic_status(void){
    DWORD kbx=export_va("KeBugCheckEx");
    if(!kbx) return 0;
    long kfo=rva2fo(kbx-image_base());
    if(kfo<0) return 0;
    if(gbuf[kfo]==0xE9) return 2;
    return 1;
}

/* ---------- PAE (Win2000) : dynamic, SP0..SP4 (no fixed offsets) ----------
 * (1) suite-gate bypass: push 7 (Datacenter) ; call suite-check ; cmp al,1 ; jnz
 *     -> NOP the jnz so the large-memory path is always taken.
 * (2) cap raise: mov eax,{0xFFFFF|0x1FFFFF|0x3FFFFF} followed by cmp/je/jne/jmp
 *     (NOT 'and' = PFN mask) -> raise to TARGET (32 GB). Lifts SKU selector + hard min-cap.
 * Generalizes evgen-b/PatchPAE3's 2195 kernel patches across every Win2000 SP, and
 * the cap raise is the min(actual,cap) ceiling that otherwise re-pins RAM at 4 GB. */
#define PAE_TARGET 0x7FFFFFu   /* 32 GB highest physical page index (gb*0x40000-1) */

static int push7_before(long callfo){
    long lo=callfo-24; if(lo<0)lo=0;
    for(long k=lo;k<callfo;k++) if(gbuf[k]==0x6A&&gbuf[k+1]==0x07) return 1;
    return 0;
}
/* suite gate. apply=0 -> count jnz still present; apply=1 -> NOP them, return count. */
static int pae_gate(int apply){
    int n=0;
    for(long a=5;a+6<=(long)gsz;a++){
        if(gbuf[a]!=0x3C||gbuf[a+1]!=0x01) continue;              /* cmp al,1 */
        long c=a-5; if(gbuf[c]!=0xE8) continue;                  /* call imm32 right before */
        if(!push7_before(c)) continue;                           /* push 7 (Datacenter suite) */
        if(gbuf[a+2]==0x75){                                     /* jnz rel8 */
            n++; if(apply){ gbuf[a+2]=0x90; gbuf[a+3]=0x90; }
        } else if(gbuf[a+2]==0x0F&&gbuf[a+3]==0x85){             /* jnz rel32 (some SPs) */
            n++; if(apply){ for(int t=0;t<6;t++) gbuf[a+2+t]=0x90; }
        }
    }
    return n;
}
static DWORD g_cap_target = PAE_TARGET;   /* highest-page-index cap, set by pae_apply(gb) */
/* cap raise. apply=0 -> count small-imm cap sites; apply=1 -> raise them to g_cap_target. */
static int pae_caps(int apply){
    int n=0;
    for(long i=0;i+6<=(long)gsz;i++){
        if(gbuf[i]!=0xB8) continue;                              /* mov eax,imm32 */
        DWORD imm=rd32(i+1);
        if(imm!=0xFFFFFu&&imm!=0x1FFFFFu&&imm!=0x3FFFFFu) continue;
        BYTE nx=gbuf[i+5];
        if(nx==0x23||nx==0x25) continue;                         /* and reg,eax -> PFN mask, KEEP */
        if(nx==0x3B||nx==0x39||nx==0x74||nx==0x75||nx==0xEB){     /* cmp / je / jne / jmp -> cap or SKU */
            n++; if(apply){ DWORD t=g_cap_target; gbuf[i+1]=t&0xff; gbuf[i+2]=(t>>8)&0xff; gbuf[i+3]=(t>>16)&0xff; gbuf[i+4]=(t>>24)&0xff; }
        }
    }
    return n;
}
/* BOOT-TIME memory SKU cap (MmInitSystem). The selector is:
 *   push 7 ; call ExVerifySuite ; cmp al,<1|reg> ; jne not_dc
 *   datacenter: ecx = 0x00800000 (32GB) ; AdvServer: mov ecx,0x200000 (8GB) ;
 *   Professional: mov ecx,0x100000 (4GB)   <-- the client cap that drops >4GB RAM
 * then descriptors with base >= ecx are dropped.
 * SAFE fix: do NOT touch control flow (forcing the Datacenter path makes Win2000
 * mis-init as Datacenter and STOP 0x67). Instead raise the Professional (0x100000)
 * and AdvServer (0x200000) page-count caps *in place* to the chosen max, so whatever
 * SKU branch runs keeps all RAM. The selector is located by push 7 + call + the
 * distinctive 0x00800000 datacenter marker, so only this one function is touched. */
static DWORD g_boot_pages = 0x800000u;   /* chosen boot cap in pages (gb*0x40000); set by pae_apply */
static int pae_has_selector(void){
    for(long i=0;i+2<=(long)gsz;i++){
        if(gbuf[i]!=0x6A||gbuf[i+1]!=0x07) continue;
        long c=-1; for(long k=i+2;k<i+18 && k+5<=(long)gsz;k++){ if(gbuf[k]==0xE8){ c=k; break; } }
        if(c<0) continue;
        for(long k=i;k<i+64 && k+4<=(long)gsz;k++)
            if(gbuf[k]==0&&gbuf[k+1]==0&&gbuf[k+2]==0x80&&gbuf[k+3]==0) return 1;
    }
    return 0;
}
/* apply=0 -> count unraised Pro/AdvServer caps; apply=1 -> raise them to g_boot_pages. */
static int pae_boot_caps(int apply){
    int n=0;
    for(long i=0;i+2<=(long)gsz;i++){
        if(gbuf[i]!=0x6A||gbuf[i+1]!=0x07) continue;             /* push 7 */
        long c=-1; for(long k=i+2;k<i+18 && k+5<=(long)gsz;k++){ if(gbuf[k]==0xE8){ c=k; break; } }
        if(c<0) continue;
        int dc=0;                                                /* confirm SKU selector via 0x00800000 */
        for(long k=i;k<i+64 && k+4<=(long)gsz;k++)
            if(gbuf[k]==0&&gbuf[k+1]==0&&gbuf[k+2]==0x80&&gbuf[k+3]==0){ dc=1; break; }
        if(!dc) continue;
        for(long k=i;k<i+88 && k+5<=(long)gsz;k++){              /* raise mov r32,{0x100000|0x200000} */
            if(gbuf[k]>=0xB8&&gbuf[k]<=0xBF){
                DWORD imm=rd32(k+1);
                if(imm==0x100000u||imm==0x200000u){
                    n++; if(apply){ DWORD p=g_boot_pages; gbuf[k+1]=p&0xff; gbuf[k+2]=(p>>8)&0xff; gbuf[k+3]=(p>>16)&0xff; gbuf[k+4]=(p>>24)&0xff; }
                }
            }
        }
    }
    return n;
}
static int pae_capable(void){
    if(pae_has_selector()) return 1;                                                      /* boot SKU selector */
    for(long i=0;i+5<=(long)gsz;i++){ if(gbuf[i]==0xB8&&rd32(i+1)==0x1FFFFFu) return 1; } /* 8GB SKU tier */
    if(pae_gate(0)>0) return 1;                                                           /* hot-add gate */
    return 0;
}
/* 0 = n/a (non-PAE kernel), 1 = can patch, 2 = already done */
static int pae_status(void){
    if(!pae_capable()) return 0;
    if(pae_boot_caps(0)>0 || pae_gate(0)>0 || pae_caps(0)>0) return 1;
    return 2;
}
/* gb in {8,16,32,64}; default 32 if out of range. */
static int pae_apply(int gb){
    if(gb!=8&&gb!=16&&gb!=32&&gb!=64) gb=32;
    g_boot_pages=(DWORD)gb*0x40000u;         /* boot cap in pages */
    g_cap_target=g_boot_pages-1u;            /* highest page index for hot-add caps */
    int boot=pae_boot_caps(1);               /* raise Pro/AdvServer boot caps (no control-flow change) */
    int a=pae_gate(1);                       /* hot-add gate */
    int b=pae_caps(1);                       /* hot-add caps */
    return (boot+a+b)>0;
}

/* ---------- kernel detector (for scanning all of system32) ----------
 * A patchable NT kernel is a NATIVE-subsystem PE that exports KeBugCheckEx. */
static int is_kernel_native(void){
    if(gsz<0x200||gbuf[0]!='M'||gbuf[1]!='Z') return 0;
    DWORD pe=pe_off(); if(pe+0x48>=gsz) return 0;
    if(gbuf[pe]!='P'||gbuf[pe+1]!='E') return 0;
    WORD subsys=gbuf[pe+0x18+0x44]|(gbuf[pe+0x18+0x45]<<8);
    if(subsys!=1) return 0;                  /* IMAGE_SUBSYSTEM_NATIVE */
    return export_va("KeBugCheckEx")?1:0;
}

/* ============ MPS HAL multi-CPU engine: ACPI MP-parser loop-exit hook (Phase0, mapper-safe)
 * + MPFIX (ROM-write AP-mark, NT3.1/3.50) + APIC self-check neutralizer (NT3.50/3.51/4.0).
 * The ACPI override runs during HalInitSystem's MP table parse (right after the processor
 * counting loop), NOT inside HalStartNextProcessor - running the physical-memory mapper between
 * the AP trampoline setup and the AP launch double-faults (STOP 0x7F). ============ */
static long hal_cave(DWORD need);
/* AP-select entry forms - read ONLY to recover entry-ptr/count globals (NOT hooked). */
static const int APSEL_A[]={0x53,0xb1,0xff,0x56,0xa1,-1,-1,-1,-1,0x57,0xbe,0x64,0x00,0x00,0x00,0x55,0x33,0xff,0x39,0x3d};
static const int APSEL_B[]={0x53,0x8b,0x0d,-1,-1,-1,-1,0x56,0xb3,0xff,0x8b,0x44,0x24,0x10,0x57};
static const int APSEL_C[]={0x53,0x56,0x57,0x55,0xbe,0x64,0x00,0x00,0x00,0xb1,0xff,0xa1,-1,-1,-1,-1,0x2b,0xff,0x39,0x3d};
/* MP-parser counting-loop exit: add eax,0x14 ; cmp byte[eax],0 ; je loop  (unique per HAL). The
 * first finalize instruction follows at +8: 'mov ecx,[count]' (8b 0d, NT3.1/3.50) or
 * 'mov [aux],eax' (a3, NT3.51/4.0) - that instruction is the hook site. */
static const int SIG_LOOPEXIT[]={0x83,0xc0,0x14,0x80,0x38,0x00,0x74};
static const int MAP_NEW[]={0x53,0x56,0x8b,0x74,0x24,0x10,0x57,0x8b,0xc6,0xc1,0xe8,0x0c,0x40,0x50,0xff,0x74,0x24,0x14,0xe8};
static const int MAP_31[]={0x56,0x57,0x8b,0x54,0x24,0x10,0x0b,0xd2,0x74};
static const int MRK_SIG[]={0x80,0x48,0x03,0x40,0x8a,0x48,0x01};
static const int CHK3_SIG[]={0xf6,0xc2,0x01,0x74,-1,0xf6,0xc2,0x02,0x75,-1,0xf6,0xc2,0x40,0x74,-1};
static const int CHKCMB_SIG[]={0xf6,0xc2,0x01,0x74,-1,0xf6,0xc2,0x42,0x74,-1};
static const int SIG_APICVER[]={0x80,0x7e,0x30,0x1f,0x76};
static const int SIG_APICAP[]={0x84,0xc0,0x74,-1,0xfe,0xc3};

/* HalpStartProcessor Prcb-preserve fix (NT 3.51 / 4.0) - see cpulift.c for the full rationale.
 * The function reads LoaderBlock->Prcb up top for the APIC-ID lookup, then RE-READS it to store the
 * logical index ('mov ecx,[ebp+8]; mov ecx,[ecx+1c]; mov [ecx+0x17c],al'); meanwhile an AP coming
 * online zeroes that field -> NULL store -> STOP 0x0A and a stuck-low processor count. Keep the PRCB
 * in ESI (preserved across the stdcall lookup, unused again before the store) and store through it. */
static const int SIG_PP_READ[9] ={0x8b,0x45,0x08,0x8b,0x40,0x1c,0x0f,0xb6,0x40};
static const int SIG_PP_STORE[8]={0x8b,0x4d,0x08,0x8b,0x49,0x1c,0x88,0x81};
/* hal_prcb_preserve() is defined after find_sig() below. */

/* ---- AP calibration speedup (NT 3.51 / NT 4.0) ----------------------------------------------
 * These HALs run a per-processor RTC-synced TSC/APIC calibration (HalpCalibratePerfCount): it
 * waits over RTC "update-in-progress" (UIP) second boundaries -- ~1-2 s of wall-clock EACH TIME.
 * HalInitSystem calls it once on the BSP and again on every AP (two call sites), so with N CPUs
 * the (N-1) APs each burn ~1-2 s re-measuring a clock that is identical on every core. That is the
 * visible "slow until the processor count appears" at 8 CPUs (~7 x ~1-2 s).
 * SIG_SPD_LOOP marks the loop-count 'mov esi,imm' immediately before the first RTC-register access
 * ('mov eax,[APIC div]; mov al,0ah; out 70h,al'); SIG_SPD_SAVE marks the post-loop
 * 'mov fs:[0a4h],eax; mov eax,-1; sub eax,ecx' where the raw TSC delta (eax) and APIC count (ecx)
 * are consumed. We hook both: the FIRST call (BSP) measures and caches the raw eax/ecx; EVERY later
 * call (APs) reuses the cache and skips the RTC wait, yielding byte-identical fs:[] scale factors
 * while still doing all per-AP APIC-timer programming. NT 3.1 / 3.50 use a different (non-UIP)
 * calibration and do not match these signatures -- they are simply left unpatched. */
static const int SIG_SPD_LOOP[14]={0xBE,-1,0x00,0x00,0x00,0xA1,0xE0,0x03,0xFE,0xFF,0xB0,0x0A,0xE6,0x70};
static const int SIG_SPD_SAVE[13]={0x64,0xA3,0xA4,0x00,0x00,0x00,0xB8,0xFF,0xFF,0xFF,0xFF,0x2B,0xC1};

/* auto-generated by gen_tpl2.py - MP-parser loop-exit ACPI hook + MPFIX */
static const BYTE ENUM_TPL[214]={104,17,0,0,113,104,0,0,14,0,232,241,255,175,241,133,192,116,50,137,198,185,0,32,0,0,129,62,82,83,68,32,117,29,129,126,4,80,84,82,32,117,20,86,81,49,192,49,201,177,20,2,6,70,73,117,250,89,94,132,192,116,9,131,198,16,73,117,213,49,192,195,139,94,16,104,18,0,0,113,83,232,170,255,175,241,133,192,116,235,137,198,139,78,4,131,233,36,193,233,2,141,126,36,133,201,116,217,139,31,81,104,18,0,0,113,83,232,134,255,175,241,89,133,192,116,8,129,56,65,80,73,67,116,9,131,199,4,73,117,219,49,192,195,139,72,4,129,249,0,8,0,0,118,5,185,0,8,0,0,141,52,8,141,120,44,49,210,57,247,115,39,128,63,0,117,22,246,71,4,1,116,16,131,250,8,115,11,138,71,3,136,132,21,19,0,0,113,66,15,182,71,1,133,192,116,4,1,199,235,213,137,208,195};
#define ENUM_LEN 214
static const int ENUM_BIOSSZ=1;
static const int ENUM_TBLSZ[2]={76,112};
static const int ENUM_DIDBUF=194;
static const int ENUM_MAPCALL[3]={11,82,118};
static const BYTE AP_MC_TPL[146]={96,156,232,0,0,0,0,93,131,189,2,0,0,113,127,119,110,232,234,255,175,242,131,248,2,114,100,131,248,8,118,5,184,8,0,0,0,137,193,59,141,2,0,0,113,114,80,141,189,4,0,0,113,49,219,15,182,132,29,5,0,0,113,198,7,0,136,71,1,198,71,2,20,133,219,117,6,198,71,3,3,235,4,198,71,3,1,49,192,137,71,4,137,71,8,137,71,12,137,71,16,131,199,20,67,57,203,114,202,141,133,4,0,0,113,137,133,3,0,0,113,137,141,2,0,0,113,157,97,232,0,0,0,0,89,139,137,33,0,0,113,233,110,255,175,243};
#define AP_MC_LEN 146
static const int AP_MC_COUNT[3]={10,41,123};
static const int AP_MC_ENTRY[1]={117};
static const int AP_MC_EBUF[2]={49,111};
static const int AP_MC_IDBUF[1]={59};
static const int AP_MC_DISP[1]={137};
static const int AP_MC_ECALL=18;
static const int AP_MC_RJMP=142;
static const BYTE AP_SE_TPL[148]={96,156,232,0,0,0,0,93,131,189,2,0,0,113,127,119,110,232,234,255,175,242,131,248,2,114,100,131,248,8,118,5,184,8,0,0,0,137,193,59,141,2,0,0,113,114,80,141,189,4,0,0,113,49,219,15,182,132,29,5,0,0,113,198,7,0,136,71,1,198,71,2,20,133,219,117,6,198,71,3,3,235,4,198,71,3,1,49,192,137,71,4,137,71,8,137,71,12,137,71,16,131,199,20,67,57,203,114,202,141,133,4,0,0,113,137,133,3,0,0,113,137,141,2,0,0,113,157,97,81,232,0,0,0,0,89,137,129,33,0,0,113,89,233,108,255,175,243};
#define AP_SE_LEN 148
static const int AP_SE_COUNT[3]={10,41,123};
static const int AP_SE_ENTRY[1]={117};
static const int AP_SE_EBUF[2]={49,111};
static const int AP_SE_IDBUF[1]={59};
static const int AP_SE_DISP[1]={138};
static const int AP_SE_ECALL=18;
static const int AP_SE_RJMP=144;
static const BYTE MARK_TPL[26]={83,232,0,0,0,0,91,129,195,1,0,0,122,15,182,72,1,15,171,11,91,233,230,255,207,250};
#define MARK_LEN 26
static const int MARK_ADDIMM=9;
static const int MARK_JDONE=22;
static const BYTE CHK3_TPL[34]={80,83,232,0,0,0,0,91,129,195,1,0,0,122,15,182,64,1,15,163,3,91,88,15,130,227,255,175,250,233,222,255,191,250};
#define CHK3_LEN 34
static const int CHK3_ADDIMM=10;
static const int CHK3_JCONT=25;
static const int CHK3_JFOUND=30;
static const BYTE CHKCMB_TPL[46]={80,83,232,0,0,0,0,91,129,195,1,0,0,122,246,194,2,117,20,15,182,64,1,15,163,3,91,88,15,130,222,255,175,250,233,217,255,191,250,91,88,233,210,255,175,250};
#define CHKCMB_LEN 46
static const int CHKCMB_ADDIMM=10;
static const int CHKCMB_JCONT=30;
static const int CHKCMB_JFOUND=35;
static const int CHKCMB_JCONT2=42;

static long find_sig(const int*p,int len,int*cnt){
    int c=0; long hit=-1;
    for(long i=0;i+len<=(long)gsz;i++){
        int ok=1;
        for(int k=0;k<len;k++){ if(p[k]>=0 && gbuf[i+k]!=(BYTE)p[k]){ ok=0; break; } }
        if(ok){ if(hit<0) hit=i; c++; }
    }
    *cnt=c; return hit;
}
static long find_scratch(DWORD need){
    long st=sec_tab(); int n=num_secs();
    for(int i=0;i<n;i++){ long s=st+i*40; DWORD ch=rd32(s+36);
        if((ch&0x20000000)||!(ch&0x80000000)||(ch&0x02000000)) continue; /* writable, non-exec, non-discardable */
        DWORD vsz=rd32(s+8),rawp=rd32(s+20),rsz=rd32(s+16);
        DWORD lim=(vsz&&vsz<rsz)?vsz:rsz;                     /* stay inside the mapped portion */
        DWORD j=0;
        while(j<lim){
            if(gbuf[rawp+j]==0){
                DWORD k=j; while(k<lim&&gbuf[rawp+k]==0) k++;  /* [j,k) all zero */
                long start=((long)rawp+(long)j+3)&~3L;        /* 4-align within the run */
                if((long)(rawp+k)-start>=(long)need) return start;
                j=k;
            } else j++;
        }
    }
    return -1;
}
/* Cave in the TAIL of the largest zero-run of an executable, non-discardable section. That run is the
 * HAL's 16-bit AP real-mode trampoline buffer (zero in the file, built at runtime). Placing the cave at
 * the far end leaves the whole front for the runtime-built AP code: the cave runs once in Phase0 (via the
 * hook) and is harmlessly overwritten when Phase1 builds the trampoline there, so the APs never execute
 * it. Unlike grow_last_section_cave this lives in .text, which every NT osloader keeps mapped -- NT 4.0's
 * loader frees the grown .reloc, so a cave there faults and NT 4.0 must use this instead. Requires >=0x300
 * bytes of AP-code headroom at the run start. */
static long find_trampoline_cave(long need){
    long st=sec_tab(); int n=num_secs();
    DWORD bestlen=0; long beststart=-1;
    for(int i=0;i<n;i++){ long s=st+i*40; DWORD ch=rd32(s+36);
        if(!(ch&0x20000000)||(ch&0x02000000)) continue;   /* executable, non-discardable */
        DWORD vsz=rd32(s+8),rawp=rd32(s+20),rsz=rd32(s+16);
        DWORD lim=(vsz&&vsz<rsz)?vsz:rsz;
        DWORD o=0;
        while(o<lim){
            if(gbuf[rawp+o]==0){ DWORD k=o; while(k<lim&&gbuf[rawp+k]==0)k++;
                if(k-o>bestlen){ bestlen=k-o; beststart=(long)(rawp+o); }
                o=k;
            } else o++;
        }
    }
    if(beststart<0 || (long)bestlen < need+0x300) return -1; /* need AP-code headroom at the run start */
    return (beststart+(long)bestlen-need)&~3L;               /* far tail, 4-aligned */
}
static long hal_cave(DWORD need){
    long c=find_slack_cave((long)need);     /* prefer in-image slack (NT3.5x osloader-safe) */
    if(c<0) c=find_cave((long)need);        /* then an UNREFERENCED zero-run in a code section */
    if(c<0) c=find_trampoline_cave((long)need); /* then the AP-trampoline buffer tail (.text, always mapped) */
    if(c<0) c=grow_last_section_cave(need);  /* then grow the last section in place */
    if(c<0) c=add_section_cave(need);        /* last resort: append a section */
    return c;
}
/* true if any code (executable section) holds a 4-byte little-endian value inside [lo,hi). Used to
 * tell a genuinely-free zero-run from zero-initialised (BSS) globals, which ARE code-referenced. */
static int range_referenced(DWORD lo,DWORD hi){
    long st=sec_tab(); int n=num_secs();
    for(int i=0;i<n;i++){ long s=st+i*40; DWORD ch=rd32(s+36);
        if(!(ch&0x20000000)) continue;                 /* executable sections only */
        DWORD rawp=rd32(s+20),rsz=rd32(s+16);
        if(rsz<4) continue;
        for(DWORD o=0;o+4<=rsz;o++){ DWORD v=rd32((long)rawp+(long)o); if(v>=lo&&v<hi) return 1; }
    }
    return 0;
}
static int hal_prcb_preserve(void){
    int cr,cs; long ro,so;
    ro=find_sig(SIG_PP_READ,9,&cr);
    so=find_sig(SIG_PP_STORE,8,&cs);
    if(ro<0||cr!=1||so<0||cs!=1) return 0;   /* not the uniform NT3.51/4.0 layout */
    if(so<=ro) return 0;
    gbuf[ro+4]=0x70;                           /* mov eax,[eax+1c] -> mov esi,[eax+1c]  */
    gbuf[ro+8]=0x46;                           /* movzx eax,[eax+off] -> movzx eax,[esi+off] */
    gbuf[so+0]=0x89; gbuf[so+1]=0xf1;          /* mov ecx,[ebp+8]  -> mov ecx,esi       */
    gbuf[so+2]=0x90; gbuf[so+3]=0x90;          /* mov ecx,[ecx+1c] -> nop nop nop nop   */
    gbuf[so+4]=0x90; gbuf[so+5]=0x90;
    return 1;
}

/* AP tiled-CR3 premature-free teardown fix (NT3.51 SP5 / NT4.0). See cpulift.c for the
 * full rationale: HalpStartProcessor's fixed spin-wait times out for a slow AP, frees the
 * tiled transition PD, and the abandoned AP triple-faults at the trampoline LJMP (confirmed
 * on a real i7-3770 via a VBox VINF_EM_TRIPLE_FAULT on VCPU5). Enlarge the wait and skip the
 * free on timeout. */
static const int SIG_APWAIT1[12]={0xb9,0xff,0xff,0x1f,0x00,0x83,0xbd,-1,-1,-1,-1,0x00};
static const int SIG_APWAIT2[10]={0x75,-1,0x49,0x83,0xf9,0x00,0x75,-1,0xeb,-1};
static int hal_tiled_cr3_fix(void){
    int c; int did=0; long a;
    if((a=find_sig(SIG_APWAIT1,12,&c))>=0&&c==1){ gbuf[a+3]=0xff; gbuf[a+4]=0xff; did=1; } /* 0x1fffff->0xffffffff */
    if((a=find_sig(SIG_APWAIT2,10,&c))>=0&&c==1&&gbuf[a-7]==0x83&&gbuf[a-6]==0xbd&&gbuf[a-1]==0x00)
        gbuf[a+9]=(unsigned char)(gbuf[a+9]+5);                       /* timeout jmp skips free */
    return did;
}

static int apic_verify_fix(void){
    int c,n=0; long a;
    if((a=find_sig(SIG_APICVER,5,&c))>=0&&c==1){ gbuf[a+4]=0xeb; n++; }  /* jbe -> jmp (skip Bad APIC Version) */
    if((a=find_sig(SIG_APICAP,6,&c))>=0&&c==1){ gbuf[a+2]=0x90; gbuf[a+3]=0x90; n++; } /* je fail -> NOP NOP */
    return n;
}
/* NT 3.1-specific MPS-HAL fixes (signatures are unique to the NT 3.1 halmps and absent on 3.5x/4.0,
 * so these are no-ops elsewhere). NT 3.1 does its APIC-version check and processor registration
 * differently from 3.5x/4.0 (which use the jbe/je branches apic_verify_fix patches):
 *  (1) APIC-version coercion: "or esi,0 / mov esi,0xd1" -> "or esi,0x900 / mov esi,0x8d1" so the
 *      HAL accepts the (virtual/modern) local-APIC version instead of rejecting >2 CPUs.
 *  (2) processor registration: "xor eax,eax;nop;nop; mov [edx+0x10],eax" (stores 0) ->
 *      "mov eax,[esp+0xc]; mov [edx+0x10],eax" so the real affinity/id is stored per added CPU. */
static const int SIG_NT31_APIC[21]={0x81,0xfe,0xff,0x00,0x00,0x00,0x74,-1,0x81,0xce,0x00,0x00,0x00,0x00,0xeb,0x05,0xbe,0xd1,0x00,0x00,0x00};
static const int SIG_NT31_PROC[9]={0x89,0x02,0x31,0xc0,0x90,0x90,0x89,0x42,0x10};
static int nt31_fixes(void){
    int c,n=0; long a;
    if((a=find_sig(SIG_NT31_APIC,21,&c))>=0&&c==1){ gbuf[a+11]=0x09; gbuf[a+18]=0x08; n++; }
    if((a=find_sig(SIG_NT31_PROC,9,&c))>=0&&c==1){ gbuf[a+2]=0x8b; gbuf[a+3]=0x44; gbuf[a+4]=0x24; gbuf[a+5]=0x0c; n++; }
    return n;
}

static int hal_apply(void){
    int c; long a;
    n_used=0;                                   /* fresh cave bookkeeping for this file */
    /* recover entry-ptr / count globals from the AP-select function (it is NOT hooked) */
    DWORD entry_g=0,count_g=0;
    if((a=find_sig(APSEL_C,20,&c))>=0&&c==1){ entry_g=rd32(a+12); count_g=rd32(a+20); }
    else if((a=find_sig(APSEL_A,20,&c))>=0&&c==1){ entry_g=rd32(a+5); count_g=rd32(a+20); }
    else if((a=find_sig(APSEL_B,15,&c))>=0&&c==1){ entry_g=rd32(a+3);
        long best=-1,i; for(i=a;i<a+48;i++){ if((gbuf[i]==0x3b||gbuf[i]==0x39)&&gbuf[i+1]==0x05){ best=i; break; } }
        if(best<0) return 0; count_g=rd32(best+2);
    } else return 0;

    /* MP-parser counting-loop exit -> the first finalize instruction is our Phase0 hook site */
    long le=find_sig(SIG_LOOPEXIT,7,&c); if(le<0||c!=1) return 0;
    long hook_fo=le+8;
    int form_mc,disp_len; DWORD disp_g;
    if(gbuf[hook_fo]==0x8b&&gbuf[hook_fo+1]==0x0d){ form_mc=1; disp_g=rd32(hook_fo+2); disp_len=6; }
    else if(gbuf[hook_fo]==0xa3){ form_mc=0; disp_g=rd32(hook_fo+1); disp_len=5; }
    else return 0;
    DWORD resume=fo2va(hook_fo)+(DWORD)disp_len;

    DWORD MAP,BIOS_SZ,TBL_SZ;
    if((a=find_sig(MAP_NEW,19,&c))>=0){
        /* MAP_NEW matches the NT3.5x/4.0 HalpScanForMP WRAPPER: it converts a byte size to pages, calls
         * the real mapper, then returns 0 (it only reports whether it saw an _MP_ table). Our ENUM needs
         * the mapped virtual address, which only the INNER mapper returns -- so follow the wrapper's own
         * call to it (the trailing 0xE8 of the signature) and pass PAGE counts, exactly like the NT3.1
         * path. Calling the wrapper instead left the ENUM scanning from address 0 -> ACPI never found
         * -> processor count never overridden -> "1 System Processor" despite the MP kernel. */
        long cf=a+18;                               /* trailing 0xE8 of MAP_NEW = call <inner mapper> */
        if(gbuf[cf]==0xe8) MAP=fo2va(cf)+5+rd32(cf+1);
        else MAP=fo2va(a);
        BIOS_SZ=0x20; TBL_SZ=2;                      /* page counts: 0x20000 B = 0x20 pg, 0x2000 B = 2 pg */
    }
    else if((a=find_sig(MAP_31,9,&c))>=0){ MAP=fo2va(a); BIOS_SZ=0x20; TBL_SZ=2; }
    else return 0;

    const BYTE*apt; int aplen,ap_ecall,ap_rjmp,ap_disp,nC; const int *apC,*apE,*apEB,*apID;
    if(form_mc){ apt=AP_MC_TPL; aplen=AP_MC_LEN; apC=AP_MC_COUNT;nC=3; apE=AP_MC_ENTRY; apEB=AP_MC_EBUF; apID=AP_MC_IDBUF; ap_disp=AP_MC_DISP[0]; ap_ecall=AP_MC_ECALL; ap_rjmp=AP_MC_RJMP; }
    else       { apt=AP_SE_TPL; aplen=AP_SE_LEN; apC=AP_SE_COUNT;nC=3; apE=AP_SE_ENTRY; apEB=AP_SE_EBUF; apID=AP_SE_IDBUF; ap_disp=AP_SE_DISP[0]; ap_ecall=AP_SE_ECALL; ap_rjmp=AP_SE_RJMP; }

    int want_mpfix=0,chk_combined=0; long check_fo=0,mark_fo=0;
    { long mrk=find_sig(MRK_SIG,7,&c);
      if(mrk>=0&&c==1){ long ck;
        if((ck=find_sig(CHK3_SIG,15,&c))>=0&&c==1){ check_fo=ck+10; chk_combined=0; want_mpfix=1; }
        else if((ck=find_sig(CHKCMB_SIG,10,&c))>=0&&c==1){ check_fo=ck+5; chk_combined=1; want_mpfix=1; }
        mark_fo=mrk;
      }
    }

    /* AP calibration speedup: locate the loop-count + post-loop sites (NT3.51/NT4 only) */
    long spd_loop=-1,spd_save=-1; int spd_len=0;
    { int sc; long sx;
      if((sx=find_sig(SIG_SPD_LOOP,14,&sc))>=0&&sc==1){ long sy;
        if((sy=find_sig(SIG_SPD_SAVE,13,&sc))>=0&&sc==1){ spd_loop=sx; spd_save=sy; } }
      if(spd_loop>=0&&spd_save>=0) spd_len=96; /* reserve for the two speedup stubs */
    }

    int enoff=(aplen+15)&~15;
    int chklen=chk_combined?CHKCMB_LEN:CHK3_LEN;
    int mpfixlen=want_mpfix?(chklen+MARK_LEN):0;
    DWORD cave_bytes=(DWORD)(enoff+ENUM_LEN+mpfixlen+spd_len);
    long base,scr;
    long ic=find_slack_cave((long)cave_bytes);          /* in-image executable slack (NT3.1/3.50/4.0) */
    if(ic<0) ic=find_cave((long)cave_bytes);            /* or an unreferenced code-section zero-run */
    /* NT 4.0's osloader frees the grown .reloc after relocation (NT 3.x's keeps it), so a cave appended
     * there faults; NT 4.0 (ImageBase 0x80010000) instead uses the AP-trampoline buffer tail, which lives
     * in .text and is always mapped. NT 3.1/3.50/3.51 (ImageBase 0x80400000) keep the proven grown cave. */
    if(ic<0 && image_base()!=0x80400000) ic=find_trampoline_cave((long)cave_bytes);
    if(ic>=0){
        base=ic;
        scr=find_scratch(192); if(scr<0) return 0;      /* writable .data zero-run for runtime scratch */
    } else {
        /* .text packed and .data all live globals (NT 3.51): grow the last section to hold BOTH the code
         * cave and the scratch in one mapped, writable, non-discardable region (no BSS corruption). */
        DWORD codesz=(cave_bytes+15)&~15u;
        long g=grow_last_section_cave(codesz+192+16);
        if(g>=0){ base=g; scr=((g+(long)codesz)+3)&~3L; }
        else {
            long t=find_trampoline_cave((long)cave_bytes);   /* last-ditch: AP-trampoline tail in .text */
            if(t>=0){ base=t; scr=find_scratch(192); if(scr<0) return 0; }
            else { base=add_section_cave(cave_bytes); if(base<0) return 0; scr=find_scratch(192); if(scr<0) return 0; }
        }
    }
    DWORD BMP=fo2va(scr), IDBUF_V=BMP+24, ENTRYBUF_V=BMP+32;
    long APo=base, ENo=base+enoff;
    DWORD APv=fo2va(APo), ENv=fo2va(ENo), p0v=APv+7;

    memcpy(gbuf+ENo,ENUM_TPL,ENUM_LEN);
    wr32(ENo+ENUM_BIOSSZ,BIOS_SZ); wr32(ENo+ENUM_TBLSZ[0],TBL_SZ); wr32(ENo+ENUM_TBLSZ[1],TBL_SZ);
    wr32(ENo+ENUM_DIDBUF, IDBUF_V-p0v);
    { int i; for(i=0;i<3;i++){ int o=ENUM_MAPCALL[i]; wr32(ENo+o, MAP-(ENv+(DWORD)o+4)); } }

    memcpy(gbuf+APo,apt,aplen);
    { int i; for(i=0;i<nC;i++) wr32(APo+apC[i], count_g-p0v); }
    wr32(APo+apE[0], entry_g-p0v);
    wr32(APo+apEB[0], ENTRYBUF_V-p0v); wr32(APo+apEB[1], ENTRYBUF_V-p0v);
    wr32(APo+apID[0], IDBUF_V-p0v);
    { int q0off=ap_disp-3; wr32(APo+ap_disp, disp_g-(APv+(DWORD)q0off)); }
    wr32(APo+ap_ecall, ENv-(APv+(DWORD)ap_ecall+4));
    wr32(APo+ap_rjmp,  resume-(APv+(DWORD)ap_rjmp+4));

    gbuf[hook_fo]=0xe9; wr32(hook_fo+1, APv-(fo2va(hook_fo)+5));
    { int k; for(k=5;k<disp_len;k++) gbuf[hook_fo+k]=0x90; }
    reloc_zap(fo2va(hook_fo)-image_base(), (DWORD)disp_len);  /* kill the finalize operand's stale fixup */

    if(want_mpfix){
        DWORD check_va=fo2va(check_fo),mark_va=fo2va(mark_fo),cont_va=check_va+5,done_va=mark_va+7;
        signed char jrel=(signed char)gbuf[check_fo+4];
        DWORD found_va=check_va+5+(DWORD)(long)jrel;
        long C1=ENo+ENUM_LEN,C2; DWORD C1v=fo2va(C1);
        if(chk_combined){
            memcpy(gbuf+C1,CHKCMB_TPL,CHKCMB_LEN); C2=C1+CHKCMB_LEN;
            wr32(C1+CHKCMB_ADDIMM,BMP-(C1v+(DWORD)CHKCMB_ADDIMM-3));
            wr32(C1+CHKCMB_JCONT,cont_va-(C1v+(DWORD)CHKCMB_JCONT+4));
            wr32(C1+CHKCMB_JFOUND,found_va-(C1v+(DWORD)CHKCMB_JFOUND+4));
            wr32(C1+CHKCMB_JCONT2,cont_va-(C1v+(DWORD)CHKCMB_JCONT2+4));
        } else {
            memcpy(gbuf+C1,CHK3_TPL,CHK3_LEN); C2=C1+CHK3_LEN;
            wr32(C1+CHK3_ADDIMM,BMP-(C1v+(DWORD)CHK3_ADDIMM-3));
            wr32(C1+CHK3_JCONT,cont_va-(C1v+(DWORD)CHK3_JCONT+4));
            wr32(C1+CHK3_JFOUND,found_va-(C1v+(DWORD)CHK3_JFOUND+4));
        }
        { DWORD C2v=fo2va(C2);
          memcpy(gbuf+C2,MARK_TPL,MARK_LEN);
          wr32(C2+MARK_ADDIMM,BMP-(C2v+(DWORD)MARK_ADDIMM-3));
          wr32(C2+MARK_JDONE,done_va-(C2v+(DWORD)MARK_JDONE+4));
          gbuf[check_fo]=0xe9; wr32(check_fo+1,C1v-(check_va+5));
          gbuf[mark_fo]=0xe9; wr32(mark_fo+1,C2v-(mark_va+5));
          reloc_zap(check_va-image_base(),5); reloc_zap(mark_va-image_base(),5);
        }
    }

    /* ---- emit AP calibration speedup (NT3.51/NT4) --------------------------------------------
     * Two position-independent stubs placed at the cave tail. G0/G1 are two scratch dwords holding
     * the BSP's cached raw TSC delta and APIC count; they start at 0 (sentinel). All scratch access
     * is PIC ('call $+5; pop ebx; [ebx+disp]') and all control flow is rel32, so no base-reloc is
     * needed on the added code even though the HAL relocates. */
    if(spd_len){
        long SPo=base+enoff+ENUM_LEN+mpfixlen;
        DWORD SPv=fo2va(SPo);
        DWORD G0=BMP+96, G1=BMP+100;
        gbuf[scr+96]=gbuf[scr+97]=gbuf[scr+98]=gbuf[scr+99]=0;   /* G_TSC  = 0 */
        gbuf[scr+100]=gbuf[scr+101]=gbuf[scr+102]=gbuf[scr+103]=0;/* G_APIC = 0 */
        DWORD esi_imm=rd32(spd_loop+1);                 /* preserve the build's loop count */
        DWORD loopbody=fo2va(spd_loop)+5;               /* resume original loop body */
        DWORD postloop=fo2va(spd_save);                 /* AP jumps into the (rewritten) save site */
        DWORD postsave=fo2va(spd_save)+6;               /* continue after the saved instruction */
        long o=SPo;
        /* SP_CHECK: */
        gbuf[o++]=0x53;                                 /* push ebx */
        gbuf[o++]=0xE8; wr32(o,0); o+=4;                /* call $+5 */
        DWORD anchor=SPv+(DWORD)(o-SPo);                /* = pushed ret addr = &pop (what ebx gets) */
        gbuf[o++]=0x5B;                                 /* pop ebx -> ebx=anchor */
        gbuf[o++]=0x83; gbuf[o++]=0xBB; wr32(o,G0-anchor); o+=4; gbuf[o++]=0x00; /* cmp [ebx+d],0 */
        long je_fix=o; gbuf[o++]=0x74; gbuf[o++]=0x00;  /* je SP_MEASURE (rel8) */
        gbuf[o++]=0x8B; gbuf[o++]=0x83; wr32(o,G0-anchor); o+=4; /* mov eax,[ebx+dT] */
        gbuf[o++]=0x8B; gbuf[o++]=0x8B; wr32(o,G1-anchor); o+=4; /* mov ecx,[ebx+dA] */
        gbuf[o++]=0x5B;                                 /* pop ebx (restore) */
        gbuf[o++]=0xE9; wr32(o,postloop-(SPv+(DWORD)(o+4-SPo))); o+=4; /* jmp postloop */
        gbuf[je_fix+1]=(BYTE)(o-(je_fix+2));            /* SP_MEASURE target */
        /* SP_MEASURE: */
        gbuf[o++]=0x5B;                                 /* pop ebx (restore) */
        gbuf[o++]=0xBE; wr32(o,esi_imm); o+=4;          /* mov esi,imm (original) */
        gbuf[o++]=0xE9; wr32(o,loopbody-(SPv+(DWORD)(o+4-SPo))); o+=4; /* jmp loopbody */
        /* SP_SAVE: */
        long SAVEo=o; DWORD SAVEv=SPv+(DWORD)(SAVEo-SPo);
        gbuf[o++]=0x53;                                 /* push ebx */
        gbuf[o++]=0xE8; wr32(o,0); o+=4;                /* call $+5 */
        DWORD anchor2=SPv+(DWORD)(o-SPo);
        gbuf[o++]=0x5B;                                 /* pop ebx -> ebx=anchor2 */
        gbuf[o++]=0x89; gbuf[o++]=0x83; wr32(o,G0-anchor2); o+=4; /* mov [ebx+dT],eax */
        gbuf[o++]=0x89; gbuf[o++]=0x8B; wr32(o,G1-anchor2); o+=4; /* mov [ebx+dA],ecx */
        gbuf[o++]=0x5B;                                 /* pop ebx (restore) */
        gbuf[o++]=0x64; gbuf[o++]=0xA3; wr32(o,0xA4); o+=4;       /* mov fs:[0a4h],eax (original) */
        gbuf[o++]=0xE9; wr32(o,postsave-(SPv+(DWORD)(o+4-SPo))); o+=4; /* jmp postsave */
        /* hook 1: 'mov esi,imm' -> jmp SP_CHECK (5 bytes, exact) */
        gbuf[spd_loop]=0xE9; wr32(spd_loop+1,SPv-(fo2va(spd_loop)+5));
        reloc_zap(fo2va(spd_loop)-image_base(),5);
        /* hook 2: 'mov fs:[0a4h],eax' (6 bytes) -> jmp SP_SAVE + nop */
        gbuf[spd_save]=0xE9; wr32(spd_save+1,SAVEv-(fo2va(spd_save)+5)); gbuf[spd_save+5]=0x90;
        reloc_zap(fo2va(spd_save)-image_base(),6);
    }

    hal_prcb_preserve();        /* STOP 0x0A / stuck-low-count race in HalpStartProcessor (NT3.51/4.0) */
    hal_tiled_cr3_fix();         /* AP tiled-CR3 premature-free teardown (trampoline triple-fault, VBox/VMs) */
    apic_verify_fix();
    nt31_fixes();
    return 1;
}

static int hal_status(void){
    int c;
    if(!export_va("HalInitSystem")) return 0;
    long le=find_sig(SIG_LOOPEXIT,7,&c);
    if(le<0||c!=1) return 0;
    long hook_fo=le+8;
    if(gbuf[hook_fo]==0xe9) return 2;
    if(gbuf[hook_fo]==0x8b||gbuf[hook_fo]==0xa3) return 1;
    return 0;
}

#endif
