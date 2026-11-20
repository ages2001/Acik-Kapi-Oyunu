/* NT4 x86 storage-class calling conventions, implemented with the NT 3.x DDK. */
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
typedef long S32;
#define NT __attribute__((stdcall))
#define FAST __attribute__((fastcall))
#define LABEL(name) __asm__("_" #name)
extern void *NT get_pool(U32, U32) LABEL(ExAllocatePool);
extern void NT free_pool(void *) LABEL(ExFreePool);
extern void NT raise_irql(U8, U8 *) LABEL(KeRaiseIrql);
extern void NT lower_irql(U8) LABEL(KeLowerIrql);
extern void NT lock_acquire(U32 *, U8 *) LABEL(KeAcquireSpinLock);
extern void NT lock_release(U32 *, U8) LABEL(KeReleaseSpinLock);
extern S32 NT call_driver(void *, void *) LABEL(IoCallDriver);
extern void NT complete(void *, signed char) LABEL(IoCompleteRequest);
extern void NT deref(void *) LABEL(ObDereferenceObject);
extern long NT reset(void *) LABEL(KeResetEvent);
extern void *NT ins_head(void *, void *, U32 *) LABEL(ExInterlockedInsertHeadList);
extern void *NT ins_tail(void *, void *, U32 *) LABEL(ExInterlockedInsertTailList);
extern void *NT rem_head(void *, U32 *) LABEL(ExInterlockedRemoveHeadList);
#define FN(ret, cc, name, args)                                                                    \
    ret cc name args LABEL(name);                                                                  \
    ret cc name args
FN(void *, NT, N3ExAllocatePoolWithTag, (U32 type, U32 size, U32 tag)) {
    (void)tag;
    return get_pool(type, size);
}
FN(U8, FAST, N3KfRaiseIrql, (U8 n)) {
    U8 old;
    raise_irql(n, &old);
    return old;
}
FN(void, FAST, N3KfLowerIrql, (U8 n)) { lower_irql(n); }
FN(S32, FAST, N3IofCallDriver, (void *d, void *irp)) { return call_driver(d, irp); }
FN(void, FAST, N3IofCompleteRequest, (void *irp, signed char boost)) { complete(irp, boost); }
FN(void, FAST, N3ObfDereferenceObject, (void *object)) { deref(object); }
FN(void, NT, N3KeClearEvent, (void *object)) { (void)reset(object); }
FN(void *, FAST, N3ExfInterlockedInsertHeadList, (void *head, void *item, U32 *lock)) {
    return ins_head(head, item, lock);
}
FN(void *, FAST, N3ExfInterlockedInsertTailList, (void *head, void *item, U32 *lock)) {
    return ins_tail(head, item, lock);
}
FN(void *, FAST, N3ExfInterlockedRemoveHeadList, (void *head, U32 *lock)) {
    return rem_head(head, lock);
}
FN(S32, FAST, N3InterlockedIncrement, (S32 * p)) { return __sync_add_and_fetch(p, 1); }
FN(S32, FAST, N3InterlockedDecrement, (S32 * p)) { return __sync_sub_and_fetch(p, 1); }
FN(void *, , N3memmove, (void *out, const void *in, U32 n)) {
    U8 *d = out;
    const U8 *s = in;
    if ((U32)d > (U32)s && (U32)d - (U32)s < n) {
        while (n) {
            --n;
            d[n] = s[n];
        }
    } else
        for (U32 i = 0; i < n; i++)
            d[i] = s[i];
    return out;
}
FN(int, , N3strncmp, (const char *a, const char *b, U32 n)) {
    for (U32 i = 0; i < n; i++) {
        U8 x = (U8)a[i], y = (U8)b[i];
        if (x != y || !x)
            return (int)x - (int)y;
    }
    return 0;
}
typedef struct Entry {
    struct Entry *next;
} Entry;
typedef struct {
    Entry *first;
    U16 count, sequence;
} Head;
typedef void *(NT *Allocate)(U32, U32, U32);
typedef void(NT *Free)(void *);
typedef struct {
    Head list;
    U16 depth, maximum;
    U32 allocs, alloc_misses, frees, free_misses, type, tag, size;
    Allocate alloc;
    Free free;
    void *forward, *back;
    U32 last_allocs, last_misses, future[2], lock;
} Lookaside;
_Static_assert(__builtin_offsetof(Lookaside, lock) == 0x48, "NT4 i386 lookaside layout");
FN(Entry *, FAST, N3ExInterlockedPopEntrySList, (Head * head, U32 *lock)) {
    U8 old;
    Entry *first;
    lock_acquire(lock, &old);
    first = head->first;
    if (first) {
        head->first = first->next;
        --head->count;
        ++head->sequence;
    }
    lock_release(lock, old);
    return first;
}
FN(Entry *, FAST, N3ExInterlockedPushEntrySList, (Head * head, Entry *item, U32 *lock)) {
    U8 old;
    Entry *first;
    lock_acquire(lock, &old);
    first = head->first;
    item->next = first;
    head->first = item;
    ++head->count;
    ++head->sequence;
    lock_release(lock, old);
    return first;
}
FN(void, NT, N3ExInitializeNPagedLookasideList,
   (Lookaside * l, Allocate a, Free f, U32 flags, U32 size, U32 tag, U16 depth)) {
    for (U32 i = 0; i < sizeof(*l); ++i)
        ((U8 *)l)[i] = 0;
    (void)depth;
    l->depth = 4;
    l->maximum = 256;
    l->type = flags;
    l->tag = tag;
    l->size = size;
    l->alloc = a ? a : N3ExAllocatePoolWithTag;
    l->free = f ? f : free_pool;
    l->forward = l->back = &l->forward;
}
FN(void, NT, N3ExDeleteNPagedLookasideList, (Lookaside * l)) {
    Entry *e;
    while ((e = N3ExInterlockedPopEntrySList(&l->list, &l->lock)) != 0)
        l->free(e);
}
extern S32 NT class_init(void *, void *) LABEL(N3CLInit);
extern S32 NT cd_init(void *, void *) LABEL(N3CDInit);
FN(S32, NT, N3Entry, (void *driver, void *path)) {
    S32 rc = class_init(driver, path);
    return rc < 0 ? rc : cd_init(driver, path);
}
