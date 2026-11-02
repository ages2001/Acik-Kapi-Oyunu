/*
 * Realtek card reader SD engine, shared by the usb-rtsx (RTS5129 family)
 * and rtsx-pci (RTS52xx / RTL84xx) test models.
 *
 * Internal registers live in a 64 KB byte array (the PCI chips use 14 bit
 * addresses: callers map them to 0xC000 | (addr & 0x3FFF)).  Writing
 * SD_TRANSFER with the start bit runs the SD bus transaction: command and
 * response (CMD_RSP), command + data into the ping-pong buffer (NORMAL_READ),
 * data from the ping-pong buffer (AUTO_WRITE_3 with the ping-pong source), or
 * ring buffer data that the front end moves by DMA / bulk pipe (AUTO_READ_x,
 * AUTO_WRITE_x with the ring source): those wait for rtsx_dma_* calls.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 */

#ifndef RTSX_ENGINE_H
#define RTSX_ENGINE_H

#include "hw/sd/sd.h"

#define RX_SD_CFG1          0xFDA0
#define RX_SD_CFG2          0xFDA1
#define RX_SD_STAT1         0xFDA3
#define RX_SD_BUS_STAT      0xFDA5
#define RX_SD_CMD0          0xFDA9
#define RX_SD_BYTE_CNT_L    0xFDAF
#define RX_SD_BYTE_CNT_H    0xFDB0
#define RX_SD_BLOCK_CNT_L   0xFDB1
#define RX_SD_BLOCK_CNT_H   0xFDB2
#define RX_SD_TRANSFER      0xFDB3
#define RX_PPBUF            0xFA00

#define RX_START            0x80
#define RX_END              0x40
#define RX_IDLE             0x20
#define RX_ERR              0x10

#define RX_RING_MAX         (256 * 1024)

typedef struct RtsxEngine {
    uint8_t regs[0x10000];
    SDBus *bus;
    uint16_t reg_data_src;          /* CARD_DATA_SOURCE: bit 0 = ping-pong buffer */
    uint16_t reg_card_stop;         /* CARD_STOP: SD_CLR_ERR 0x40 */
    uint8_t *ring;                  /* ring buffer data (DMA side) */
    uint32_t ring_len;
    int pending;                    /* 0, 1 ring read waits for the DMA, 2 ring write waits for data */
    uint8_t pend_mode;
    uint32_t pend_bytes;
    uint32_t commands, data_bytes, errors;
} RtsxEngine;

static void rtsx_engine_init(RtsxEngine *e, SDBus *bus, uint16_t data_src, uint16_t card_stop)
{
    memset(e->regs, 0, sizeof(e->regs));
    e->bus = bus;
    e->reg_data_src = data_src;
    e->reg_card_stop = card_stop;
    if (!e->ring) {
        e->ring = g_malloc0(RX_RING_MAX);
    }
    e->ring_len = 0;
    e->pending = 0;
    e->regs[RX_SD_TRANSFER] = RX_END | RX_IDLE;
}

static uint32_t rtsx_xfer_bytes(RtsxEngine *e)
{
    uint32_t bc = e->regs[RX_SD_BYTE_CNT_L] | (e->regs[RX_SD_BYTE_CNT_H] << 8);
    uint32_t blk = e->regs[RX_SD_BLOCK_CNT_L] | (e->regs[RX_SD_BLOCK_CNT_H] << 8);

    if (bc == 0) {
        bc = 512;
    }
    if (blk == 0) {
        blk = 1;
    }
    return bc * blk;
}

/* command from SD_CMD0..4, response type from SD_CFG2; FALSE: no response */
static bool rtsx_do_cmd(RtsxEngine *e)
{
    SDRequest req;
    uint8_t rsp[16];
    int rlen, i;
    uint8_t type = e->regs[RX_SD_CFG2] & 0x03;      /* 0 none, 1 6 bytes, 2 17 bytes */

    req.cmd = e->regs[RX_SD_CMD0] & 0x3F;
    req.arg = ((uint32_t)e->regs[RX_SD_CMD0 + 1] << 24) | (e->regs[RX_SD_CMD0 + 2] << 16) |
              (e->regs[RX_SD_CMD0 + 3] << 8) | e->regs[RX_SD_CMD0 + 4];
    req.crc = 0;
    e->commands++;
    if (!sdbus_get_inserted(e->bus)) {
        return false;
    }
    rlen = sdbus_do_command(e->bus, &req, rsp);
    if (type == 0) {
        return true;
    }
    if (rlen == 0) {
        return false;
    }
    if (type == 2 && rlen == 16) {
        e->regs[RX_PPBUF] = 0x3F;
        for (i = 0; i < 15; i++) {
            e->regs[RX_PPBUF + 1 + i] = rsp[i];
        }
        e->regs[RX_PPBUF + 16] = rsp[15];
    } else if (rlen == 4) {
        /* 48 bit response: start, transmission, index (0x3F for R3), 32 bits, CRC */
        e->regs[RX_SD_CMD0] = (req.cmd == 41 || req.cmd == 1) ? 0x3F : req.cmd;
        for (i = 0; i < 4; i++) {
            e->regs[RX_SD_CMD0 + 1 + i] = rsp[i];
        }
    } else {
        return false;
    }
    return true;
}

