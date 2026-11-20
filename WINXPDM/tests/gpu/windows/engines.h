/* Guest command streams for the QEMU 1b36:ff01 test function.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef WINXPDM_ENGINE_TEST_H
#define WINXPDM_ENGINE_TEST_H
#include <stdint.h>
#include "../../../src/display/links.h"

#ifdef __WATCOMC__
static uint32_t read_port(unsigned port)
{
    uint32_t value;
    _asm {
        mov dx, port
        in eax, dx
        mov value, eax
    }
    return value;
}
static void write_port(unsigned port, uint32_t value)
{
    _asm {
        mov dx, port
        mov eax, value
        out dx, eax
    }
}
#else
static uint32_t read_port(unsigned port)
{
    uint32_t v;
    __asm__ volatile("inl %w1,%0" : "=a"(v) : "d"((unsigned short)port)); return v;
}
static void write_port(unsigned port, uint32_t v)
{ __asm__ volatile("outl %0,%w1" : : "a"(v), "d"((unsigned short)port)); }
#endif

static uint32_t diagnostic_cfg;
static uint32_t reg_read(unsigned port,unsigned offset)
{
    uint32_t saved, value;
    if (port != 0xffff) return read_port(port+offset);
    saved = read_port(0xcf8); write_port(0xcf8, diagnostic_cfg | (0x80+offset));
    value = read_port(0xcfc); write_port(0xcf8,saved);
    return value;
}
static void reg_write(unsigned port,unsigned offset,uint32_t value)
{
    uint32_t saved;
    if (port != 0xffff) { write_port(port+offset,value); return; }
    saved = read_port(0xcf8); write_port(0xcf8, diagnostic_cfg | (0x80+offset));
    write_port(0xcfc,value); write_port(0xcf8,saved);
}
static unsigned engine_port(void)
{
    unsigned s;
    uint32_t before = read_port(0xcf8), bar = 0;
    /* The test machine provisions this function on the root bus. */
    for (s = 0; s < 32; ++s) {
        uint32_t cfg = 0x80000000UL | ((uint32_t)s << 11);
        write_port(0xcf8, cfg);
        if (read_port(0xcfc) != 0xff011b36UL) continue;
        diagnostic_cfg = cfg;
        write_port(0xcf8, cfg | 0x10);
        bar = read_port(0xcfc);
        break;
    }
    write_port(0xcf8, before);
    if ((bar & 1) && bar <= 0xffe1 && (bar & ~3UL)) {
        s = (unsigned)(bar & ~3UL);
        if (read_port(s) == 0x31505857UL) return s;
    }
    return diagnostic_cfg && reg_read(0xffff,0) == 0x31505857UL ? 0xffff : 0;
}
static void nv_method(unsigned port, unsigned method, unsigned count, const uint32_t *values)
{
    unsigned i;
    reg_write(port,4, 0x20000000UL | (uint32_t)count << 16 | (method >> 2));
    for (i = 0; i < count; ++i) reg_write(port,4, values[i]);
}
static uint32_t memory_word(unsigned port, uint32_t address)
{
    reg_write(port,8,address);
    return reg_read(port,12);
}

static int check_nv(unsigned port, uint32_t cls)
{
    static const uint32_t target[] = {0, 0x1000, 128, 12, 0xd5, 1UL << 12};
    static const uint32_t rgba[] = {0x3f800000UL, 0, 0, 0x3f800000UL};
    static const uint32_t surface[] = {0xd5, 1};
    static const uint32_t dimensions[] = {128, 32, 12, 0, 0x1000};
    static const uint32_t prim[] = {3, 0xd5, 0xff00ff00UL};
    static const uint32_t vertices[] = {1, 1, 9, 1, 1, 9};
    uint32_t v; unsigned x, y;
    nv_method(port,0,1,&cls);
    nv_method(port,0x800,6,target);
    v = 0; nv_method(port,0x10f8,1,&v);
    nv_method(port,0xd80,4,rgba);
    v = 0x3c; nv_method(port,0x19d0,1,&v);
    if (reg_read(port,20) || memory_word(port,0x1000UL) != 0xff0000ffUL) return 0;
    v = 0x902d; nv_method(port,0,1,&v);
    nv_method(port,0x200,2,surface); nv_method(port,0x214,5,dimensions);
    v = 3; nv_method(port,0x2ac,1,&v);
    nv_method(port,0x580,3,prim); nv_method(port,0x600,6,vertices);
    if (reg_read(port,20)) return 0;
    for (y = 0; y < 12; ++y) for (x = 0; x < 32; ++x) {
        uint32_t expected = x >= 1 && y >= 1 && x+y < 9 ? 0xff00ff00UL : 0xff0000ffUL;
        if (memory_word(port,0x1000UL+y*128+x*4) != expected) return 0;
    }
    return !memory_word(port,0xffc) && !memory_word(port,0x1600);
}
static int check_sdma(unsigned port)
{
    static const uint32_t fill[] = {0x1000b, 0x2000, 0, 0x9138cc77UL, 255};
    static const uint32_t copy[] = {0x80001, 12, 0, 0x2002, 0, 0x2201, 0, 0};
    static const uint32_t expected[] = {0x77913800UL, 0x779138ccUL, 0x779138ccUL, 0x000038ccUL};
    static const uint32_t invalid[] = {0x1000b, 0xfffc, 0, 0xffffffffUL, 7};
    unsigned i;
    for (i = 0; i < 5; ++i) reg_write(port,16,fill[i]);
    for (i = 0; i < 8; ++i) reg_write(port,16,copy[i]);
    if (reg_read(port,20)) return 0;
    for (i = 0; i < 64; ++i)
        if (memory_word(port,0x80002000UL+i*4) != fill[3]) return 0;
    for (i = 0; i < 4; ++i)
        if (memory_word(port,0x80002200UL+i*4) != expected[i]) return 0;
    if (memory_word(port,0x80001ffcUL) || memory_word(port,0x80002100UL) ||
        memory_word(port,0x80002210UL)) return 0;
    for (i = 0; i < 5; ++i) reg_write(port,16,invalid[i]);
    if (reg_read(port,20) != 3) return 0;
    reg_write(port,20,0);
    return !memory_word(port,0x8000fffcUL);
}

