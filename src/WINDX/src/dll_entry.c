/* Keep callbacks registered by the statically linked MinGW math library
 * with the DLL that owns their code. Old CRTDLL has no per-module onexit table. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef void (__cdecl *exit_callback)(void);
struct exit_node {
    exit_callback callback;
    struct exit_node *next;
};

static struct exit_node *callbacks;
static CRITICAL_SECTION callback_lock;

int __cdecl atexit(exit_callback callback)
{
    struct exit_node *node = HeapAlloc(GetProcessHeap(), 0, sizeof(*node));
    if (!node)
        return -1;
    node->callback = callback;
    EnterCriticalSection(&callback_lock);
    node->next = callbacks;
    callbacks = node;
    LeaveCriticalSection(&callback_lock);
    return 0;
}

BOOL WINAPI windx_dll_main(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        InitializeCriticalSection(&callback_lock);
    if (reason == DLL_PROCESS_DETACH) {
        while (callbacks) {
            struct exit_node *node = callbacks;
            callbacks = node->next;
            node->callback();
            HeapFree(GetProcessHeap(), 0, node);
        }
        DeleteCriticalSection(&callback_lock);
    }
    return TRUE;
}
