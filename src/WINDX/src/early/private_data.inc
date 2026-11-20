struct PrivateData {
    PrivateData *next;
    GUID tag;
    DWORD size, flags, uniqueness;
    IUnknown *object;
    BYTE data[];
};
static PrivateData **find_data(Surface *s, const GUID *tag)
{
    PrivateData **p=&s->private_data;
    while (*p && !wx_same_guid(&(*p)->tag,tag)) p=&(*p)->next;
    return p;
}
static void dispose_data(PrivateData *entry)
{
    if (entry->object) IUnknown_Release(entry->object);
    HeapFree(GetProcessHeap(),0,entry);
}
static void release_surface_data(Surface *s)
{
    while (s->private_data) {
        PrivateData *entry=s->private_data;
        s->private_data=entry->next;
        dispose_data(entry);
    }
}
HRESULT wx_private_set(IDirectDrawSurface *surface,const GUID *tag,void *data,DWORD bytes,DWORD flags)
{
    Surface *s=(Surface *)surface;
    BOOL object=(flags & DDSPD_IUNKNOWNPOINTER)!=0;
    if (!tag || !data || !bytes || flags & ~(DDSPD_IUNKNOWNPOINTER | DDSPD_VOLATILE) ||
        (object && bytes!=sizeof(IUnknown *)) || bytes>MAXDWORD-sizeof(PrivateData)) return DDERR_INVALIDPARAMS;
    PrivateData *entry=HeapAlloc(GetProcessHeap(),0,sizeof(*entry)+(object?0:bytes));
    if (!entry) return DDERR_OUTOFMEMORY;
    entry->tag=*tag; entry->flags=flags; entry->size=bytes; entry->object=object?(IUnknown *)data:NULL;
    if (object) IUnknown_AddRef(entry->object);
    else for (DWORD i=0;i<bytes;++i) entry->data[i]=((const BYTE *)data)[i];
    EnterCriticalSection(&s->owner->guard);
    PrivateData **place=find_data(s,tag), *old=*place;
    entry->uniqueness=s->uniqueness;
    entry->next=old?old->next:NULL;
    *place=entry;
    LeaveCriticalSection(&s->owner->guard);
    if (old) dispose_data(old);
    return DD_OK;
}
HRESULT wx_private_get(IDirectDrawSurface *surface,const GUID *tag,void *data,DWORD *bytes)
{
    Surface *s=(Surface *)surface;
    if (!tag || !bytes) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&s->owner->guard);
    PrivateData *entry=*find_data(s,tag);
    HRESULT hr=DDERR_NOTFOUND;
    if (entry) {
        if ((entry->flags&DDSPD_VOLATILE) && s->uniqueness!=entry->uniqueness) hr=DDERR_EXPIRED;
        else if (!data || *bytes<entry->size) { *bytes=entry->size; hr=DDERR_MOREDATA; }
        else {
            if (entry->object) {
                IUnknown_AddRef(entry->object);
                *(IUnknown **)data=entry->object;
            } else for (DWORD i=0;i<entry->size;++i) ((BYTE *)data)[i]=entry->data[i];
            *bytes=entry->size;
            hr=DD_OK;
        }
    }
    LeaveCriticalSection(&s->owner->guard);
    return hr;
}
HRESULT wx_private_free(IDirectDrawSurface *surface,const GUID *tag)
{
    Surface *s=(Surface *)surface;
    if (!tag) return DDERR_INVALIDPARAMS;
    EnterCriticalSection(&s->owner->guard);
    PrivateData **place=find_data(s,tag), *entry=*place;
    if (entry) *place=entry->next;
    LeaveCriticalSection(&s->owner->guard);
    if (!entry) return DDERR_NOTFOUND;
    dispose_data(entry);
    return DD_OK;
}
