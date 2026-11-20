#include "winternl.h"
/* The original mixer uses the NTDLL resource API. These internal locks use
 * Win32 primitives available on both Windows 9x and NT 3.1. */
void wx_resource_init(RTL_RWLOCK *lock);
void wx_resource_delete(RTL_RWLOCK *lock);
BYTE wx_resource_write(RTL_RWLOCK *lock, BYTE wait);
BYTE wx_resource_read(RTL_RWLOCK *lock, BYTE wait);
void wx_resource_release(RTL_RWLOCK *lock);
#define RtlInitializeResource wx_resource_init
#define RtlDeleteResource wx_resource_delete
#define RtlAcquireResourceExclusive wx_resource_write
#define RtlAcquireResourceShared wx_resource_read
#define RtlReleaseResource wx_resource_release
