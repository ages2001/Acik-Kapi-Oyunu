#include "kit.h"
#ifdef _WIN32
BOOL WkNtHost(void) { return !(GetVersion() & 0x80000000UL); }
static BOOL request(HANDLE file, BOOL nt, DWORD code, DWORD expected)
{
    DWORD reply = 0, bytes = 0;
    BOOL ok = DeviceIoControl(file, code, NULL, 0, nt ? &reply : NULL,
                              nt ? sizeof(reply) : 0, &bytes, NULL);
    return ok && (!nt || (bytes == sizeof(reply) && reply == expected));
}
static BOOL check(BOOL pci)
{
    BOOL nt = WkNtHost();
    HANDLE file = CreateFileA(nt ? "\\\\.\\WdmKit" : "\\\\.\\WDMKIT", 0, 0, NULL,
                              OPEN_EXISTING, 0, NULL);
    BOOL ok;
    if (file == INVALID_HANDLE_VALUE)
        return FALSE;
    ok = pci ? request(file,nt,WK_IOCTL_PCI,2) : request(file,nt,WK_IOCTL,WK_SIGNATURE) &&
         request(file,nt,WK_IOCTL_STACK,WK_STACK_BASE|WK_STACK_FILTER) &&
         request(file,nt,WK_IOCTL_DEFERRED,WK_SIGNATURE);
    CloseHandle(file);
    return ok;
}
int WkActiveTarget(void)
{
    HANDLE file;
    DWORD bytes;
    int i;
    file=CreateFileA(WkNtHost() ? "\\\\.\\WdmKit" : "\\\\.\\WDMKIT",0,0,NULL,OPEN_EXISTING,0,NULL);
    if (file==INVALID_HANDLE_VALUE) return -1;
    for (i=WK_TARGET_COUNT-1;i>=0;--i)
        if (DeviceIoControl(file,WK_IOCTL_TARGET_BASE+4UL*i,NULL,0,NULL,0,&bytes,NULL)) break;
    CloseHandle(file);
    return i;
}
BOOL WkKernelReady(void) { return check(FALSE); }
BOOL WkPciCheck(void) { return check(TRUE); }
#else
typedef unsigned (__far *Api)(void);
BOOL WkNtHost(void) { return FALSE; }
static unsigned rawQuery(unsigned operation)
{
    Api entry;
    unsigned answer;
    _asm {
        push es
        push di
        mov ax, 1684h
        mov bx, WK_VXD_ID
        xor di, di
        mov es, di
        int 2fh
        mov word ptr entry, di
        mov word ptr entry+2, es
        pop di
        pop es
    }
    if (!entry) return FALSE;
    _asm {
        mov ax, operation
        call dword ptr entry
        mov answer, ax
    }
    return answer;
}
int WkActiveTarget(void)
{
    unsigned version=rawQuery(WK_VXD_TARGET);
    int i;
    for (i=0;i<WK_TARGET_COUNT;++i) if (WkTargets[i].version==version) return i;
    return -1;
}
BOOL WkKernelReady(void) { return rawQuery(0)==WK_SIGNATURE; }
BOOL WkPciCheck(void) { return rawQuery(WK_VXD_PCI)==WK_SIGNATURE; }
#endif
