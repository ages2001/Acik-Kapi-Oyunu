/* XP video-port DDC2B monitor reads for an NT 4.0 display miniport.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * SCL and SDA are open-drain lines. A one releases the line. The
 * miniport provides the board-specific input/output callbacks.
 */
#include <ntdef.h>
#include <ddk/miniport.h>
#include <ddk/dderror.h>
#include <ddk/video.h>

#define BLOCK_SIZE 128
#define HALF_PERIOD_US 5

typedef struct {
    PVOID device;
    const I2C_CALLBACKS *pins;
} BUS;

static void data(BUS *bus,UCHAR level)
{
    bus->pins->WriteDataLine(bus->device,level);
    VideoPortStallExecution(HALF_PERIOD_US);
}
static BOOLEAN clock_line(BUS *bus,UCHAR level)
{
    unsigned i;
    bus->pins->WriteClockLine(bus->device,level);
    if (level) {
        for (i=0;i<2000 && !bus->pins->ReadClockLine(bus->device);++i)
            VideoPortStallExecution(HALF_PERIOD_US);
        if (i==2000) return FALSE;
    }
    VideoPortStallExecution(HALF_PERIOD_US);
    return TRUE;
}
static BOOLEAN start(BUS *bus)
{
    data(bus,1);
    if (!clock_line(bus,1) || !bus->pins->ReadDataLine(bus->device)) return FALSE;
    data(bus,0);
    return clock_line(bus,0);
}
static void stop(BUS *bus)
{
    data(bus,0);
    clock_line(bus,1);
    data(bus,1);
}
static BOOLEAN send(BUS *bus,UCHAR value)
{
    unsigned mask;
    BOOLEAN acknowledged;
    for (mask=0x80; mask; mask>>=1) {
        data(bus,(value&mask)!=0);
        if (!clock_line(bus,1)) return FALSE;
        clock_line(bus,0);
    }
    data(bus,1);
    if (!clock_line(bus,1)) return FALSE;
    acknowledged=!bus->pins->ReadDataLine(bus->device);
    clock_line(bus,0);
    return acknowledged;
}
static BOOLEAN read_byte(BUS *bus,UCHAR *out,BOOLEAN last)
{
    unsigned i;
    UCHAR value=0;
    data(bus,1);
    for (i=0;i<8;++i) {
        if (!clock_line(bus,1)) return FALSE;
        value=(UCHAR)((value<<1) | !!bus->pins->ReadDataLine(bus->device));
        clock_line(bus,0);
    }
    data(bus,last?1:0);
    if (!clock_line(bus,1)) return FALSE;
    clock_line(bus,0);
    data(bus,1);
    *out=value;
    return TRUE;
}

BOOLEAN NTAPI WxpReadMonitor(PVOID device,PVOID parameters,PUCHAR out,ULONG size)
{
    static const UCHAR header[8]={0,255,255,255,255,255,255,0};
    DDC_CONTROL *control=(DDC_CONTROL*)parameters;
    BUS bus;
    UCHAR bytes[256],sum=0,segment=0;
    ULONG i, length;
    BOOLEAN ok;
    if (!control || control->Size<FIELD_OFFSET(DDC_CONTROL,EdidSegment) ||
        !out || size<BLOCK_SIZE) return FALSE;
    bus.device=device; bus.pins=&control->I2CCallbacks;
    if (!bus.pins->WriteClockLine || !bus.pins->WriteDataLine ||
        !bus.pins->ReadClockLine || !bus.pins->ReadDataLine) return FALSE;
    if (control->Size>=sizeof(DDC_CONTROL)) segment=control->EdidSegment;
    if (segment && !(start(&bus) && send(&bus,0x60) && send(&bus,segment))) {
        stop(&bus);
        return FALSE;
    }
    length=size>=sizeof(bytes)?sizeof(bytes):BLOCK_SIZE;
    ok=start(&bus) && send(&bus,0xa0) && send(&bus,0) && start(&bus) && send(&bus,0xa1);
    for (i=0;ok && i<length;++i) {
        /* Final byte of the base block is NACKed for a single-block EDID. */
        if (!segment && i==127 && !bytes[126]) length=BLOCK_SIZE;
        ok=read_byte(&bus,bytes+i,(BOOLEAN)(i+1==length));
    }
    stop(&bus);
    if (!ok) return FALSE;
    for (i=0;i<length;++i) {
        if (!segment && i<8 && bytes[i]!=header[i]) return FALSE;
        sum=(UCHAR)(sum+bytes[i]);
        if ((i+1)%BLOCK_SIZE==0 && sum) return FALSE;
    }
    for (i=0;i<length;++i) out[i]=bytes[i];
    for (;i<size;++i) out[i]=0;
    return TRUE;
}

LONG NTAPI WxpEntry(PVOID driver,PVOID path)
{
    (void)driver; (void)path;
    return 0;
}