static void rtsx_end(RtsxEngine *e, uint8_t mode, bool ok)
{
    e->regs[RX_SD_TRANSFER] = (mode & 0x0F) | RX_END | RX_IDLE | (ok ? 0 : RX_ERR);
    e->regs[RX_SD_STAT1] = ok ? 0 : 0x80;
    e->pending = 0;
    if (!ok) {
        e->errors++;
    }
}

static void rtsx_card_read(RtsxEngine *e, uint8_t *buf, uint32_t n)
{
    if (sdbus_get_inserted(e->bus)) {
        sdbus_read_data(e->bus, buf, n);
    } else {
        memset(buf, 0, n);
    }
    e->data_bytes += n;
}

static void rtsx_card_write(RtsxEngine *e, const uint8_t *buf, uint32_t n)
{
    if (sdbus_get_inserted(e->bus)) {
        sdbus_write_data(e->bus, buf, n);
    }
    e->data_bytes += n;
}

static void rtsx_transfer(RtsxEngine *e, uint8_t v)
{
    uint8_t mode = v & 0x0F;
    bool pingpong = e->regs[e->reg_data_src] & 1;
    uint32_t n = rtsx_xfer_bytes(e);
    bool ok = true;

    e->regs[RX_SD_TRANSFER] = v & ~RX_END;
    switch (mode) {
    case 0x08:                                          /* CMD_RSP */
        ok = rtsx_do_cmd(e);
        rtsx_end(e, mode, ok);
        return;
    case 0x0C:                                          /* NORMAL_READ: command, data to the ping-pong buffer */
        ok = rtsx_do_cmd(e);
        if (ok) {
            rtsx_card_read(e, e->regs + RX_PPBUF, MIN(n, 512));
        }
        rtsx_end(e, mode, ok);
        return;
    case 0x0D:                                          /* NORMAL_WRITE: command, data from the ping-pong buffer */
        ok = rtsx_do_cmd(e);
        if (ok) {
            rtsx_card_write(e, e->regs + RX_PPBUF, MIN(n, 512));
        }
        rtsx_end(e, mode, ok);
        return;
    case 0x0E:                                          /* AUTO_READ_2: command + data */
    case 0x05:                                          /* AUTO_READ_3: data only */
    case 0x06:                                          /* AUTO_READ_4 */
        if (mode == 0x0E && !rtsx_do_cmd(e)) {
            rtsx_end(e, mode, false);
            return;
        }
        if (pingpong) {
            rtsx_card_read(e, e->regs + RX_PPBUF, MIN(n, 512));
            rtsx_end(e, mode, true);
            return;
        }
        n = MIN(n, RX_RING_MAX);
        rtsx_card_read(e, e->ring, n);
        e->ring_len = n;
        e->pending = 1;                                 /* done when the DMA took the data */
        e->pend_mode = mode;
        return;
    case 0x01:                                          /* AUTO_WRITE_3: data only */
    case 0x02:                                          /* AUTO_WRITE_4 */
    case 0x0A:                                          /* AUTO_WRITE_2: command + data */
        if (mode == 0x0A && !rtsx_do_cmd(e)) {
            rtsx_end(e, mode, false);
            return;
        }
        if (pingpong) {
            rtsx_card_write(e, e->regs + RX_PPBUF, MIN(n, 512));
            rtsx_end(e, mode, true);
            return;
        }
        e->pending = 2;                                 /* waits for the data */
        e->pend_mode = mode;
        e->pend_bytes = MIN(n, RX_RING_MAX);
        return;
    default:
        rtsx_end(e, mode, false);
        return;
    }
}

/* the front end moved the ring data: a read finishes */
static void rtsx_dma_read_done(RtsxEngine *e)
{
    if (e->pending == 1) {
        rtsx_end(e, e->pend_mode, true);
    }
}

/* the front end has the data of a ring write */
static void rtsx_dma_write(RtsxEngine *e, const uint8_t *buf, uint32_t n)
{
    if (e->pending == 2) {
        rtsx_card_write(e, buf, MIN(n, e->pend_bytes));
        rtsx_end(e, e->pend_mode, true);
    }
}

static uint8_t rtsx_reg_read(RtsxEngine *e, uint16_t a)
{
    return e->regs[a];
}

static void rtsx_reg_write(RtsxEngine *e, uint16_t a, uint8_t mask, uint8_t v)
{
    uint8_t nv = (e->regs[a] & ~mask) | (v & mask);

    if (a == RX_SD_TRANSFER) {
        if (nv & RX_START) {
            rtsx_transfer(e, nv);
        }
        return;
    }
    e->regs[a] = nv;
    if (a == e->reg_card_stop && (nv & 0x40)) {           /* SD_CLR_ERR */
        e->regs[RX_SD_TRANSFER] = RX_END | RX_IDLE;
        e->regs[a] &= ~0x44;
        e->pending = 0;
    }
}

#endif
