/* Register protocol compared with vendor r8152 v2.22.1:
 * r8152_phy_read/write and rtl_tgphy_access/wait_tgphy_cmd_ready.
 * This is not a replacement for revision-specific firmware and PHY setup.
 */
#include "usbphy.h"
#define PLA_GPHY_BASE 0xe86c
#define USB_TGPHY_ADDR 0xd630
#define USB_TGPHY_DATA 0xd632
#define USB_TGPHY_CMD  0xd634
#define TGPHY_BUSY 1
#define TGPHY_WRITE 2

static NDIS_STATUS ReadWord(PRTL_USB_NIC n, USHORT bank, USHORT off, PUSHORT value)
{
    UCHAR data[2];
    NDIS_STATUS st = RtlUsbReadRegs(n, bank, off, data, 2);
    if (USB_OK(st)) *value = (USHORT)(data[0] | ((USHORT)data[1] << 8));
    return st;
}
static NDIS_STATUS WriteWord(PRTL_USB_NIC n, USHORT bank, USHORT off, USHORT value)
{
    UCHAR data[2];
    data[0]=(UCHAR)value; data[1]=(UCHAR)(value>>8);
    return RtlUsbWriteRegs(n, bank, off, RTLU_BYTE_EN_WORD, data, 2);
}
static NDIS_STATUS WaitReady(PRTL_USB_NIC n, PFN_USB_PHY_WAIT wait, PVOID context)
{
    UINT i;
    for (i=0;i<10;i++) {
        USHORT command=0xffff;
        NDIS_STATUS st=ReadWord(n,RTLU_MCU_TYPE_USB,USB_TGPHY_CMD,&command);
        if (!USB_OK(st)) return st;
        if (command==0xffff) return NDIS_STATUS_FAILURE;
        if (!(command&TGPHY_BUSY)) return NDIS_STATUS_SUCCESS;
        if (!wait) return NDIS_STATUS_FAILURE;
        wait(context,1);
    }
    return NDIS_STATUS_FAILURE;
}
static NDIS_STATUS PhyAccess(PRTL_USB_NIC n, USHORT address, PUSHORT value,
                             BOOLEAN write, PFN_USB_PHY_WAIT wait, PVOID context)
{
    NDIS_STATUS st;
    if (!n->Version || n->Version>17) return NDIS_STATUS_ADAPTER_NOT_FOUND;
    if (address&1) return NDIS_STATUS_FAILURE;
    if (n->Version<16) {
        /* Always select the page. A cached page can become stale after reset,
           firmware access, or another register user sharing this device. */
        st=WriteWord(n,RTLU_MCU_TYPE_PLA,PLA_GPHY_BASE,(USHORT)(address&0xf000));
        if (!USB_OK(st)) return st;
        address=(USHORT)(0xb000|(address&0x0fff));
        return write ? WriteWord(n,RTLU_MCU_TYPE_PLA,address,*value) :
                       ReadWord(n,RTLU_MCU_TYPE_PLA,address,value);
    }
    st=WaitReady(n,wait,context);
    if (!USB_OK(st)) return st;
    if (write) {
        st=WriteWord(n,RTLU_MCU_TYPE_USB,USB_TGPHY_DATA,*value);
        if (!USB_OK(st)) return st;
    }
    st=WriteWord(n,RTLU_MCU_TYPE_USB,USB_TGPHY_ADDR,address);
    if (!USB_OK(st)) return st;
    st=WriteWord(n,RTLU_MCU_TYPE_USB,USB_TGPHY_CMD,
                 (USHORT)(TGPHY_BUSY|(write?TGPHY_WRITE:0)));
    if (!USB_OK(st)) return st;
    /* Complete writes before returning too. The vendor defers this wait to
       the next access; this synchronous API reports completion to its caller. */
    st=WaitReady(n,wait,context);
    if (!USB_OK(st)) return st;
    return write ? NDIS_STATUS_SUCCESS :
                   ReadWord(n,RTLU_MCU_TYPE_USB,USB_TGPHY_DATA,value);
}
NDIS_STATUS RtlUsbPhyRead(PRTL_USB_NIC n, USHORT address, PUSHORT value,
                         PFN_USB_PHY_WAIT wait, PVOID context)
{
    USHORT result=0xffff;
    NDIS_STATUS st;
    if (!value) return NDIS_STATUS_FAILURE;
    *value=0xffff;
    st=PhyAccess(n,address,&result,FALSE,wait,context);
    if (USB_OK(st)) *value=result;
    return st;
}
NDIS_STATUS RtlUsbPhyWrite(PRTL_USB_NIC n, USHORT address, USHORT value,
                          PFN_USB_PHY_WAIT wait, PVOID context)
{
    return PhyAccess(n,address,&value,TRUE,wait,context);
}
