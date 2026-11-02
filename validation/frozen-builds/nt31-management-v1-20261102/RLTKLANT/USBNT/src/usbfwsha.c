/* SPDX-License-Identifier: GPL-2.0-only
 * SHA-256 for immutable USB firmware containers (FIPS 180-4, section 6.2).
 * No allocation, CRT, platform cryptography service or 64-bit arithmetic.
 * This checksum detects corruption; it does not authenticate a publisher.
 */
#include "usbfw.h"
static RTL_UFW_U32 rotate(RTL_UFW_U32 n,unsigned s){return (n>>s)|(n<<(32-s));}
static RTL_UFW_U32 big(const RTL_UFW_U8*p)
{
    return (RTL_UFW_U32)p[0]<<24|(RTL_UFW_U32)p[1]<<16|
           (RTL_UFW_U32)p[2]<<8|(RTL_UFW_U32)p[3];
}
static void put(RTL_UFW_U8*p,RTL_UFW_U32 n)
{
    p[0]=(RTL_UFW_U8)(n>>24);p[1]=(RTL_UFW_U8)(n>>16);
    p[2]=(RTL_UFW_U8)(n>>8);p[3]=(RTL_UFW_U8)n;
}
static void block(RTL_UFW_U32*h,const RTL_UFW_U8*p)
{
    static const RTL_UFW_U32 k[64]={
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,
        0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,
        0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,
        0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,
        0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,
        0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,
        0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,
        0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,
        0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    RTL_UFW_U32 w[16],a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
    unsigned i;
    for(i=0;i<64;i++){
        RTL_UFW_U32 t1,t2;
        if(i<16)w[i]=big(p+4*i);
        else{
            RTL_UFW_U32 x=w[(i-15)&15],y=w[(i-2)&15];
            w[i&15]+=(rotate(x,7)^rotate(x,18)^(x>>3))+w[(i-7)&15]+
                       (rotate(y,17)^rotate(y,19)^(y>>10));
        }
        t1=v+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+k[i]+w[i&15];
        t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
        v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
}
int RtlUsbFwSha256(const RTL_UFW_U8*data,RTL_UFW_U32 size,RTL_UFW_U8*out)
{
    RTL_UFW_U32 h[8]={0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                     0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    RTL_UFW_U8 tail[64];RTL_UFW_U32 remaining=size;unsigned n,i;
    if(!out||(!data&&size))return 0;
    while(remaining>=64){block(h,data);data+=64;remaining-=64;}
    for(i=0;i<remaining;i++)tail[i]=data[i];
    n=remaining;tail[n++]=0x80;
    if(n>56){while(n<64)tail[n++]=0;block(h,tail);n=0;}
    while(n<56)tail[n++]=0;
    put(tail+56,size>>29);put(tail+60,size<<3);block(h,tail);
    for(i=0;i<8;i++)put(out+4*i,h[i]);
    return 1;
}
int RtlUsbFwCheckDigest(void*context,const RTL_UFW_U8*data,RTL_UFW_U32 size,
                        const RTL_UFW_U8*expected)
{
    RTL_UFW_U8 hash[32];unsigned i,different=0;(void)context;
    if(!expected||!RtlUsbFwSha256(data,size,hash))return 0;
    for(i=0;i<32;i++)different|=hash[i]^expected[i];
    return !different;
}
