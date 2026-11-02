/* Bounded copied-frame queue. The owner provides synchronization. */
#ifndef RTL_USB_QUEUE_H
#define RTL_USB_QUEUE_H
#define RTL_USB_QUEUE_DEPTH 16
typedef struct {
    UINT Head, Count;
    struct { UINT Length; UCHAR Bytes[RLTK_MAX_FRAME]; } Frames[RTL_USB_QUEUE_DEPTH];
} RTL_USB_QUEUE;
static BOOLEAN RtlUsbQueuePut(RTL_USB_QUEUE *q, const UCHAR *data, UINT len)
{
    UINT at;
    if (len < RLTK_HEADER_SIZE || len > RLTK_MAX_FRAME || q->Count == RTL_USB_QUEUE_DEPTH) return FALSE;
    at=(q->Head+q->Count)%RTL_USB_QUEUE_DEPTH;
    NdisMoveMemory(q->Frames[at].Bytes,data,len);q->Frames[at].Length=len;q->Count++;
    return TRUE;
}
static UINT RtlUsbQueueGet(RTL_USB_QUEUE *q, UCHAR *data)
{
    UINT len;
    if (!q->Count) return 0;
    len=q->Frames[q->Head].Length;NdisMoveMemory(data,q->Frames[q->Head].Bytes,len);
    q->Head=(q->Head+1)%RTL_USB_QUEUE_DEPTH;q->Count--;
    return len;
}
static VOID RtlUsbQueueClear(RTL_USB_QUEUE *q){q->Head=q->Count=0;}
#endif
