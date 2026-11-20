/* Paired bus-relative and translated resource lists for native NT miniports. */
#define PC_NO_IMPORTS
#include "portcls_x86.h"

typedef struct {
    IResourceList Interface;
    KSPIN_LOCK ReferencesLock;
    KSEMAPHORE EditLock;
    ULONG References, Capacity, Entries;
    PCM_RESOURCE_LIST Translated, Raw;
} RESOURCES;

static const GUID iidResources = {0x22c6ac60,0x851b,0x11d0,{0x9a,0x7f,0,0xaa,0,0x38,0xac,0xfe}};
static const GUID iidUnknown = {0,0,0,{0xc0,0,0,0,0,0,0,0x46}};

static RESOURCES *object(IResourceList *iface)
{
    return CONTAINING_RECORD(iface, RESOURCES, Interface);
}

static ULONG NTAPI addRef(IResourceList *iface)
{
    RESOURCES *r = object(iface);
    return ExInterlockedAddUlong(&r->References, 1, &r->ReferencesLock) + 1;
}

static ULONG NTAPI release(IResourceList *iface)
{
    RESOURCES *r = object(iface);
    ULONG refs = ExInterlockedAddUlong(&r->References, (ULONG)-1, &r->ReferencesLock) - 1;
    if (!refs) {
        ExFreePool(r->Raw);
        ExFreePool(r->Translated);
        ExFreePool(r);
    }
    return refs;
}

static NTSTATUS NTAPI query(IResourceList *iface, REFIID iid, PVOID *out)
{
    if (!out) return STATUS_INVALID_PARAMETER;
    *out = NULL;
    if (!iid || (!IsEqualGUID(iid, &iidUnknown) && !IsEqualGUID(iid, &iidResources))) return STATUS_NOINTERFACE;
    addRef(iface);
    *out = iface;
    return STATUS_SUCCESS;
}

#define GROUP_HEADER FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList.PartialDescriptors)
#define LIST_HEADER FIELD_OFFSET(CM_RESOURCE_LIST, List)

/* A device-specific payload follows the last descriptor in its group. */
static ULONG groupBytes(PCM_FULL_RESOURCE_DESCRIPTOR group)
{
    ULONG count = group->PartialResourceList.Count, bytes, extra;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR last;
    if (count > (MAXULONG - GROUP_HEADER) / sizeof(*last)) return 0;
    bytes = GROUP_HEADER + count * sizeof(*last);
    if (!count) return bytes;
    last = &group->PartialResourceList.PartialDescriptors[count - 1];
    if (last->Type != CmResourceTypeDeviceSpecific) return bytes;
    extra = last->u.DeviceSpecificData.DataSize;
    if (extra > MAXULONG - bytes) return 0;
    return bytes + extra;
}

static NTSTATUS measure(PCM_RESOURCE_LIST list, PULONG bytes, PULONG entries)
{
    ULONG i, offset = LIST_HEADER, total = 0;
    if (!list) return STATUS_INVALID_PARAMETER;
    for (i = 0; i < list->Count; ++i) {
        PCM_FULL_RESOURCE_DESCRIPTOR group = (PVOID)((PUCHAR)list + offset);
        ULONG n = groupBytes(group);
        if (!n || n > MAXULONG - offset || group->PartialResourceList.Count > MAXULONG - total)
            return STATUS_INTEGER_OVERFLOW;
        total += group->PartialResourceList.Count;
        offset += n;
    }
    *bytes = offset;
    *entries = total;
    return STATUS_SUCCESS;
}

static VOID lock(RESOURCES *r)
{
    KeWaitForSingleObject(&r->EditLock, Executive, KernelMode, FALSE, NULL);
}

static VOID unlock(RESOURCES *r)
{
    KeReleaseSemaphore(&r->EditLock, IO_NO_INCREMENT, 1, FALSE);
}

static PCM_PARTIAL_RESOURCE_DESCRIPTOR findType(RESOURCES *r, CM_RESOURCE_TYPE type, ULONG index,
                                                BOOLEAN wantRaw, PULONG total)
{
    ULONG i, j, seen = 0;
    PCM_FULL_RESOURCE_DESCRIPTOR cooked = r->Translated->List, original = r->Raw->List;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR result = NULL;
    lock(r);
    for (i = 0; i < r->Translated->Count; ++i) {
        for (j = 0; j < cooked->PartialResourceList.Count; ++j) {
            PCM_PARTIAL_RESOURCE_DESCRIPTOR entry = &cooked->PartialResourceList.PartialDescriptors[j];
            if (entry->Type == type && seen++ == index && !total) {
                result = wantRaw ? &original->PartialResourceList.PartialDescriptors[j] : entry;
                goto done;
            }
        }
        cooked = (PVOID)((PUCHAR)cooked + groupBytes(cooked));
        original = (PVOID)((PUCHAR)original + groupBytes(original));
    }
done:
    unlock(r);
    if (total) *total = seen;
    return result;
}

static ULONG NTAPI number(IResourceList *iface)
{
    RESOURCES *r = object(iface);
    ULONG entries;
    lock(r);
    entries = r->Entries;
    unlock(r);
    return entries;
}
static ULONG NTAPI countType(IResourceList *iface, CM_RESOURCE_TYPE type)
{
    ULONG count;
    findType(object(iface), type, 0, FALSE, &count);
    return count;
}
static PCM_PARTIAL_RESOURCE_DESCRIPTOR NTAPI translated(IResourceList *iface, CM_RESOURCE_TYPE type, ULONG index)
{
    return findType(object(iface), type, index, FALSE, NULL);
}
static PCM_PARTIAL_RESOURCE_DESCRIPTOR NTAPI raw(IResourceList *iface, CM_RESOURCE_TYPE type, ULONG index)
{
    return findType(object(iface), type, index, TRUE, NULL);
}
static PCM_RESOURCE_LIST NTAPI translatedList(IResourceList *iface) { return object(iface)->Translated; }
static PCM_RESOURCE_LIST NTAPI rawList(IResourceList *iface) { return object(iface)->Raw; }

