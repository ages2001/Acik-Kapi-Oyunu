/* Local VRAM tests for the modeled GeForce and Radeon vendor endpoints.
 * The caller supplies checked mappings, native 32-bit register access and a delay.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
static int board_sdma(board_regs rb, board_regs page, board_word *saved)
{
    enum { base = 0x400000, start = 240, src = 512, dst = 768, wb = 960, fence = 992 };
    const board_word packets[] = {0x0001000b, base + src, 0, 0x6b5ac331, 31,
        0x00080001, 18, 0, base + src + 1, 0, base + dst + 3, 0, 0,
        0x00030005, base + fence, 0, 0x76543210, 0x00030005, base + fence + 4, 0, 0xfedcba98};
    const unsigned char pattern[] = {0x31, 0xc3, 0x5a, 0x6b};
    board_word saved_regs[9], i, pos, actual;
    int ok = 0;
    for (i = 0; i < 9; ++i) saved_regs[i] = board_read(rb + i);
    if (saved_regs[0] & 1) return 0;
    for (i = 0; i < 256; ++i) saved[i] = board_read(page + i);
    for (i = 0; i < 8; ++i) {
        board_write(page + src / 4 + i, 0);
        board_write(page + dst / 4 + i, 0x55555555);
    }
    board_write(page + wb / 4, 0xffffffffUL);
    board_write(page + wb / 4 + 1, 0xffffffffUL);
    board_write(page + fence / 4, 0);
    board_write(page + fence / 4 + 1, 0);
    for (i = 0; i < (sizeof(packets) / sizeof(*packets)); ++i)
        board_write(page + ((start / 4 + i) & 63), packets[i]);
    board_write(rb, 0);
    board_write(rb + 1, base >> 8);
    board_write(rb + 2, 0);
    board_write(rb + 3, start);
    board_write(rb + 4, 0);
    board_write(rb + 5, start);
    board_write(rb + 6, 0);
    board_write(rb + 7, base + wb);
    board_write(rb + 8, 0);
    board_write(rb, 1 | (6 << 1) | 0x1000);
    board_write(rb + 5, start + 16); /* Part of the fill packet. */
    if (board_read(rb + 3) != start || board_read(page + src/4))
        goto restore;
    pos = start + sizeof(packets);
    board_write(rb + 5, pos);
    for (i = 0; i < 1000 && board_read(rb + 3) != pos; ++i)
        board_delay();
    if (board_read(rb + 3) != pos || board_read(page + wb/4) != pos ||
        board_read(page + wb/4 + 1) != 0 || board_read(page + fence/4) != 0x76543210 ||
        board_read(page + fence/4 + 1) != 0xfedcba98UL) goto restore;
    for (i = 0; i < 32; ++i) {
        actual = board_read(page + dst/4 + i/4);
        if ((unsigned char)(actual >> ((i & 3) * 8)) != (i >= 3 && i < 22 ? pattern[(i - 2) & 3] : 0x55))
            goto restore;
    }
    ok = 1;
restore:
    board_write(rb, 0);
    for (i = 1; i < 9; ++i) board_write(rb + i, saved_regs[i]);
    board_write(rb, saved_regs[0]);
    for (i = 0; i < 256; ++i) board_write(page + i, saved[i]);
    return ok;
}

static int board_pramin(board_regs selector, board_regs window, board_regs page, board_regs counter)
{
    board_word old_select = board_read(selector);
    board_word old_word, actual, mirrored, before, after;
    unsigned i;
    board_write(selector, 0x10);
    old_word = board_read(page + 16);
    board_write(window + 16, ~old_word);
    actual = board_read(page + 16);
    board_write(page + 16, old_word);
    mirrored = board_read(window + 16);
    board_write(selector, old_select);
    before = board_read(counter);
    for (i = 0; i < 100; ++i) {
        board_delay();
        after = board_read(counter);
        if (after != before) break;
    }
    return actual == (board_word)~old_word && mirrored == old_word && after != before;
}
