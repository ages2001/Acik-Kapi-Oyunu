/* Native integration test. Argument 0/1 is the expected selected mode. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "cpu_features.h"
static HANDLE logh;
static DWORD checks,failures;
static void line(const char *name,DWORD value) {
    char b[200];DWORD n=0,w;char digits[12];int count=0;
    while(*name)b[n++]=*name++;
    do{digits[count++]='0'+value%10;value/=10;}while(value);
    while(count)b[n++]=digits[--count];
    b[n++]='\r';b[n++]='\n';WriteFile(logh,b,n,&w,0);
}
static void check(BOOL ok,const char *name){checks++;if(!ok)failures++;line(name,ok);}
void WINAPI entry(void) {
    HMODULE dll,kernel;
    void(WINAPI *initialize)(void);
    BOOL(WINAPI *mode)(void);
    void(WINAPI *filter)(DWORD,DWORD*);
    BOOL(WINAPI *feature)(DWORD);
    char path[260],*cmd=GetCommandLineA();DWORD n=0,i,r[8],expect=2;
    GetModuleFileNameA(0,path,sizeof(path));
    while(path[n])n++;
    if(n>4){path[n-3]='L';path[n-2]='O';path[n-1]='G';}
    logh=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
    if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
    while(*cmd){if((*cmd=='0'||*cmd=='1')&&!cmd[1])expect=*cmd-'0';cmd++;}
    check(expect<=1,"Expected argument valid ");
    line("Hardware SSE2 ",(fp_cpu_edx()&(1u<<26))!=0);
    line("Context ready ",fp_xmm_context_ready());
    dll=LoadLibraryA("FP10CPU.DLL");
    check(dll!=0,"Mode DLL loaded ");
    initialize=dll?(void*)GetProcAddress(dll,"FpInitializeCpuMode"):0;
    mode=dll?(void*)GetProcAddress(dll,"FpSse2Enabled"):0;
    filter=dll?(void*)GetProcAddress(dll,"FpFilterCpuid"):0;
    check(initialize&&mode&&filter,"Mode exports present ");
    if(!initialize||!mode||!filter)goto done;
    check(mode()==FALSE,"Safe before initialization ");
    initialize();line("Selected SSE2 ",mode());
    check(mode()==expect,"Expected mode selected ");
    initialize();check(mode()==expect,"Initialization stable ");
    for(i=0;i<8;i++)r[i]=0xffffffffu;
    filter(1,r);
    check(r[5]==(expect?0xffffffffu:0xf9ffffffu),"CPUID SSE bits ");
    check(r[6]==0xc1e7edfcu,"CPUID newer features hidden ");
    check(r[0]==0xffffffffu&&r[1]==0xffffffffu&&r[2]==0xffffffffu&&r[3]==0xffffffffu&&r[4]==0xffffffffu&&r[7]==0xffffffffu,"Other registers retained ");
    filter(7,r);check(!(r[4]|r[5]|r[6]|r[7]),"Leaf 7 hidden ");
    kernel=LoadLibraryA("FPK.DLL");
    check(kernel!=0,"Kernel adapter loaded ");
    feature=kernel?(void*)GetProcAddress(kernel,"IsProcessorFeaturePresent"):0;
    check(feature!=0,"Feature export present ");
    if(feature){check(feature(PF_XMMI_INSTRUCTIONS_AVAILABLE)==expect,"SSE feature agrees ");
        check(feature(PF_XMMI64_INSTRUCTIONS_AVAILABLE)==expect,"SSE2 feature agrees ");}
done:
    line("Checks ",checks);line("Failures ",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
