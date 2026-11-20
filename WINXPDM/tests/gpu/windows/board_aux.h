/* Read a monitor through the GeForce or DCN 4.1 physical AUX register aperture.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
static int board_nv_transfer(board_regs base, unsigned cmd, board_word address,
                              unsigned char *bytes, unsigned length)
{
    board_word ctl, status, word;
    unsigned i, j, wait;
    int ok = 0;
    if (length > 16) return 0;
    for (wait = 0; wait < 100; ++wait) {
        ctl = board_read(base + 9);
        if (!(ctl & 0x07010000UL)) break;
        board_delay();
    }
    if (wait == 100) return 0;
    board_write(base + 9, (ctl & ~0x00700000UL) | 0x00100000UL);
    for (wait = 0; wait < 100; ++wait) {
        ctl = board_read(base + 9);
        if ((ctl & 0x07000000UL) == 0x01000000UL) break;
        board_delay();
    }
    if (wait == 100 || !(board_read(base + 10) & 0x10000000UL)) goto done;
    if (!(cmd & 1)) {
        for (i = 0; i < length; i += 4) {
            word = 0;
            for (j = 0; j < 4 && i + j < length; ++j) word |= (board_word)bytes[i+j] << (8*j);
            board_write(base + i/4, word);
        }
    }
    ctl = (ctl & ~0x0001f1ffUL) | ((board_word)cmd << 12) | (length ? length - 1 : 0x100);
    board_write(base + 8, address);
    board_write(base + 9, ctl | 0x80000000UL);
    board_write(base + 9, ctl);
    board_write(base + 9, ctl | 0x10000UL);
    for (wait = 0; wait < 200; ++wait) {
        if (!(board_read(base + 9) & 0x10000UL)) break;
        board_delay();
    }
    status = board_read(base + 10);
    if (wait == 200 || (status & 0x000f0f00UL)) goto done;
    if (cmd & 1) {
        if ((status & 31) != length) goto done;
        for (i = 0; i < length; i += 4) {
            word = board_read(base + 4 + i/4);
            for (j = 0; j < 4 && i + j < length; ++j) bytes[i+j] = (unsigned char)(word >> (8*j));
        }
    }
    ok = 1;
done:
    board_write(base + 9, board_read(base + 9) & ~0x00710000UL);
    return ok;
}

static int board_dcn_transfer(board_regs base, unsigned cmd, board_word address,
                              unsigned char *bytes, unsigned length)
{
    unsigned i, count, wait;
    board_word enable, arb, status, reply;
    int ok = 0;
    if (length > 16) return 0;
    enable = board_read(base);
    arb = board_read(base + 2);
    if (arb & 12) return 0;
    if (!(enable & 1)) {
        board_write(base, enable | 0x11);
        for (wait = 0; wait < 11 && !(board_read(base) & 0x20); ++wait) board_delay();
        if (wait == 11) goto restore;
        board_write(base, (enable & ~0x10UL) | 1);
        for (wait = 0; wait < 11 && (board_read(base) & 0x20); ++wait) board_delay();
        if (wait == 11) goto restore;
    }
    board_write(base + 2, (arb & ~0x20000UL) | 0x10000UL);
    if ((board_read(base + 2) & 12) != 4) goto release;
    board_write(base + 3, board_read(base + 3) | 2);
    count = (length ? 4 : 3) + ((cmd & 1) ? 0 : length);
    board_write(base + 1, (board_word)count << 16);
    board_write(base + 6, 0x80000000UL | (((board_word)cmd << 4 | (address >> 16 & 15)) << 8));
    board_write(base + 6, address & 0xff00);
    board_write(base + 6, (address & 255) << 8);
    if (length) board_write(base + 6, (board_word)(length - 1) << 8);
    if (!(cmd & 1)) for (i = 0; i < length; ++i) board_write(base + 6, (board_word)bytes[i] << 8);
    board_write(base + 1, ((board_word)count << 16) | 1);
    for (wait = 0; wait < 200; ++wait) {
        if (board_read(base + 4) & 1) break;
        board_delay();
    }
    status = board_read(base + 4);
    if (wait == 200 || (status & 0x00ffdf80UL) ||
        ((status >> 24) & 31) != 1 + ((cmd & 1) ? length : 0)) goto release;
    board_write(base + 6, 0x80000001UL);
    reply = board_read(base + 6);
    if ((reply >> 12) & 15) goto release;
    if (cmd & 1) for (i = 0; i < length; ++i) bytes[i] = (unsigned char)(board_read(base + 6) >> 8);
    ok = 1;
release:
    board_write(base + 2, (arb & ~0x10000UL) | 0x20000UL);
restore:
    if (!(enable & 1)) board_write(base, enable);
    return ok;
}

static int board_aux_transfer(board_regs base, int nv, unsigned cmd, board_word address,
                              unsigned char *bytes, unsigned length)
{
    if (nv) return board_nv_transfer(base, cmd, address, bytes, length);
    return board_dcn_transfer(base, cmd, address, bytes, length);
}

static int board_aux_edid(board_regs base, int nv, unsigned char *edid, unsigned block)
{
    unsigned i, checksum = 0;
    unsigned char offset = (unsigned char)(block * 128);
    if (block > 1 || !board_aux_transfer(base, nv, 4, 0x50, &offset, 1)) return 0;
    for (i = 0; i < 128; i += 16)
        if (!board_aux_transfer(base, nv, i == 112 ? 1 : 5, 0x50, edid+i, 16)) return 0;
    for (i = 0; i < 128; ++i) checksum += edid[i];
    if (checksum & 255) return 0;
    if (block) return edid[0] == 2;
    if (edid[0] || edid[7]) return 0;
    for (i = 1; i <= 6; ++i) if (edid[i] != 255) return 0;
    return 1;
}

static int board_two_sinks(board_regs base, int nv)
{
    unsigned n;
    unsigned char caps[4], block[128];
    for (n = 0; n < 2; ++n) {
        board_regs aux = base + n * (nv ? 0x50/4 : 0x70/4);
        unsigned x, y;
        if (!board_aux_transfer(aux, nv, 9, 0, caps, sizeof(caps)) ||
            caps[0] != (n ? 0x12 : 0x14) || caps[1] != (n ? 0x14 : 0x1e) ||
            (caps[2] & 31) != 4 || !board_aux_edid(aux, nv, block, 0)) return 0;
        x = block[56] | ((unsigned)(block[58] & 0xf0) << 4);
        y = block[59] | ((unsigned)(block[61] & 0xf0) << 4);
        if (x != (n ? 1280 : 1920) || y != (n ? 1024 : 1080) || block[12] != n + 1 ||
            !block[126] || !board_aux_edid(aux, nv, block, 1)) return 0;
    }
    return !board_aux_transfer(base + 2 * (nv ? 0x50/4 : 0x70/4), nv, 9, 0, caps, sizeof(caps));
}
