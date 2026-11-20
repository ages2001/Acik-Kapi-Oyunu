/* Run the real guest driver against QEMU PCI/DMA before booting Windows.
 * The halted guest and the probe share a memory-backend-file.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <time.h>
#include "scene.h"

static FILE *qtest;
static unsigned long io(const char *operation, unsigned port, uint32_t data)
{
    char line[192], reply[192];
    if (operation[0] == 'i') snprintf(line,sizeof(line),"%s 0x%x\n",operation,port);
    else snprintf(line,sizeof(line),"%s 0x%x 0x%x\n",operation,port,data);
    fputs(line,qtest); fflush(qtest);
    do {
        if (!fgets(reply,sizeof(reply),qtest)) { perror("qtest response"); exit(2); }
    } while (!strncmp(reply,"IRQ ",4));
    if (strncmp(reply,"OK",2)) { fprintf(stderr,"%s: %s",line,reply); exit(2); }
    return strtoul(reply+2, NULL, 0);
}
static unsigned select_pci(unsigned bus, unsigned slot, unsigned reg)
{
    io("outl",0xcf8,0x80000000UL | (bus << 16) | (slot << 8) | (reg & 252));
    return 0xcfc + (reg & 3);
}
static uint32_t read_cfg(unsigned bus, unsigned slot, unsigned reg, unsigned n)
{
    return io(n == 4 ? "inl" : n == 2 ? "inw" : "inb", select_pci(bus,slot,reg),0);
}
static void write_cfg(unsigned bus, unsigned slot, unsigned reg, unsigned n, uint32_t v)
{
    io(n == 4 ? "outl" : n == 2 ? "outw" : "outb",select_pci(bus,slot,reg),v);
}
static uint32_t ticks(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint32_t)(t.tv_sec*1000UL+t.tv_nsec/1000000UL);
}
static void poll_gpu(void) { struct timespec delay={0,1000000}; nanosleep(&delay,NULL); }

static void observe(void)
{
    const char *marker = getenv("WINXPDM_SCANOUT_MARKER");
    if (marker) {
        FILE *f = fopen(marker,"w");
        uint32_t start = ticks();
        if (!f) { perror("scanout marker"); exit(2); }
        fclose(f);
        while (!access(marker,F_OK) && (uint32_t)(ticks()-start)<30000) poll_gpu();
    }
}

int main(int argc, char **argv)
{
    struct sockaddr_un addr = {0};
    WxpGpu g = {0};
    WxpFrame frame;
    unsigned i, black=0, green=0;
    if (argc != 3 || strlen(argv[1]) >= sizeof(addr.sun_path)) return 2;
    int sock=socket(AF_UNIX,SOCK_STREAM,0), mem=open(argv[2],O_RDWR);
    addr.sun_family=AF_UNIX; strcpy(addr.sun_path,argv[1]);
    if (mem < 0 || sock < 0 || connect(sock,(void*)&addr,sizeof(addr))) {perror("QEMU");return 2;}
    qtest=fdopen(sock,"r+"); if (!qtest) return 2;
    for (i=0;i<WXP_DMA_SIZE/4096;++i) g.page[i]=0x800000UL+4096UL*i;
    g.dma=mmap(NULL,WXP_DMA_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,mem,g.page[0]);
    if (g.dma==MAP_FAILED) return 2;
    g.read=read_cfg;g.write=write_cfg;g.time_ms=ticks;g.poll=poll_gpu;g.observe_outputs=observe;
    wxp_gpu_draw(&g,&scene,&frame);
    for(i=0;i<WXP_SIDE*WXP_SIDE;++i) {
        if(frame.pixels[i]==0xff000000UL)++black;
        if(frame.pixels[i]==0xff00ff00UL)++green;
    }
    printf("status=%u request=%04x reply=%04x commands=%u fence=%u outputs=%u enabled=%x edid=%x scanouts=%x black=%u green=%u center=%08x\n",
           frame.status,frame.request,frame.response,frame.completed,frame.fence,frame.outputs,frame.enabled_outputs,
           frame.edid_outputs,frame.tested_outputs,black,green,frame.pixels[16*WXP_SIDE+16]);
    if(!check_scene(&frame)) return 1;
    puts("Offscreen VirGL triangle interpolation, depth ordering and clear passed");
    if (wxp_gpu_draw(&g,&textured,&frame) || !check_texture(&frame)) {
        fprintf(stderr,"Texture failure status=%u request=%04x reply=%04x samples=%08x,%08x,%08x,%08x\n",
                frame.status,frame.request,frame.response,frame.pixels[0],frame.pixels[31],
                frame.pixels[31*32],frame.pixels[1023]); return 1;
    }
    puts("Second draw texture sample passed 1024 pixels");
    if (wxp_gpu_close(&g) || g.owned) return 1;
    if (wxp_gpu_draw(&g,&textured,&frame) || !check_texture(&frame) || wxp_gpu_close(&g)) return 1;
    puts("Device close and reopen passed");
    return 0;
}
