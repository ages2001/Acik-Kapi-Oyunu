/* Thread-local storage for dynamically loaded Windows CPU kernels. */
#ifndef WINOCL_KERNEL_TLS_H
#define WINOCL_KERNEL_TLS_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdint.h>

namespace FreeOCL {
class KernelThreadStorage {
    struct Value {
        Value *next;
        const void *key;
        void *data;
    };
    struct Thread {
        Thread *next;
        HANDLE handle;
        Value *values;
    };
    DWORD slot;
    CRITICAL_SECTION lock;
    Thread *threads;

    static void release(Thread *thread) {
        while (thread->values) {
            Value *value = thread->values;
            thread->values = value->next;
            HeapFree(GetProcessHeap(), 0, value);
        }
        CloseHandle(thread->handle);
        HeapFree(GetProcessHeap(), 0, thread);
    }
    static void allocation_failed() {
        RaiseException(STATUS_NO_MEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL);
    }
    Thread *attach() {
        Thread *thread = (Thread *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Thread));
        if (!thread) allocation_failed();
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &thread->handle,
                SYNCHRONIZE, FALSE, 0) || !TlsSetValue(slot, thread)) {
            if (thread->handle) CloseHandle(thread->handle);
            HeapFree(GetProcessHeap(), 0, thread);
            allocation_failed();
        }
        EnterCriticalSection(&lock);
        Thread **link = &threads;
        while (*link) {
            Thread *existing = *link;
            if (WaitForSingleObject(existing->handle, 0) == WAIT_OBJECT_0) {
                *link = existing->next;
                release(existing);
            } else {
                link = &existing->next;
            }
        }
        thread->next = threads;
        threads = thread;
        LeaveCriticalSection(&lock);
        return thread;
    }
public:
    KernelThreadStorage() : slot(TlsAlloc()), threads(NULL) {
        if (slot == TLS_OUT_OF_INDEXES) allocation_failed();
        InitializeCriticalSection(&lock);
    }
    ~KernelThreadStorage() {
        while (threads) {
            Thread *thread = threads;
            threads = thread->next;
            release(thread);
        }
        TlsFree(slot);
        DeleteCriticalSection(&lock);
    }
    void *get(const void *key, size_t size, size_t alignment) {
        Thread *thread = (Thread *)TlsGetValue(slot);
        if (!thread) thread = attach();
        for (Value *v = thread->values; v; v = v->next)
            if (v->key == key) return v->data;
        Value *v = (Value *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Value) + alignment - 1 + size);
        if (!v) allocation_failed();
        v->key = key;
        v->data = (void *)(((uintptr_t)(v + 1) + alignment - 1) & ~(uintptr_t)(alignment - 1));
        v->next = thread->values;
        thread->values = v;
        return v->data;
    }
};

/* Each translation unit owns its kernel-local symbols. All values are released
 * while that kernel module is still loaded, including those on live workers. */
static KernelThreadStorage kernel_thread_storage;
template<typename T, int Id> static inline T &kernel_local_value() {
    static char key;
    return *(T *)kernel_thread_storage.get(&key, sizeof(T), __alignof__(T));
}
}
#endif
