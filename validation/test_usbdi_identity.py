"""Production PnP identity gating across delayed removal and port reuse.

No Windows PnP behavior is mocked as a guest result: this host test only
establishes that the planner cannot queue a conflicting live instance ID.
"""
from pathlib import Path
import re, subprocess, tempfile
source=(Path(__file__).resolve().parents[1]/'USBNT/src/usbdi.c').read_text()
def function(name):
    m=re.search(r'static BOOLEAN '+name+r'\([^;{]*\)\s*\{', source)
    assert m
    end=m.end();depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[m.start():end]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UCHAR;typedef int BOOLEAN;typedef uint32_t ULONG;
#define TRUE 1
#define FALSE 0
#define GET16(p) ((unsigned)(p)[0]|((unsigned)(p)[1]<<8))
typedef struct {int Index;} HC;
typedef struct {HC*Hc;int Index,Gen,RootPort,Port,Depth;UCHAR DevDesc[18];char Serial[32];} USB_DEV;
typedef struct F {struct F*Next;USB_DEV*Dev;int Gen,Dead,Whole,First,HcIndex,RootPort,Port,Depth;UCHAR DevDesc[18];char Serial[32];} UD_FUNC;
typedef struct {int Dev,Gen,Whole,First;} UD_WANT;
static UD_FUNC *Funcs;static UD_WANT Want[32];static int NWant;
'''
# SerialOk is an int-returning production helper.
m=re.search(r'static BOOLEAN SerialOk\([^;{]*\)\s*\{',source);assert m
end=m.end();depth=1
while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
main=r'''
int main(void){HC hc={3};USB_DEV d;UD_FUNC f;unsigned cycle;
 memset(&d,0,sizeof(d));memset(&f,0,sizeof(f));d.Hc=&hc;d.Index=4;d.Gen=8;
 d.RootPort=1;d.Port=2;d.Depth=1;d.DevDesc[8]=0xda;d.DevDesc[9]=0x0b;d.DevDesc[10]=0x53;d.DevDesc[11]=0x81;
 f.HcIndex=3;f.RootPort=1;f.Port=2;f.Depth=1;f.Whole=1;f.Gen=7;f.Dead=1;
 memcpy(f.DevDesc,d.DevDesc,18);strcpy(d.Serial,"Abc123");strcpy(f.Serial,"aBC123");Funcs=&f;
 /* The old physical pointer has already gone; cached identity still gates. */
 assert(!f.Dev&&SameInstance(&f,&d,0,1)&&Covered(&d,0,1));
 d.DevDesc[10]++;assert(!SameInstance(&f,&d,0,1));d.DevDesc[10]--;
 d.DevDesc[8]++;assert(!SameInstance(&f,&d,0,1));d.DevDesc[8]--;
 d.DevDesc[12]++;assert(SameInstance(&f,&d,0,1)); /* revision is not device ID */
 d.Serial[0]='z';assert(!SameInstance(&f,&d,0,1));d.Serial[0]='A';
 d.Port++;assert(SameInstance(&f,&d,0,1));d.Port--; /* valid unique serial */
 d.Serial[0]=0;assert(!SameInstance(&f,&d,0,1));f.Serial[0]=0;
 assert(SameInstance(&f,&d,0,1));
 d.Port++;assert(!SameInstance(&f,&d,0,1));d.Port--;
 d.RootPort++;assert(!SameInstance(&f,&d,0,1));d.RootPort--;
 d.Depth++;assert(!SameInstance(&f,&d,0,1));d.Depth--;
 hc.Index++;assert(!SameInstance(&f,&d,0,1));hc.Index--;
 f.Whole=0;f.First=2;assert(!SameInstance(&f,&d,2,1));
 assert(SameInstance(&f,&d,2,0)&&!SameInstance(&f,&d,3,0));
 strcpy(d.Serial,"A B");strcpy(f.Serial,"ignored");assert(SameInstance(&f,&d,2,0));
 f.Whole=1;f.Serial[0]=0;assert(SameInstance(&f,&d,0,1));
 d.Serial[0]=0;
 for(cycle=0;cycle<1000;cycle++){
  f.Dead=1;f.Dev=0;Funcs=&f;d.Gen++;
  assert(Covered(&d,0,1)); /* delayed REMOVE must not expose new PDO */
  Funcs=0;assert(!Covered(&d,0,1)); /* old FreeW allows next Plan */
  Want[0].Dev=d.Index;Want[0].Gen=d.Gen;Want[0].Whole=1;NWant=1;
  assert(Covered(&d,0,1));NWant=0;
  f.Dead=0;f.Dev=&d;f.Gen=d.Gen;Funcs=&f;assert(Covered(&d,0,1));
 }
 puts("PASS: identity/serial/topology/composite boundaries and 1000 delayed-PDO-removal generations");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'test.c';exe=Path(td)/'test'
    p.write_text(prefix+source[m.start():end]+function('SameInstance')+function('Covered')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
