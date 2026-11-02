/* Validate real USB containers, including every truncated prefix with a
   recomputed digest so structural bounds cannot hide behind SHA failures. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "../RLTKLANT/USBNT/src/usbfw.h"
static unsigned digest_calls;
static int check(void*unused,const unsigned char*p,unsigned n,const unsigned char*expected)
{
    unsigned char d[32];int actual;(void)unused;digest_calls++;SHA256(p,n,d);
    actual=RtlUsbFwCheckDigest(NULL,p,n,expected);
    assert(actual==!memcmp(d,expected,32));return actual;
}
static void hashes(void)
{
    unsigned char *data=malloc(1000001),ref[32],got[32];unsigned i,n;
    assert(data);for(i=0;i<1000001;i++)data[i]=(unsigned char)(i*197U+(i>>7));
    assert(!RtlUsbFwSha256(NULL,1,got)&&!RtlUsbFwSha256(data,1,NULL));
    assert(RtlUsbFwSha256(NULL,0,got));SHA256(data,0,ref);assert(!memcmp(ref,got,32));
    for(n=0;n<=1024;n++){
        assert(RtlUsbFwSha256(data+1,n,got));SHA256(data+1,n,ref);assert(!memcmp(ref,got,32));
    }
    for(n=65503;n<=65569;n++){
        assert(RtlUsbFwSha256(data+1,n,got));SHA256(data+1,n,ref);assert(!memcmp(ref,got,32));
    }
    memset(data,'a',1000000);SHA256(data,1000000,ref);
    assert(RtlUsbFwSha256(data,1000000,got)&&!memcmp(ref,got,32));
    /* The one-shot API also supports the firmware's in-place digest field. */
    assert(RtlUsbFwSha256(data,1000000,data)&&!memcmp(ref,data,32));
    assert(!RtlUsbFwCheckDigest(NULL,data,1,NULL));free(data);
    puts("PASS: freestanding SHA256 vs OpenSSL; padding/block boundaries, unaligned input, million-a and overlapping output");
}
static void seal(unsigned char*p,unsigned n){if(n>=64)SHA256(p+32,n-32,p);}
static unsigned get32(const unsigned char*p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static void put32(unsigned char*p,unsigned n){p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24;}
int main(int argc,char**argv)
{
    unsigned file,cases=0;RTL_UFW_REPORT r;
    assert(argc>1&&(argc%2)==1);
    hashes();
    assert(RtlUsbFwValidate(NULL,64,4,check,NULL,&r)==RtlUsbFwArgument&&!digest_calls);
    for(file=1;file<(unsigned)argc;file+=2){
        FILE*f=fopen(argv[file],"rb");long length;unsigned n,i,at,v=(unsigned)atoi(argv[file+1]);unsigned char *data,*copy,*unaligned;
        assert(f);assert(!fseek(f,0,SEEK_END));length=ftell(f);assert(length>=64&&length<262144);rewind(f);n=(unsigned)length;
        data=malloc(n);copy=malloc(n);unaligned=malloc(n+1);assert(data&&copy&&unaligned);
        assert(fread(data,1,n,f)==n);fclose(f);
        if(RtlUsbFwValidate(data,n,v,check,NULL,&r)!=RtlUsbFwOk){fprintf(stderr,"%s version %u result %u offset %u\n",argv[file],v,r.Result,r.ErrorOffset);abort();}
        assert(r.Blocks&&r.PayloadBytes&&r.EndMarker&&!r.ErrorOffset);
        memcpy(unaligned+1,data,n);assert(!RtlUsbFwValidate(unaligned+1,n,v,check,NULL,&r));
        for(i=0;i<n;i++){
            unsigned char*shorter=malloc(i?i:1);assert(shorter);memcpy(shorter,data,i);seal(shorter,i);
            assert(RtlUsbFwValidate(shorter,i,v,check,NULL,&r)!=RtlUsbFwOk);free(shorter);cases++;
        }
        memcpy(copy,data,n);copy[n-1]^=1;
        assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)==RtlUsbFwDigest);
        assert(RtlUsbFwValidate(data,n,0,check,NULL,&r)==RtlUsbFwArgument);
        assert(RtlUsbFwValidate(data,n,18,check,NULL,&r)==RtlUsbFwArgument);
        assert(RtlUsbFwValidate(data,0xffffffffU,v,check,NULL,&r)==RtlUsbFwArgument);
        for(at=64;at<n;){
            unsigned size=get32(data+at+4),type=get32(data+at),j;
            const unsigned bad[]={0,1,7,0x7fffffffU,0xffffffffU};
            for(j=0;j<sizeof(bad)/sizeof(bad[0]);j++){
                memcpy(copy,data,n);put32(copy+at+4,bad[j]);seal(copy,n);
                assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)!=RtlUsbFwOk);cases++;
            }
            memcpy(copy,data,n);put32(copy+at,99);seal(copy,n);
            assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)==RtlUsbFwUnsupported);cases++;
            if(type==1||type==2||type==5||(type>=7&&type<=13)){
                memcpy(copy,data,n);copy[at+8]=copy[at+9]=0xff;seal(copy,n);
                assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)==RtlUsbFwBounds);cases++;
            }
            if(type==3){ /* no PHY patch may stop before it starts */
                memcpy(copy,data,n);put32(copy+at,4);seal(copy,n);
                assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)==RtlUsbFwSequence);cases++;
            }
            if(type==1||type==2||type==5){
                const unsigned offsets[]={10,12,16,20};
                for(j=0;j<4;j++){
                    memcpy(copy,data,n);copy[at+offsets[j]]^=1;seal(copy,n);
                    assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)==RtlUsbFwRegister);cases++;
                }
                memcpy(copy,data,n);copy[at+(type==5?30:22)]=17;seal(copy,n);
                assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)==RtlUsbFwBounds);cases++;
            }
            if(type>=7&&type<=12){
                for(j=56;j<=57;j++){
                    memcpy(copy,data,n);copy[at+j]=9;seal(copy,n);
                    assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)==RtlUsbFwBounds);cases++;
                }
            }
            if(type==6||type==14){
                memcpy(copy,data,n);copy[at+8]^=1;seal(copy,n);
                assert(RtlUsbFwValidate(copy,n,v,check,NULL,&r)==RtlUsbFwRegister);cases++;
            }
            at+=(size+7)&~7U;
        }
        printf("VALID USB %s version=%u\n",argv[file],v);free(unaligned);free(copy);free(data);
    }
    printf("PASS: USB firmware real inputs, %u rehashed structural faults/truncations, checksum and unaligned reads\n",cases);
    return 0;
}
