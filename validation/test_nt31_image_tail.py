"""Execute production x86 tail boundaries in a freestanding QEMU kernel.
The mock kernel starts queued work immediately, including before queue return.
"""
from pathlib import Path
import ast,re,subprocess,tempfile
source=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/drvnt31.c').read_text()
def fn(name):
 m=re.search(r'static (?:VOID|PVOID|ULONG)(?: STDC)? '+name+r'\([^;{]*\)\s*\{',source);assert m,name
 end=m.end();depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[m.start():end]
struct=source[source.index('typedef struct _N31_IMAGE_OWNER'):source.index('static N31_IMAGE_OWNER *g_owner;')]
asm='.text\n.align 4\n.long 0x1badb002,0,-0x1badb002\n'
for name in ['n31_dpc_entry','n31_driver_unload','n31_reclaim_entry']:
 body=fn(name);body=body[body.index('__asm__(')+8:body.rindex(');')]
 body=re.sub(r'/\*.*?\*/','',body,flags=re.S)
 instructions=''.join(ast.literal_eval(x) for x in re.findall(r'"(?:\\.|[^"\\])*"',body))
 instructions=re.sub(r'_([A-Za-z0-9_]+)@\d+',r'\1',instructions).replace('_g_owner','g_owner')
 asm+='\n.global '+name+'\n'+name+':\n'+instructions+'\n'
asm+=r'''
.global kernel_call_dpc,after_dpc
kernel_call_dpc:
 pushl $44
 pushl $33
 pushl $22
 pushl $11
 call n31_dpc_entry
after_dpc:
 movl $0,current_irql
 ret
.global kernel_call_unload,after_unload
kernel_call_unload:
 pushl $99
 call n31_driver_unload
after_unload:
 ret
.global kernel_call_worker,after_worker
kernel_call_worker:
 pushl 4(%esp)
 call n31_reclaim_entry
after_worker:
 ret
.global _start
_start:
 movl $stack_top,%esp
 xorl %ebp,%ebp
 call testmain
 movl $0xf4,%edx
 outl %eax,%dx
1: hlt
 jmp 1b
.bss
.align 16
.skip 16384
stack_top:
.section .note.GNU-stack,"",@progbits
'''
prefix=r'''
#define VOID void
typedef void* PVOID;
typedef unsigned char* PUCHAR;
#define ULONG unsigned
typedef unsigned char KIRQL;
#define NULL ((void*)0)
#define STDC __attribute__((stdcall))
static void fail(int n){__asm__ volatile("outl %%eax,%%dx"::"a"(n),"d"(0xf4):"memory");__builtin_unreachable();}
#define CHECK(x) do{if(!(x))fail(__LINE__%250+1);}while(0)
'''
shim=r'''
static N31_IMAGE_OWNER owner;N31_IMAGE_OWNER*g_owner=&owner;
static unsigned char fake_image[1];static void*g_poll;
static unsigned freed_owner,freed_image,queued,polled,unloaded,rearm,pending;
unsigned current_irql;
extern char after_dpc,after_unload,after_worker;
extern void kernel_call_dpc(void),kernel_call_unload(void),kernel_call_worker(void*);
static void dbg(const char*s){(void)s;}
void STDC ExQueueWorkItem(void*,int);
void STDC KeAcquireSpinLock(unsigned*p,KIRQL*old){CHECK(p==&owner.Lock&&!*p&&!freed_owner);*p=1;*old=current_irql;current_irql=2;}
void STDC KeReleaseSpinLock(unsigned*p,KIRQL old){
 void*ret=__builtin_return_address(0);CHECK(p==&owner.Lock&&*p==1&&(old==0||old==2)&&!freed_owner);*p=0;current_irql=old;
 if(pending){unsigned saved=current_irql;CHECK(ret==&after_dpc||ret==&after_unload);pending=0;current_irql=0;kernel_call_worker(&owner);CHECK(freed_owner&&freed_image&&!current_irql);current_irql=saved;}
}
void STDC MmFreeContiguousMemory(void*p){
 CHECK(!current_irql);
 if(p==&owner){CHECK(!freed_owner&&!owner.References);freed_owner++;}
 else{CHECK(p==fake_image&&freed_owner&&!freed_image);CHECK(__builtin_return_address(0)==&after_worker);freed_image++;}
}
'''
# Symbols referenced only from the extracted assembly must survive optimization.
funcs='\n'.join(fn(x).replace('static ','__attribute__((used,noinline)) ',1) for x in ['n31_image_get','n31_release_prepare','n31_reclaim_prepare','n31_finish_driver_unload'])
main=r'''
static void STDC original_unload(void*p){CHECK(p==(void*)99);unloaded++;}
void STDC n31_dpc(void*a,void*b,void*c,void*d){CHECK(a==(void*)11&&b==(void*)22&&c==(void*)33&&d==(void*)44&&!freed_image);current_irql=2;polled++;if(rearm)n31_image_get();}
void STDC ExQueueWorkItem(void*item,int kind){
 CHECK(item==&owner&&kind==1&&!owner.References&&!queued&&!freed_owner&&owner.Lock);
 /* An immediately scheduled worker must wait for the final kernel unlock.
    The mock resumes it within that unlock, before its caller returns. */
 queued++;pending=1;
}
static void setup(unsigned refs){owner.References=refs;owner.Image=fake_image;owner.Unload=original_unload;g_poll=0;
 freed_owner=freed_image=queued=polled=unloaded=rearm=pending=owner.Lock=current_irql=0;}
int testmain(void){unsigned i;
 for(i=0;i<10000;i++){
  setup(2);kernel_call_dpc();CHECK(owner.References==1&&polled==1&&!queued);kernel_call_unload();CHECK(queued==1&&unloaded==1&&freed_image);
  setup(2);kernel_call_unload();CHECK(owner.References==1&&!queued);kernel_call_dpc();CHECK(queued==1&&freed_image);
  setup(2);rearm=1;kernel_call_dpc();CHECK(owner.References==2&&!queued);rearm=0;kernel_call_unload();kernel_call_dpc();CHECK(queued==1&&freed_image&&polled==2);
  setup(1);kernel_call_unload();CHECK(queued==1&&freed_image);
  setup(1);g_poll=&owner;kernel_call_unload();CHECK(!queued&&!freed_owner&&owner.References==1);g_poll=0;kernel_call_unload();CHECK(queued==1&&freed_image);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
 td=Path(td);(td/'test.c').write_text(prefix+struct+shim+funcs+main);(td/'thunks.S').write_text(asm)
 subprocess.run(['gcc','-m32','-O2','-mno-sse','-mno-mmx','-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables','-nostdlib','-no-pie','-Wl,--build-id=none','-Wl,-Ttext,0x100000','-Wl,-e,_start',str(td/'thunks.S'),str(td/'test.c'),'-o',str(td/'test')],check=True)
 result=subprocess.run(['/workspace/rltklant-tools/qemu-src/build/qemu-system-i386','-machine','pc','-cpu','pentium3','-m','32','-nodefaults','-display','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-kernel',str(td/'test')],timeout=30)
 assert result.returncode==1,('QEMU test exit',result.returncode)
print('PASS: 50000 actual x86 stdcall DPC/unload/reclaim schedules; worker joins final kernel unlock; exact kernel return addresses; stack balanced; image retained while live')
