#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fwvm.h"
typedef struct {unsigned calls,fail,cancel,read,delays,nwrite;unsigned mode[32],reg[32],value[32];} State;
static int readword(void *p,int mode,RTL_FW_U16 reg,RTL_FW_U16 *v){
    State*s=p;(void)mode;(void)reg;s->calls++;if(s->calls==s->fail)return 0;*v=(RTL_FW_U16)s->read;return 1;
}
static int writeword(void *p,int mode,RTL_FW_U16 reg,RTL_FW_U16 v){
    State*s=p;s->calls++;if(s->calls==s->fail)return 0;assert(s->nwrite<32);
    s->mode[s->nwrite]=mode;s->reg[s->nwrite]=reg;s->value[s->nwrite++]=v;return 1;
}
static int delay(void*p,RTL_FW_U16 ms){State*s=p;s->calls++;if(s->calls==s->fail)return 0;s->delays+=ms;return 1;}
static int cancelled(void*p){State*s=p;return s->cancel&&s->calls>=s->cancel;}
static void put(unsigned char*p,unsigned v){unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static unsigned char code[1024];
static unsigned pack(const unsigned *words,unsigned n){unsigned i;for(i=0;i<n;i++)put(code+4*i,words[i]);return n*4;}
static enum RTL_FW_RESULT run(const unsigned*w,unsigned n,State*s,RTL_FW_REPORT*r,unsigned budget,unsigned wait){
    RTL_FW_IO io={s,readword,writeword,delay,cancelled,budget,wait};return RtlFwExecute(code,pack(w,n),&io,r);
}
int main(int argc,char **argv){
    State s={0};RTL_FW_REPORT r;unsigned n,i,size;FILE*f;unsigned char *bytes;
    const unsigned program[]={0x70000000,0x00010000,0x1000000f,0x200000ff,0xc0020000,
      0x40000001,0x801e1234,0x40000000,0xe0000005,0x90000001,0x8003dead,
      0xa00100af,0x8004dead,0xb0010000,0x8005dead,0xd0010000,0x8006dead,0x8007beef};
    s.read=0x12a0;assert(run(program,18,&s,&r,100,10)==RtlFwOk);
    assert(s.nwrite==3&&s.value[0]==0xaf&&s.mode[0]==0&&s.reg[0]==2);
    assert(s.value[1]==0x1234&&s.mode[1]==1&&s.reg[1]==30&&s.value[2]==0xbeef&&s.mode[2]==0);
    assert(r.DelayMs==5&&s.delays==5&&r.FailedIndex==~0U);
    n=s.calls;
    for(i=1;i<=n;i++){memset(&s,0,sizeof(s));s.read=0x12a0;s.fail=i;
      assert(run(program,18,&s,&r,100,10)==RtlFwIoError&&s.calls==i);}
    memset(&s,0,sizeof(s));s.read=0x12a0;s.cancel=2;
    assert(run(program,18,&s,&r,100,10)==RtlFwCancelled&&s.calls==2);
    memset(&s,0,sizeof(s));s.read=0x12a0;
    assert(run(program,18,&s,&r,100,4)==RtlFwBudgetExceeded&&r.DelayMs==0);
    {const unsigned loop[]={0x70000000,0x00010000,0x90000003,0x30020000,0x80021234};
      memset(&s,0,sizeof(s));assert(run(loop,5,&s,&r,30,0)==RtlFwOk&&s.calls==4);
      assert(run(loop,5,&s,&r,4,0)==RtlFwBudgetExceeded);}
    {const unsigned loop[]={0x30000000};memset(&s,0,sizeof(s));
      assert(run(loop,1,&s,&r,1000,0)==RtlFwBudgetExceeded&&r.Executed==1000&&!s.calls);}
    {const unsigned variants[]={0x50000000,0x60000000,0xf0000000,0x30010000,
       0x40000002,0x90000000,0xa0010000,0xb0010000,0xd0010000};
      for(i=0;i<sizeof(variants)/sizeof(*variants);i++){
        memset(&s,0,sizeof(s));assert(run(variants+i,1,&s,&r,100,100)==RtlFwInvalidCommand&&!s.calls);
      }}
    /* Header bounds/checksum, deliberately unaligned code, and full preflight. */
    memset(code,0,sizeof(code));put(code+36,45);put(code+40,2);
    put(code+45,0x8001abcd);put(code+49,0xf0000000);
    for(i=0;i<53;i++)code[44]=(unsigned char)(code[44]-code[i]*(i!=44));
    assert(RtlFwInspect(code,53,&r)==RtlFwInvalidCommand&&r.FailedIndex==1);
    code[52]=0x80;code[44]+=0x70;
    assert(RtlFwInspect(code,53,&r)==RtlFwOk);
    code[4]^=1;assert(RtlFwInspect(code,53,&r)==RtlFwInvalidImage);code[4]^=1;
    for(i=0;i<45;i++)assert(RtlFwInspect(code,i,&r)==RtlFwInvalidImage);
    assert(RtlFwInspect(NULL,4,&r)==RtlFwInvalidImage);
    assert(RtlFwInspect(code,53,NULL)==RtlFwInvalidArgument);
    for(i=1;i<(unsigned)argc;i++){
      f=fopen(argv[i],"rb");assert(f);assert(!fseek(f,0,SEEK_END));size=(unsigned)ftell(f);rewind(f);
      bytes=malloc(size+1);assert(bytes);assert(fread(bytes+1,1,size,f)==size);fclose(f);
      assert(RtlFwInspect(bytes+1,size,&r)==RtlFwOk);
      printf("VALID %s commands=%u version=%s\n",argv[i],r.CommandCount,r.Version);free(bytes);
    }
    puts("PASS: PCI firmware opcodes, branch targets, mode switches, I/O failures, cancellation, budgets and header bounds");
    return 0;
}
