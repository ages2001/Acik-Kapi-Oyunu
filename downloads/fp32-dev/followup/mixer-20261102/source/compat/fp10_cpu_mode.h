/* The entry gate chooses one immutable mode before Adobe's entry point. */
#ifndef FP10_CPU_MODE_H
#define FP10_CPU_MODE_H
static BOOL fp10_selected_sse2(void) {
    typedef BOOL (WINAPI *ModeFn)(void);
    HMODULE module=GetModuleHandleA("FP10CPU.DLL");
    ModeFn mode=module?(ModeFn)GetProcAddress(module,"FpSse2Enabled"):0;
    return mode && mode();
}
#endif
