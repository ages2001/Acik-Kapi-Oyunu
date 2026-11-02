/*
 * rltkhw.h - hardware register definitions for the Realtek LAN backends.
 *
 * Three register models are covered:
 *   NE2000  - RTL8029(AS)   (DP8390 core, I/O mapped)
 *   RTL8139 - RTL8129/8139/810x   (CR/RBSTART ring model)
 *   C+      - RTL8169..RTL8127     (descriptor model; 8125+ extends it)
 *
 * Copyright (C) 2026 ages2001.
 */
#ifndef RLTKHW_H
#define RLTKHW_H

/* ====================================================================
 *  RTL8139 family register set (offsets from I/O or MMIO base)
 * ================================================================== */
#define R8139_IDR0          0x00    /* MAC address 0..5                 */
#define R8139_MAR0          0x08    /* multicast 0..7                   */
#define R8139_TSD0          0x10    /* transmit status desc 0..3        */
#define R8139_TSAD0         0x20    /* transmit start addr 0..3         */
#define R8139_RBSTART       0x30    /* receive buffer start (phys)      */
#define R8139_ERBCR         0x34
#define R8139_ERSR          0x36
#define R8139_CR            0x37    /* command register                 */
#define R8139_CAPR          0x38    /* current addr of packet read      */
#define R8139_CBR           0x3A    /* current buffer addr              */
#define R8139_IMR           0x3C    /* interrupt mask                   */
#define R8139_ISR           0x3E    /* interrupt status                 */
#define R8139_TCR           0x40    /* transmit config                  */
#define R8139_RCR           0x44    /* receive config                   */
#define R8139_TCTR          0x48
#define R8139_MPC           0x4C
#define R8139_9346CR        0x50    /* 93C46 EEPROM command             */
#define R8139_CONFIG0       0x51
#define R8139_CONFIG1       0x52
#define R8139_MSR           0x58    /* media status                     */
#define R8139_CONFIG3       0x59
#define R8139_CONFIG4       0x5A
#define R8139_MULINT        0x5C
#define R8139_BMCR          0x62    /* PHY basic mode control           */
#define R8139_BMSR          0x64    /* PHY basic mode status            */
#define R8139_ANAR          0x66
#define R8139_ANLPAR        0x68
#define R8139_ANER          0x6A

/* CR bits */
#define R8139_CR_BUFE       0x01    /* rx buffer empty                  */
#define R8139_CR_TE         0x04    /* transmitter enable               */
#define R8139_CR_RE         0x08    /* receiver enable                  */
#define R8139_CR_RST        0x10    /* reset                            */

/* ISR/IMR bits */
#define R8139_INT_ROK       0x0001
#define R8139_INT_RER       0x0002
#define R8139_INT_TOK       0x0004
#define R8139_INT_TER       0x0008
#define R8139_INT_RXOVW     0x0010
#define R8139_INT_PUN       0x0020  /* link change                      */
#define R8139_INT_FOVW      0x0040
#define R8139_INT_LENCHG    0x2000
#define R8139_INT_TIMEOUT   0x4000
#define R8139_INT_SERR      0x8000
#define R8139_INT_DEFAULT   (R8139_INT_ROK|R8139_INT_RER|R8139_INT_TOK|R8139_INT_TER|\
                             R8139_INT_RXOVW|R8139_INT_PUN|R8139_INT_FOVW|R8139_INT_SERR)

/* TSD bits */
#define R8139_TSD_OWN       0x2000
#define R8139_TSD_TUN       0x4000
#define R8139_TSD_TOK       0x8000
#define R8139_TSD_SIZE_MASK 0x1FFF

/* RCR bits */
#define R8139_RCR_AAP       0x00000001  /* accept all phys (promisc)    */
#define R8139_RCR_APM       0x00000002  /* accept phys match            */
#define R8139_RCR_AM        0x00000004  /* accept multicast             */
#define R8139_RCR_AB        0x00000008  /* accept broadcast             */
#define R8139_RCR_AR        0x00000010  /* accept runt                  */
#define R8139_RCR_AER       0x00000020  /* accept error                 */
#define R8139_RCR_WRAP      0x00000080
#define R8139_RCR_RBLEN_32K 0x00001800  /* 32K+16 ring (bits 11:12=11)  */
#define R8139_RCR_MXDMA_UNL 0x00000700
#define R8139_RCR_RXFTH_NO  0x0000E000

/* TCR bits */
#define R8139_TCR_MXDMA_2048 0x00000700
#define R8139_TCR_IFG_STD    0x03000000
#define R8139_TCR_HWVERID_A  0x7C800000  /* used to read chip version    */
#define R8139_TCR_HWVERID_B  0x00400000
#define R8139_TCR_HWVERMASK  0x7CC00000

