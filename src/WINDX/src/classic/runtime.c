/* Supplemental x86 CRT calls used by the graphics libraries built with MinGW.
 * The matching allocation, stream, jump and environment functions all use
 * the same system CRTDLL instance. This DLL is private to the WINDX stack. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

typedef struct {
    unsigned long dev;
    unsigned short ino, mode;
    short links, uid, gid;
    unsigned long rdev;
    long size, atime, mtime, ctime;
} OldStat;
typedef struct {
    unsigned long dev;
    unsigned short ino, mode;
    short links, uid, gid;
    unsigned long rdev;
    long long size;
    long atime, mtime, ctime;
} LargeStat;
extern int __cdecl _fstat(int, OldStat *);
extern int __cdecl _stat(const char *, OldStat *);
extern intptr_t __cdecl _get_osfhandle(int);
#include <errno.h>

static CRITICAL_SECTION crt_locks[64];
static int error_mode;

BOOL WINAPI runtime_entry(HINSTANCE instance, DWORD reason, void *reserved)
{
    unsigned int i;
    if (reason == DLL_PROCESS_ATTACH)
        for (i = 0; i < 64; ++i) InitializeCriticalSection(&crt_locks[i]);
    else if (reason == DLL_PROCESS_DETACH && !reserved)
        for (i = 0; i < 64; ++i) DeleteCriticalSection(&crt_locks[i]);
    return TRUE;
}
void __cdecl wx_lock(int index) { EnterCriticalSection(&crt_locks[(unsigned)index % 64]); }
void __cdecl wx_unlock(int index) { LeaveCriticalSection(&crt_locks[(unsigned)index % 64]); }
int __cdecl wx_error_mode(int mode)
{
    int old;
    wx_lock(0);
    old = error_mode;
    if (mode >= 0 && mode <= 2) error_mode = mode;
    else if (mode != 3) { errno = EINVAL; old = -1; }
    wx_unlock(0);
    return old;
}
uintptr_t __cdecl wx_beginthread(void *security, unsigned stack, unsigned (__stdcall *start)(void *),
        void *arg, unsigned flags, unsigned *id)
{
    DWORD tid;
    HANDLE h;
    if (!start) { errno = EINVAL; return 0; }
    h = CreateThread(security, stack, (LPTHREAD_START_ROUTINE)start, arg, flags, &tid);
    if (!h) { errno = EAGAIN; return 0; }
    if (id) *id = tid;
    return (uintptr_t)h;
}
void __cdecl wx_endthread(unsigned result) { ExitThread(result); }

static void large_stat(LargeStat *to, const OldStat *from, HANDLE file)
{
    DWORD high = 0, low;
    to->dev = from->dev; to->ino = from->ino; to->mode = from->mode;
    to->links = from->links; to->uid = from->uid; to->gid = from->gid; to->rdev = from->rdev;
    to->size = from->size; to->atime = from->atime; to->mtime = from->mtime; to->ctime = from->ctime;
    if (file != INVALID_HANDLE_VALUE && GetFileType(file) == FILE_TYPE_DISK) {
        SetLastError(NO_ERROR);
        low = GetFileSize(file, &high);
        if (low != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
            to->size = ((unsigned long long)high << 32) | low;
    }
}
int __cdecl wx_fstati64(int fd, LargeStat *out)
{
    OldStat old;
    if (!out) { errno = EINVAL; return -1; }
    if (_fstat(fd, &old)) return -1;
    large_stat(out, &old, (HANDLE)_get_osfhandle(fd));
    return 0;
}
int __cdecl wx_stati64(const char *path, LargeStat *out)
{
    OldStat old;
    HANDLE file;
    if (!path || !out) { errno = EINVAL; return -1; }
    if (_stat(path, &old)) return -1;
    file = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    large_stat(out, &old, file);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return 0;
}

/* Match the optional math handler startup used by MinGW's CRTDLL target. */
extern void __cdecl __setusermatherr(void *);
void __cdecl wx_math_hook(void *handler) { __setusermatherr(handler); }

/* NT 3.1 CRTDLL's _setjmp reads the try level from fs:[0]+12 even when
 * fs:[0] is the end-of-chain marker. Save the standard x86 jump context
 * directly for this case. Real SEH registrations retain CRTDLL's handling. */
__asm__(
    ".text\n"
    ".globl _wx_setjmp3\n"
    "_wx_setjmp3:\n"
    "  movl %fs:0,%ecx\n"
    "  cmpl $-1,%ecx\n"
    "  je 1f\n"
    "  jmp *__imp___setjmp\n"
    "1:movl 4(%esp),%edx\n"
    "  movl %ebp,0(%edx)\n"
    "  movl %ebx,4(%edx)\n"
    "  movl %edi,8(%edx)\n"
    "  movl %esi,12(%edx)\n"
    "  movl %esp,16(%edx)\n"
    "  movl (%esp),%eax\n"
    "  movl %eax,20(%edx)\n"
    "  movl %ecx,24(%edx)\n"
    "  movl %ecx,28(%edx)\n"
    "  xorl %eax,%eax\n"
    "  ret\n"
    ".globl _wx_longjmp\n"
    "_wx_longjmp:\n"
    "  movl 4(%esp),%edx\n"
    "  cmpl $-1,24(%edx)\n"
    "  je 2f\n"
    "  jmp *__imp__longjmp\n"
    "2:cmpl $-1,%fs:0\n"
    "  je 3f\n"
    "  pushl $0\n"
    "  call *__imp___global_unwind2\n"
    "  addl $4,%esp\n"
    "  movl 4(%esp),%edx\n"
    "3:movl 8(%esp),%eax\n"
    "  testl %eax,%eax\n"
    "  jnz 4f\n"
    "  incl %eax\n"
    "4:movl 0(%edx),%ebp\n"
    "  movl 4(%edx),%ebx\n"
    "  movl 8(%edx),%edi\n"
    "  movl 12(%edx),%esi\n"
    "  movl 16(%edx),%esp\n"
    "  addl $4,%esp\n"
    "  jmp *20(%edx)\n");