static int receive_link(void *context,unsigned bus,unsigned addr,uint8_t *data,unsigned length)
{
    unsigned port=*(unsigned *)context, i;
    for (i=0; i<length; ++i) {
        reg_write(port,24,(bus==WXP_SCDC ? 0x80000000UL : 0) | (addr+i));
        data[i]=(uint8_t)reg_read(port,28);
    }
    return 1;
}
static int send_link(void *context,unsigned bus,unsigned addr,const uint8_t *data,unsigned length)
{
    unsigned port=*(unsigned *)context, i;
    for (i=0; i<length; ++i) {
        reg_write(port,24,(bus==WXP_SCDC ? 0x80000000UL : 0) | (addr+i));
        reg_write(port,28,data[i]);
    }
    return 1;
}
static void set_source(unsigned port,unsigned reg,unsigned value)
{ reg_write(port,24,0x40000000UL|reg); reg_write(port,28,value); }
static int dp_source(void *context,unsigned rate,unsigned lanes,unsigned pattern,const uint8_t drive[4])
{
    unsigned port=*(unsigned *)context,i;
    set_source(port,0,rate); set_source(port,1,lanes); set_source(port,2,pattern);
    for (i=0;i<4;++i) set_source(port,3+i,drive[i]);
    return 1;
}
static int hdmi_source(void *context,uint32_t clock,unsigned config)
{
    unsigned port=*(unsigned *)context,i;
    for (i=0; i<4; ++i) { set_source(port,16+i,(unsigned)(clock&255)); clock >>= 8; }
    set_source(port,20,config);
    return 1;
}
static void link_delay(unsigned us)
{
#ifdef __WATCOMC__
    DWORD start=GetTickCount();
    while (GetTickCount()-start < (us+999U)/1000U) Yield();
#else
    LARGE_INTEGER delay;
    delay.QuadPart = -(LONGLONG)us*10;
    KeDelayExecutionThread(KernelMode,FALSE,&delay);
#endif
}
static int one_lane_source(void *context,unsigned rate,unsigned lanes,unsigned pattern,const uint8_t drive[4])
{
    (void)lanes;
    return dp_source(context,rate,1,pattern,drive);
}
static int check_dp_link(WxpLink *l)
{
    uint8_t config[7],status[3];
    unsigned i;
    l->dp_source=one_lane_source;
    if (wxp_dp_hbr3(l)) return 0;
    receive_link(l->context,WXP_DPCD,0x102,config,1);
    if (config[0]) return 0;
    l->dp_source=dp_source;
    if (!wxp_dp_hbr3(l)) return 0;
    receive_link(l->context,WXP_DPCD,0x100,config,7);
    receive_link(l->context,WXP_DPCD,0x202,status,3);
    if (config[0]!=0x1e || config[1]!=0x84 || config[2] || status[0]!=0x77 ||
        status[1]!=0x77 || !(status[2]&1)) return 0;
    for (i=3;i<7;++i) if (config[i]!=1) return 0;
    return 1;
}
static int check_hdmi_link(WxpLink *l)
{
    uint8_t config,scramble;
    if (!wxp_hdmi_tmds(l,594000UL)) return 0;
    receive_link(l->context,WXP_SCDC,0x20,&config,1);
    if (config!=3 || !wxp_hdmi_tmds(l,148500UL)) return 0;
    receive_link(l->context,WXP_SCDC,0x20,&config,1);
    receive_link(l->context,WXP_SCDC,0x21,&scramble,1);
    return !config && !scramble && !wxp_hdmi_tmds(l,600001UL);
}
static void check_engines(void (*log_line)(const char *))
{
    WxpLink link;
    unsigned port = engine_port();
    if (!port) { log_line("Vendor command port absent\n"); return; }
    reg_write(port,20,0);
    log_line(check_nv(port,0xb097) ? "Maxwell A B097 + 902D PASS\n" : "Maxwell A FAIL\n");
    log_line(check_nv(port,0xb197) ? "Maxwell B B197 + 902D PASS\n" : "Maxwell B FAIL\n");
    log_line(check_nv(port,0xc197) ? "Pascal B C197 + 902D PASS\n" : "Pascal B FAIL\n");
    log_line(check_sdma(port) ? "RDNA 4 SDMA 7 fill, unaligned copy and guards PASS\n" : "RDNA 4 SDMA FAIL\n");
    link.context=&port; link.receive=receive_link; link.send=send_link;
    link.dp_source=dp_source; link.hdmi_source=hdmi_source; link.wait_us=link_delay;
    log_line(check_dp_link(&link) ? "DPCD sink model HBR3 x4 TPS1/TPS4 and adjust request PASS\n" : "DPCD FAIL\n");
    log_line(check_hdmi_link(&link) ? "HDMI SCDC sink model 594 MHz 1:40 and 148.5 MHz 1:10 PASS\n" : "SCDC FAIL\n");
}
#endif