/* receive packet header status bits (first USHORT of each rx frame) */
#define R8139_RX_ROK        0x0001
#define R8139_RX_FAE        0x0002
#define R8139_RX_CRC        0x0004
#define R8139_RX_LONG       0x0008
#define R8139_RX_RUNT       0x0010
#define R8139_RX_ISE        0x0020
#define R8139_RX_BAR        0x2000  /* broadcast                        */
#define R8139_RX_PAM        0x4000  /* physical match                   */
#define R8139_RX_MAR        0x8000  /* multicast                        */

/* MSR bits */
#define R8139_MSR_SPEED_10  0x08    /* 1 = 10Mb, 0 = 100Mb              */
#define R8139_MSR_LINKB     0x04    /* 1 = link down                    */

/* 93C46 command register */
#define R8139_9346_EEM_NORMAL  0x00
#define R8139_9346_EEM_CONFIG  0xC0

/* RX ring geometry */
#define R8139_RXBUF_IDX     3       /* 32K ring (0..3 -> 8/16/32/64 K)  */
#define R8139_RXBUF_LEN     (32*1024)
#define R8139_RXBUF_PAD     16
#define R8139_RXBUF_WRAPPAD 2048
#define R8139_TX_FIFO_THRESH 256
#define R8139_TX_DESCS      4

/* ====================================================================
 *  C+ / RTL8169..RTL8127 descriptor model
 * ================================================================== */
#define RCP_IDR0            0x00
#define RCP_MAR0            0x08
#define RCP_DTCCR           0x10    /* dump tally counter               */
#define RCP_TNPDS           0x20    /* tx normal priority desc start    */
#define RCP_THPDS           0x28    /* tx high priority desc start      */
#define RCP_FLASH           0x30
#define RCP_ERBCR           0x34
#define RCP_ERSR            0x36
#define RCP_CR              0x37    /* command                          */
#define RCP_TPPOLL          0x38    /* transmit priority polling        */
#define RCP_IMR             0x3C
#define RCP_ISR             0x3E
#define RCP_TCR             0x40
#define RCP_RCR             0x44
#define RCP_TCTR            0x48
#define RCP_MPC             0x4C
#define RCP_9346CR          0x50
#define RCP_CONFIG0         0x51
#define RCP_CONFIG1         0x52
#define RCP_CONFIG2         0x53
#define RCP_CONFIG3         0x54
#define RCP_CONFIG4         0x55
#define RCP_CONFIG5         0x56
#define RCP_PHYAR           0x60    /* PHY access register (MDIO)       */
#define RCP_TBICSR          0x64
#define RCP_PHYSTATUS       0x6C    /* PHY(MAC) status                  */
#define RCP_RMS             0xDA    /* rx max packet size               */
#define RCP_CPCR            0xE0    /* C+ command register              */
#define RCP_RDSAR           0xE4    /* rx desc start addr               */
#define RCP_ETTHR           0xEC    /* early tx threshold               */
#define RCP_MISC            0xF0

/* 8125+ moved some registers; extended IMR/ISR are 32-bit at 0x38/0x3C */
#define RCP_IMR0_8125       0x38
#define RCP_ISR0_8125       0x3C
#define RCP_TPPOLL_8125     0x90
#define RCP_RMS_8125        0xDA

/* CR bits */
#define RCP_CR_TE           0x04
#define RCP_CR_RE           0x08
#define RCP_CR_RST          0x10

/* C+ command register (CPCR) */
#define RCP_CPCR_RXCHKSUM   0x0020
#define RCP_CPCR_RXVLAN     0x0040
#define RCP_CPCR_PKTCNTRL   0x0080
#define RCP_CPCR_RXENB      0x0002
#define RCP_CPCR_TXENB      0x0001

/* TPPOLL */
#define RCP_TPPOLL_NPQ      0x40
#define RCP_TPPOLL_HPQ      0x80

/* ISR/IMR bits (legacy 16-bit path, 8169/8168) */
#define RCP_INT_ROK         0x0001
#define RCP_INT_RER         0x0002
#define RCP_INT_TOK         0x0004
#define RCP_INT_TER         0x0008
#define RCP_INT_RDU         0x0010  /* rx descriptor unavailable        */
#define RCP_INT_LINKCHG     0x0020
#define RCP_INT_RXFOVW      0x0040
#define RCP_INT_TDU         0x0080
#define RCP_INT_SWINT       0x0100
#define RCP_INT_TIMEOUT     0x4000
#define RCP_INT_SERR        0x8000
#define RCP_INT_DEFAULT     (RCP_INT_ROK|RCP_INT_RER|RCP_INT_TOK|RCP_INT_TER|\
                             RCP_INT_RDU|RCP_INT_LINKCHG|RCP_INT_RXFOVW|RCP_INT_SERR)

/* RCR bits (same low nibble meaning as 8139) */
#define RCP_RCR_AAP         0x00000001
#define RCP_RCR_APM         0x00000002
#define RCP_RCR_AM          0x00000004
#define RCP_RCR_AB          0x00000008
#define RCP_RCR_AR          0x00000010
#define RCP_RCR_AER         0x00000020
#define RCP_RCR_MXDMA_UNL   0x00000700
#define RCP_RCR_RXFTH_UNL   0x0000E000

