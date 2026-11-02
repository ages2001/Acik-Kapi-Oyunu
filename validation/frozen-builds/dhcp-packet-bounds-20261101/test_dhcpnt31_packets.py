"""Exercise production DHCP receive parsing at actual datagram boundaries."""
from pathlib import Path
import re, subprocess, tempfile

source = (Path(__file__).resolve().parents[1] / 'RLTKLANT/DHCPNT31/src/dhcpnt31.c').read_text()
def function(name):
    match = re.search(r'static [^;{}]+\b' + name + r'\([^;{}]*\)\s*\{', source)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

structure = source[source.index('#pragma pack(push,1)'):source.index('#pragma pack(pop)')+len('#pragma pack(pop)')]
structure = structure.replace('unsigned long', 'uint32_t').replace('unsigned short', 'uint16_t')
prefix = '''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define BOOTREPLY 2
#define OPT_PAD 0
#define OPT_END 255
'''
test = r'''
static uint32_t rng = 0x81578159;
static unsigned next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
int main(void) {
    DHCP_MSG full; unsigned i, n; unsigned char length, mac[6] = {0x52,0x54,0,0xab,0xcd,0xef};
    const unsigned char options[] = {99,130,83,99,53,1,2,54,4,10,0,2,2,1,4,255,255,255,0,255};
    assert(sizeof(full) == 548);
    memset(&full, 0, sizeof(full)); full.op=2; full.htype=1; full.hlen=6; full.xid=0x12345678;
    memcpy(full.chaddr, mac, 6); memcpy(full.options, options, sizeof(options));
    n = 236 + sizeof(options);
    assert(valid_reply(&full,n,full.xid,mac));
    assert(*find_opt(&full,n,53,&length)==2 && length==1);
    assert(!find_opt(&full,n,51,&length) && length==0);
    assert(!valid_reply(&full,n,full.xid+1,mac));
    for(i=0;i<n;i++) {
        unsigned char *packet=malloc(i?i:1); memcpy(packet,&full,i);
        assert(!valid_reply((const DHCP_MSG*)packet,i,full.xid,mac));
        (void)find_opt((const DHCP_MSG*)packet,i,54,&length);
        free(packet);
    }
    for(i=0;i<6;i++) { full.chaddr[i]^=1; assert(!valid_reply(&full,n,full.xid,mac)); full.chaddr[i]^=1; }
    full.op=1; assert(!valid_reply(&full,n,full.xid,mac)); full.op=2;
    full.htype=2; assert(!valid_reply(&full,n,full.xid,mac)); full.htype=1;
    full.hlen=5; assert(!valid_reply(&full,n,full.xid,mac)); full.hlen=6;
    for(i=0;i<4;i++) { full.options[i]^=1; assert(!valid_reply(&full,n,full.xid,mac)); full.options[i]^=1; }
    full.options[5]=255;
    assert(!valid_reply(&full,n,full.xid,mac)); assert(!find_opt(&full,n,53,&length));
    full.options[5]=1;
    assert(!valid_reply(&full,sizeof(full)+1,full.xid,mac));
    assert(!find_opt(&full,sizeof(full)+1,53,&length));
    /* Actual-size allocations expose stale-tail reads under ASan. */
    for(i=0;i<65536;i++) {
        unsigned j, size=next()%549; unsigned char *packet=malloc(size?size:1);
        for(j=0;j<size;j++) packet[j]=(unsigned char)next();
        if(size>=240) { memcpy(packet,&full,240); }
        (void)valid_reply((const DHCP_MSG*)packet,size,full.xid,mac);
        for(j=0;j<4;j++) (void)find_opt((const DHCP_MSG*)packet,size,(unsigned char)next(),&length);
        free(packet);
    }
    puts("PASS: DHCP exact-length truncation, header/cookie/MAC/XID mismatch, malformed options and 65536 bounded fuzz datagrams");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-dhcp-packets-') as directory:
    code, exe = Path(directory)/'test.c', Path(directory)/'test'
    code.write_text(prefix + structure + '\n' + function('find_opt') + '\n' + function('valid_reply') + test)
    subprocess.run(['gcc', '-std=c99', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(code), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
