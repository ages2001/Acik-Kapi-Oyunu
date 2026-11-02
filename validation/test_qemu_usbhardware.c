/* Driver pipeline against the production QEMU register core. Synthetic MCU
   completion checks transport bytes, not instruction execution or analog PHY. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../RLTKLANT/USBNT/src/usbfwio.h"
#include "../RLTKLANT/USBNT/src/usbnic.h"
#include "../RLTKLANT/qemu/hw-usb/rtl815x-core.h"
static R815xCore core;
static unsigned calls,cancel_at,corrupt;
static NDIS_STATUS control(PUSBNT_PIPE pipe,UCHAR rt,UCHAR request,USHORT value,
                           USHORT index,PVOID data,USHORT length)
{
    int result;unsigned char changed[4];(void)pipe;calls++;
    assert(!cancel_at||calls<cancel_at);assert(request==5&&length==4);
    if(rt==0xc0)result=r815c_control_read(&core,value,index,data,length);
    else{
        assert(rt==0x40);
        if(corrupt&&!(index&0x100)&&value==0x9a00){memcpy(changed,data,4);changed[0]^=1;data=changed;}
        result=r815c_control_write(&core,value,index,data,length);
    }
    return result?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;
}
static int wait_ms(void*p,unsigned ms){(void)p;assert(ms<=20);r815c_tick(&core,ms);return 1;}
static int cancel(void*p){(void)p;return cancel_at&&calls+1>=cancel_at;}
static NDIS_STATUS submit(PVOID object,UCHAR kind,UCHAR ep,PUCHAR setup_packet,
                          PVOID buffer,ULONG length,PULONG done)
{
    NDIS_STATUS status;(void)object;(void)ep;assert(!kind&&length==4);
    status=control(NULL,setup_packet[0],setup_packet[1],r815c_word(setup_packet+2),
                   r815c_word(setup_packet+4),buffer,(USHORT)length);
    *done=USB_OK(status)?length:0;return status;
}
static void setup(RTL_USB_NIC*nic,unsigned signature,unsigned package,unsigned faults)
{
    static const unsigned char mac[]={0x52,0x54,0,0x12,0x34,0x56};
    memset(&core,0,sizeof(core));core.version=(uint16_t)signature;core.link_up=1;core.phy_faults=faults;
    r815c_set_mac(&core,mac);r815c_reset(&core);if(signature==0x2020)core.reg[0xdc48]=(uint8_t)package;
    memset(nic,0,sizeof(*nic));nic->Pipe.Control=control;calls=cancel_at=corrupt=0;
    assert(USB_OK(RtlUsbDetectVersion(nic)));
}
int main(void)
{
    static const unsigned signatures[]={0x4c00,0x4c10,0x5c00,0x5c10,0x5c20,0x5c30,
        0x4800,0x6000,0x6010,0x7020,0x7030,0x7400,0x7410,0x6400,0x7420,0x1030,0x2020};
    RTL_USB_NIC nic;RTL_USB_HW_STATE state;RTL_UPHY_REPORT report;unsigned i,package;
    for(i=0;i<17;i++)for(package=i==16?0x18:0;package<=(i==16?0x1e:0);package+=2){
        enum RTL_UFW_EXEC_RESULT result;unsigned prep,enable;
        setup(&nic,signatures[i],package,0);
        result=RtlUsbHardwareApply(&nic,&state,3,0x0bda,0x8153,2048,wait_ms,cancel,NULL,&report);
        if(result){fprintf(stderr,"FAIL QEMU hardware v%u pkg%02x result%u reg%x calls%u fw%u hash%llx bad%u\n",i+1,package,result,report.LastRegister,calls,core.fw_bytes,(unsigned long long)core.fw_hash,core.fw_bad);return 1;}
        prep=calls;assert(!nic.Running&&!core.rx_enabled&&!core.tx_enabled);
        r815c_tick(&core,300);assert(r815c_carrier(&core));
        assert(USB_OK(RtlUsbSetRxMode(&nic,RTLU_RCR_APM|RTLU_RCR_AB)));
        assert(RtlUsbHardwareEnableApply(&nic,&state,wait_ms,cancel,NULL,&report)==RtlUsbFwExecOk);
        enable=calls-prep;assert(nic.Running&&core.rx_enabled&&core.tx_enabled);
        if(i==12||i==14)assert(core.sram[0x801e]==0x21);
        if(i==15)assert(core.sram[0x801e]==0x14);
        if(i==16)assert(core.sram[0x801e]==0x22);
        printf("PASS QEMU register/PHY transport v%u package%02x: prepare%u enable%u transfers\n",i+1,package,prep,enable);fflush(stdout);
    }
    for(i=1;i<=128;i<<=1){
        setup(&nic,0x2020,0x1e,i);
        assert(RtlUsbHardwareApply(&nic,&state,3,0x0bda,0x8159,2048,wait_ms,cancel,NULL,&report)!=RtlUsbFwExecOk);
        assert(!nic.Running&&!core.rx_enabled&&!core.tx_enabled);
    }
    setup(&nic,0x2020,0x1e,0);corrupt=1;
    assert(RtlUsbHardwareApply(&nic,&state,3,0x0bda,0x8159,2048,wait_ms,cancel,NULL,&report)!=RtlUsbFwExecOk);
    assert(core.sram[0x801e]!=0x22&&!nic.Running);
    /* Actual USB front end: initialization stays stopped until the passive
       owner sees carrier; cable cycling must re-enable without PHY replay. */
    for(i=0;i<17;i++){
        USBNIC adapter;USBNIC_PLATFORM platform={3,wait_ms,cancel,NULL};USBNT_OSVER os={5,0};
        BOOLEAN up,duplex;ULONG speed;unsigned cycle,uploaded;
        setup(&nic,signatures[i],i==16?0x1e:0,0);
        assert(USB_OK(UsbNicAttachEx(&adapter,os,UsbHcEhci,0x0bda,0x8153,NULL,submit,&platform)));
        assert(adapter.HardwareReady&&!adapter.Nic.Running);r815c_tick(&core,300);uploaded=core.fw_bytes;
        for(cycle=0;cycle<5;cycle++){
            assert(USB_OK(UsbNicReadMedia(&adapter,&up,&speed,&duplex))&&up&&speed==adapter.Nic.MaxSpeedMbit);
            assert(USB_OK(UsbNicApplyCarrier(&adapter,up,speed,duplex))&&adapter.Nic.Running&&core.tx_enabled&&core.rx_enabled);
            r815c_set_link(&core,0);
            assert(USB_OK(UsbNicReadMedia(&adapter,&up,&speed,&duplex))&&!up);
            assert(USB_OK(UsbNicApplyCarrier(&adapter,up,speed,duplex))&&!adapter.Nic.Running&&!core.tx_enabled&&!core.rx_enabled);
            assert(core.fw_bytes==uploaded);r815c_set_link(&core,1);
        }
        for(cycle=0;cycle<3;cycle++){
            BOOLEAN lost=FALSE;
            assert(USB_OK(UsbNicReadMedia(&adapter,&up,&speed,&duplex))&&up);
            assert(USB_OK(UsbNicApplyCarrier(&adapter,up,speed,duplex)));
            assert(USB_OK(UsbNicCheckReset(&adapter,&lost))&&!lost);
            r815c_reset(&core); /* actual QEMU bus-reset register loss */
            assert(USB_OK(UsbNicCheckReset(&adapter,&lost))&&lost);
            assert(USB_OK(UsbNicRecoverReset(&adapter))&&adapter.HardwareReady);
            assert(!adapter.Nic.Running&&!core.rx_enabled&&!core.tx_enabled);
            r815c_tick(&core,300);
            assert(USB_OK(UsbNicReadMedia(&adapter,&up,&speed,&duplex))&&up);
            assert(USB_OK(UsbNicApplyCarrier(&adapter,up,speed,duplex)));
            assert(core.rx_enabled&&core.tx_enabled&&adapter.Nic.Running);
        }
        /* A new identity must not inherit an existing NDIS binding. */
        core.mac[5]^=1;r815c_reset(&core);
        assert(!USB_OK(UsbNicRecoverReset(&adapter))&&!adapter.HardwareReady&&!adapter.Nic.Running);
        assert(adapter.Nic.Mac[5]!=core.mac[5]&&!core.rx_enabled&&!core.tx_enabled);
        UsbNicDetach(&adapter);assert(!adapter.HardwareReady&&adapter.Route==UsbRouteNone);
    }
    puts("PASS: actual front-end attach and 85 carrier down/up cycles across all17 revisions");
    puts("PASS: 51 bus-reset MAC/PHY rebuilds and 17 changed-MAC rejections across all17 revisions");
    puts("PASS: production QEMU PHY fault injection and corrupted firmware transport rejection");
    return 0;
}
