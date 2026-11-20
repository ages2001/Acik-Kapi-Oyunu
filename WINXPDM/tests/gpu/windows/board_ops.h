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

/* Exercise an unused instruction page on the dedicated, idle GM204 test board. */
static int board_falcon(board_regs pmu)
{
    /* mov r1,0x100; ld r2,D[r1]; add r2,r2,7; st D[r1+4],r2; exit */
    static const unsigned char code[] = {0x41,0,1, 0xbf,0x12, 0x90,0x22,7,
                                          0xb5,0x12,1, 0xf8,2};
    board_regs entry = pmu + 0x104/4, tlb = pmu + 0x140/4;
    board_regs imem = pmu + 0x180/4, dmem = pmu + 0x1c0/4;
    board_word old_entry, old_index, old_tag, old_data, input, old_words[2], word;
    unsigned i, j, trial;
    int ok = 0;
    if (!(board_read(pmu + 0x100/4) & 16) || (board_read(pmu + 2) & 16) ||
        (board_read(pmu + 0x108/4) & 0x1ff) != 128) return 0;
    board_write(tlb, 0x0200007fUL);
    if (board_read(tlb + 1)) return 0;
    board_write(tlb, 0x03010000UL);
    if (board_read(tlb + 1) != 0x80000000UL) return 0;
    old_entry = board_read(entry);
    old_index = board_read(imem);
    old_tag = board_read(imem + 2);
    old_data = board_read(dmem);
    board_write(dmem, 0x02000100UL);
    for (i = 0; i < 2; ++i) old_words[i] = board_read(dmem + 1);
    board_write(imem, 0x01007f00UL);
    board_write(imem + 2, 0x100);
    for (i = 0; i < 256; i += 4) {
        word = 0;
        for (j = 0; j < 4 && i + j < sizeof(code); ++j)
            word |= (board_word)code[i + j] << (j * 8);
        board_write(imem + 1, word);
    }
    board_write(tlb, 0x03010000UL);
    if (board_read(tlb + 1) != 0x0100007fUL) goto restore;
    for (trial = 0; trial < 2; ++trial) {
        input = trial ? 0xfffffffcUL : 0x61a87d33UL;
        board_write(dmem, 0x01000100UL);
        board_write(dmem + 1, input);
        board_write(dmem + 1, input ^ 0x5a5a5a5aUL);
        board_write(entry, 0x10000UL);
        board_write(pmu + 0x100/4, 2);
        for (i = 0; i < 1000 && !(board_read(pmu + 0x100/4) & 16); ++i)
            board_delay();
        if (!(board_read(pmu + 0x100/4) & 16)) return 0;
        board_write(dmem, 0x104);
        if (!(board_read(pmu + 2) & 16) || board_read(dmem + 1) != input + 7) goto restore;
        board_write(pmu + 1, 16);
    }
    ok = 1;
restore:
    board_write(tlb, 0x0100007fUL);
    board_write(tlb, 0x0200007fUL);
    if (board_read(tlb + 1)) ok = 0;
    board_write(dmem, 0x01000100UL);
    for (i = 0; i < 2; ++i) board_write(dmem + 1, old_words[i]);
    board_write(dmem, old_data);
    board_write(imem, old_index);
    board_write(imem + 2, old_tag);
    board_write(entry, old_entry);
    return ok;
}
