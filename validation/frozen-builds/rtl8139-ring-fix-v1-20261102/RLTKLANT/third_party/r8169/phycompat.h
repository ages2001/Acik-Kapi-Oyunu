/* SPDX-License-Identifier: GPL-2.0-only
 * Local compatibility declarations, not Linux kernel ABI structures.
 * The caller serializes this entire passive-level transaction with adapter
 * initialization, stop, reset and every other MDIO/MCU accessor.
 */
#ifndef RTL_PHYCOMPAT_H
#define RTL_PHYCOMPAT_H
#include "../../src/physeq.h"
#include "versions.h"
typedef RTL_FW_U16 u16;
typedef RTL_FW_U8 u8;
typedef RTL_FW_U32 u32;
#ifndef NULL
#define NULL ((void*)0)
#endif
#define BIT(n) (1U<<(n))
#define ARRAY_SIZE(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define MDIO_MMD_VEND2 31
#define MDIO_MMD_AN 7
#define MDIO_AN_EEE_ADV 0x3c
struct phy_device {
    RTL_FW_BUS *Bus;
    const RTL_FW_CHIP *Chip;
    RTL_PHY_REPORT *Report;
};
struct rtl8169_private {struct phy_device *Phy;};
int phy_read(struct phy_device *,int);
int phy_write(struct phy_device *,int,u16);
int phy_modify(struct phy_device *,int,u16,u16);
int phy_read_paged(struct phy_device *,int,int);
int phy_write_paged(struct phy_device *,int,int,u16);
int phy_modify_paged(struct phy_device *,int,int,u16,u16);
int phy_select_page(struct phy_device *,int);
int phy_restore_page(struct phy_device *,int,int);
int phy_write_mmd(struct phy_device *,int,u16,u16);
int __phy_modify_mmd(struct phy_device *,int,u16,u16,u16);
void RtlPhyDelay(struct phy_device *,unsigned);
void RtlPhyNotReady(struct phy_device *);
void r8169_apply_firmware(struct rtl8169_private *);
u8 rtl8168d_efuse_read(struct rtl8169_private *,int);
u16 rtl8168h_2_get_adc_bias_ioffset(struct rtl8169_private *);
void r8169_hw_phy_config(struct rtl8169_private *,struct phy_device *,enum mac_version);
#define __phy_read phy_read
#define __phy_write phy_write
#define __phy_modify phy_modify
#define __phy_write_mmd phy_write_mmd
#define phy_set_bits(p,r,v) phy_modify(p,r,0,v)
#define phy_clear_bits(p,r,v) phy_modify(p,r,v,0)
/* Exclusive ownership is enforced by the enclosing transaction, not a
 * spin lock across firmware waits. These do not introduce nested locks. */
#define phy_lock_mdio_bus(p) ((void)(p))
#define phy_unlock_mdio_bus(p) ((void)(p))
#define msleep(ms) RtlPhyDelay(phydev,ms)
#define phydev_warn(p,message) RtlPhyNotReady(p)
#endif