/* PHYAR */
#define RCP_PHYAR_FLAG      0x80000000  /* 1=write busy / read ready    */

/* PHYSTATUS bits */
#define RCP_PHYS_LINKSTS    0x02
#define RCP_PHYS_FULLDUP    0x01
#define RCP_PHYS_10M        0x04
#define RCP_PHYS_100M       0x08
#define RCP_PHYS_1000M      0x10
#define RCP_PHYS_2500M      0x400   /* 8125+                            */
#define RCP_PHYS_5000M      0x1000   /* 8126                             */
#define RCP_PHYS_10000M     0x4000  /* 8127 (vendor-defined)            */

/* C+ descriptor flags (OWN by NIC = bit31 of opts1) */
#define RCP_DESC_OWN        0x80000000UL
#define RCP_DESC_EOR        0x40000000UL  /* end of ring                */
#define RCP_DESC_FS         0x20000000UL  /* first segment              */
#define RCP_DESC_LS         0x10000000UL  /* last segment               */
#define RCP_DESC_FRAGLEN    0x0000FFFFUL
/* rx opts1 extra */
#define RCP_RDESC_RES       0x00200000UL  /* receive error summary      */

#define RCP_NUM_TX_DESC     64
#define RCP_NUM_RX_DESC     64
#define RCP_RX_BUF_SIZE     2048      /* per rx descriptor (std MTU)     */
#define RCP_RX_BUF_SIZE_JUMBO 9216

/* One hardware descriptor (16 bytes), shared by tx and rx. */
typedef struct _RCP_DESC {
    volatile ULONG opts1; /* DMA ownership/status must be read each time */
    ULONG   opts2;      /* vlan / checksum                              */
    ULONG   addr_lo;    /* buffer physical address low                  */
    ULONG   addr_hi;    /* buffer physical address high (64-bit DMA)    */
} RCP_DESC, *PRCP_DESC;

/* x86/x64 coherent DMA with ordinary WB buffers and uncached MMIO.
 * The CPU preserves the required load/load and store/store order; the
 * compiler must also preserve it across descriptor ownership hand-offs.
 * This is not a portable barrier for non-coherent or write-combined DMA.
 */
static __inline void RtlDmaBarrier(void)
{
    __asm__ __volatile__("" ::: "memory");
}

/* ====================================================================
 *  NE2000 (RTL8029) register set - DP8390 core, page-switched
 * ================================================================== */
#define NE_CR               0x00
#define NE_P0_CLDA0         0x01
#define NE_P0_PSTART        0x01
#define NE_P0_PSTOP         0x02
#define NE_P0_BNRY          0x03
#define NE_P0_TPSR          0x04
#define NE_P0_TBCR0         0x05
#define NE_P0_TBCR1         0x06
#define NE_P0_ISR           0x07
#define NE_P0_RSAR0         0x08
#define NE_P0_RSAR1         0x09
#define NE_P0_RBCR0         0x0A
#define NE_P0_RBCR1         0x0B
#define NE_P0_RCR           0x0C
#define NE_P0_TCR           0x0D
#define NE_P0_DCR           0x0E
#define NE_P0_IMR           0x0F
#define NE_P1_PAR0          0x01    /* page1: phys addr                 */
#define NE_P1_CURR          0x07
#define NE_P1_MAR0          0x08
#define NE_DATA             0x10    /* RTL8029 data port (remote DMA)   */
#define NE_RESET            0x1F

/* CR bits */
#define NE_CR_STP           0x01
#define NE_CR_STA           0x02
#define NE_CR_TXP           0x04
#define NE_CR_RD0           0x08    /* remote read                      */
#define NE_CR_RD1           0x10    /* remote write                     */
#define NE_CR_RD2           0x20    /* abort/complete remote DMA        */
#define NE_CR_PAGE0         0x00
#define NE_CR_PAGE1         0x40
#define NE_CR_PAGE2         0x80

/* ISR/IMR bits */
#define NE_ISR_PRX          0x01
#define NE_ISR_PTX          0x02
#define NE_ISR_RXE          0x04
#define NE_ISR_TXE          0x08
#define NE_ISR_OVW          0x10
#define NE_ISR_CNT          0x20
#define NE_ISR_RDC          0x40
#define NE_ISR_RST          0x80

#define NE_RCR_AB           0x04    /* accept broadcast                 */
#define NE_RCR_AM           0x08    /* accept multicast                 */
#define NE_RCR_PRO          0x10    /* promiscuous                      */
#define NE_DCR_DEFAULT      0x49    /* word-wide, FIFO 8                */
#define NE_TCR_LOOPBACK     0x02

#define NE_PAGE_SIZE        256
#define NE_TX_START         0x40    /* tx buffer page start (6 pages)   */
#define NE_RX_START         0x46
#define NE_RX_STOP          0x80    /* 0x46..0x80 rx ring               */

#endif /* RLTKHW_H */
