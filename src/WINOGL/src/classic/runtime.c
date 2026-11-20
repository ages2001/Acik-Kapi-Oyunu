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
