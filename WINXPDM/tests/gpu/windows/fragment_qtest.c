/* Run the Windows fragment test vectors through QEMU's actual PCI I/O BAR.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
static FILE *qtest;

static uint32_t io(const char *op, unsigned port, uint32_t value)
{
    char reply[128];
    if (*op == 'o') fprintf(qtest,"%s 0x%x 0x%x\n",op,port,value);
    else fprintf(qtest,"%s 0x%x\n",op,port);
    fflush(qtest);
    do {
        if (!fgets(reply,sizeof(reply),qtest)) exit(2);
    } while (!strncmp(reply,"IRQ ",4));
    if (strncmp(reply,"OK",2)) { fputs(reply,stderr); exit(2); }
    return strtoul(reply+2,NULL,0);
}

static uint32_t reg_read(unsigned port, unsigned offset) { return io("inl",port+offset,0); }
static void reg_write(unsigned port, unsigned offset, uint32_t value) { io("outl",port+offset,value); }
#include "fragments.h"

int main(int argc, char **argv)
{
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    int fd = socket(AF_UNIX, SOCK_STREAM, 0), failures = 0;
    if (argc != 2 || fd < 0 || strlen(argv[1]) >= sizeof(addr.sun_path)) return 2;
    strcpy(addr.sun_path,argv[1]);
    if (connect(fd,(struct sockaddr *)&addr,sizeof(addr)) || !(qtest=fdopen(fd,"r+"))) return 2;
    io("outl",0xcf8,0x80003000);
    if (io("inl",0xcfc,0) != 0xff011b36) return 2;
    io("outl",0xcf8,0x80003010); io("outl",0xcfc,0xc001);
    io("outl",0xcf8,0x80003004); io("outw",0xcfc,1);
    if (reg_read(0xc000,0) != 0x31505857) return 2;
    for (unsigned i = 0; i < FRAG_COUNT(fragment_targets); i++) {
        int stage = frag_case(0xc000, fragment_targets[i]);
        printf("%04x %s stage=%d\n",fragment_targets[i],stage ? "FAIL" : "PASS",stage);
        fflush(stdout);
        if (stage) failures++;
    }
    fclose(qtest);
    return !!failures;
}