static NTSTATUS NTAPI append(IResourceList *iface, PCM_PARTIAL_RESOURCE_DESCRIPTOR cooked,
                             PCM_PARTIAL_RESOURCE_DESCRIPTOR original)
{
    RESOURCES *r = object(iface);
    if (!cooked || !original) return STATUS_INVALID_PARAMETER;
    /* A sublist reserves fixed-size slots. Variable private payloads stay in
     * their original resource list. */
    if ((cooked->Type == CmResourceTypeDeviceSpecific && cooked->u.DeviceSpecificData.DataSize) ||
        (original->Type == CmResourceTypeDeviceSpecific && original->u.DeviceSpecificData.DataSize))
        return STATUS_NOT_SUPPORTED;
    lock(r);
    if (r->Entries >= r->Capacity) {
        unlock(r);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    r->Translated->List[0].PartialResourceList.PartialDescriptors[r->Entries] = *cooked;
    r->Raw->List[0].PartialResourceList.PartialDescriptors[r->Entries] = *original;
    ++r->Entries;
    r->Raw->List[0].PartialResourceList.Count = r->Translated->List[0].PartialResourceList.Count = r->Entries;
    unlock(r);
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI appendFrom(IResourceList *iface, IResourceList *parent, CM_RESOURCE_TYPE type, ULONG index)
{
    if (!parent) return STATUS_INVALID_PARAMETER;
    return append(iface, parent->lpVtbl->FindTranslatedEntry(parent, type, index),
                  parent->lpVtbl->FindUntranslatedEntry(parent, type, index));
}

static IResourceListVtbl methods = { query, addRef, release, number, countType, translated, raw,
                                     append, appendFrom, translatedList, rawList };

static RESOURCES *allocate(POOL_TYPE pool, ULONG cookedSize, ULONG rawSize)
{
    RESOURCES *r = ExAllocatePool(NonPagedPool, sizeof(*r));
    if (!r) return NULL;
    r->Translated = ExAllocatePool(pool, cookedSize);
    r->Raw = ExAllocatePool(pool, rawSize);
    if (!r->Translated || !r->Raw) {
        if (r->Translated) ExFreePool(r->Translated);
        if (r->Raw) ExFreePool(r->Raw);
        ExFreePool(r);
        return NULL;
    }
    r->Interface.lpVtbl = &methods;
    r->References = 1;
    KeInitializeSpinLock(&r->ReferencesLock);
    KeInitializeSemaphore(&r->EditLock, 1, 1);
    return r;
}

NTSTATUS NTAPI PcNewResourceList(PRESOURCELIST *out, PUNKNOWN outer, POOL_TYPE pool,
                                 PCM_RESOURCE_LIST cooked, PCM_RESOURCE_LIST original)
{
    ULONG a, b, count, rawCount, i;
    PCM_FULL_RESOURCE_DESCRIPTOR left, right;
    RESOURCES *r;
    NTSTATUS status;
    if (!out) return STATUS_INVALID_PARAMETER;
    *out = NULL;
    if (outer) return STATUS_NOT_SUPPORTED;
    status = measure(cooked, &a, &count);
    if (!NT_SUCCESS(status)) return status;
    status = measure(original, &b, &rawCount);
    if (!NT_SUCCESS(status)) return status;
    if (count != rawCount || cooked->Count != original->Count) return STATUS_INVALID_PARAMETER;
    left = cooked->List;
    right = original->List;
    for (i = 0; i < cooked->Count; ++i) {
        if (left->PartialResourceList.Count != right->PartialResourceList.Count) return STATUS_INVALID_PARAMETER;
        left = (PVOID)((PUCHAR)left + groupBytes(left));
        right = (PVOID)((PUCHAR)right + groupBytes(right));
    }
    r = allocate(pool, a, b);
    if (!r) return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(r->Translated, cooked, a);
    RtlCopyMemory(r->Raw, original, b);
    r->Entries = r->Capacity = count;
    *out = &r->Interface;
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI PcNewResourceSublist(PRESOURCELIST *out, PUNKNOWN outer, POOL_TYPE pool,
                                    PRESOURCELIST parent, ULONG capacity)
{
    ULONG header = LIST_HEADER + GROUP_HEADER, bytes;
    PCM_RESOURCE_LIST cooked, original;
    RESOURCES *r;
    if (!out) return STATUS_INVALID_PARAMETER;
    *out = NULL;
    if (outer) return STATUS_NOT_SUPPORTED;
    if (!parent) return STATUS_INVALID_PARAMETER;
    cooked = parent->lpVtbl->TranslatedList(parent);
    original = parent->lpVtbl->UntranslatedList(parent);
    if (!cooked || !original || cooked->Count != 1 || original->Count != 1) return STATUS_NOT_SUPPORTED;
    if (capacity > (MAXULONG - header) / sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)) return STATUS_INTEGER_OVERFLOW;
    bytes = header + capacity * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
    r = allocate(pool, bytes, bytes);
    if (!r) return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(r->Translated, cooked, header);
    RtlCopyMemory(r->Raw, original, header);
    r->Translated->List[0].PartialResourceList.Count = r->Raw->List[0].PartialResourceList.Count = 0;
    r->Capacity = capacity;
    r->Entries = 0;
    *out = &r->Interface;
    return STATUS_SUCCESS;
}
