/* Shared CPU/OS policy. Compile this header with the 386 instruction baseline. */
#ifndef FP_CPU_FEATURES_H
#define FP_CPU_FEATURES_H
#include <windows.h>
#include "../winsimd-1.2/include/guard_query.h"
static DWORD fp_cpu_edx(void) {
    DWORD before,after,a,b,c,d;
    __asm__ volatile("pushfl; popl %0":"=r"(before));
    after=before^(1u<<21);
    __asm__ volatile("pushl %0; popfl"::"r"(after):"cc","memory");
    __asm__ volatile("pushfl; popl %0":"=r"(after));
    __asm__ volatile("pushl %0; popfl"::"r"(before):"cc","memory");
    if(!((before^after)&(1u<<21)))return 0;
    a=0;c=0;
    __asm__ volatile("cpuid":"+a"(a),"=b"(b),"+c"(c),"=d"(d));
    if(a<1)return 0;
    a=1;c=0;
    __asm__ volatile("cpuid":"+a"(a),"=b"(b),"+c"(c),"=d"(d));
    return d;
}
static BOOL fp_guard_status_valid(const FpGuardStatus *s,DWORD bytes) {
    return bytes==sizeof(*s) && s->size==sizeof(*s) && s->magic==FP_GUARD_MAGIC &&
        s->abi==FP_GUARD_ABI && s->flags==FP_GUARD_XMM_MXCSR &&
        s->processors>=1 && s->processors<=8 && s->thread_capacity==512 &&
        (s->kernel_timestamp==0x2c921d20u || s->kernel_timestamp==0x2c51c08au);
}
static BOOL fp_xmm_context_ready(void) {
    typedef BOOL (WINAPI *FeatureFn)(DWORD);
    DWORD version=GetVersion(),bytes=0;
    HANDLE device;
    FpGuardStatus s={0};
    /* Use the real kernel's support report on native NT5+ only. A CPUID bit
       cannot establish whether a legacy scheduler preserves XMM state. */
    if(!(version&0x80000000u) && (version&255)>=5) {
        FeatureFn feature=(FeatureFn)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),
                                                   "IsProcessorFeaturePresent");
        return feature && feature(PF_XMMI_INSTRUCTIONS_AVAILABLE);
    }
    device=CreateFileA("\\\\.\\FpXmmGuard",0,FILE_SHARE_READ|FILE_SHARE_WRITE,
                       0,OPEN_EXISTING,0,0);
    if(device==INVALID_HANDLE_VALUE)return FALSE;
    if(!DeviceIoControl(device,FP_GUARD_IOCTL,0,0,&s,sizeof(s),&bytes,0))bytes=0;
    CloseHandle(device);
    return fp_guard_status_valid(&s,bytes);
}
static BOOL fp_sse_feature(DWORD feature) {
    DWORD flag=feature==PF_XMMI_INSTRUCTIONS_AVAILABLE ? (1u<<25) : (1u<<26);
    return (fp_cpu_edx()&flag)!=0 && fp_xmm_context_ready();
}
#endif
