"""Production function retirement must not free a still-owned allocation."""
from pathlib import Path
import re, subprocess, tempfile
source=(Path(__file__).resolve().parents[1]/'USBNT/src/usbdi.c').read_text()
def function(name):
    m=re.search(r'static void '+name+r'\([^;{]*\)\s*\{', source);assert m
    end=m.end();depth=1
    while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[m.start():end]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int KIRQL;
#define TRUE 1
#define FALSE 0
typedef struct F {struct F *Next;void *Dev,*Cfg;int Dead,Started,Retiring;volatile int Pending;} UD_FUNC;
static UD_FUNC *Funcs;static int UdLock,locked,closed,freed;
static void KeAcquireSpinLock(int*l,KIRQL*i){(void)l;*i=0;assert(!locked);locked=1;}
static void KeReleaseSpinLock(int*l,KIRQL i){(void)l;(void)i;assert(locked);locked=0;}
static int InterlockedCompareExchange(volatile int*p,int value,int compare){return __sync_val_compare_and_swap(p,compare,value);}
static void ExFreePool(void*p){assert(p);free(p);freed++;}
static void ClosePipes(UD_FUNC*f,int which){assert(f->Dead&&!f->Started&&f->Dev&&which==255);closed++;}
static UD_FUNC*make(int pending){UD_FUNC*f=calloc(1,sizeof(*f));assert(f);f->Cfg=malloc(24);assert(f->Cfg);f->Pending=pending;f->Dev=f;f->Started=1;f->Next=Funcs;Funcs=f;return f;}
'''
main=r'''
int main(void){unsigned cycle,poll;int before;UD_FUNC *a,*b,*c;
 for(cycle=0;cycle<1000;cycle++){
  a=make(2);b=make(0);c=make(1);before=freed;
  FreeW(a);assert(a->Dead&&a->Retiring&&!a->Dev&&!a->Started&&freed==before);
  FreeW(b);assert(freed==before+2&&Funcs==c&&c->Next==a);
  FreeW(c);assert(freed==before+2);
  /* Well beyond the former 500 polls: ownership, not elapsed time, decides. */
  for(poll=0;poll<600;poll++)ReapRetired();
  assert(freed==before+2&&a->Pending==2&&c->Pending==1);
  a->Pending--;ReapRetired();assert(freed==before+2);
  c->Pending--;ReapRetired();assert(freed==before+4&&Funcs==a);
  a->Pending--;ReapRetired();assert(freed==before+6&&!Funcs);
  ReapRetired();assert(freed==before+6);
 }
 assert(closed==3000&&!locked);
 puts("PASS: 1000 production removal schedules, delayed work beyond old timeout, out-of-order rundown, immediate and deferred frees");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'test.c';exe=Path(td)/'test'
    p.write_text(prefix+function('DestroyFunc')+function('ReapRetired')+function('FreeW')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
