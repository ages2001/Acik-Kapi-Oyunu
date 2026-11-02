"""Exercise production Program Manager parsing and command ownership bounds."""
from pathlib import Path
import re, subprocess, tempfile

source = (Path(__file__).resolve().parents[1] / 'RLTKLANT/tools/rltsetup.c').read_text()

def function(name):
    match = re.search(r'^static [^\n]*\b' + name + r'\([^\n]*\)', source, re.M)
    assert match, name
    at = source.index('{', match.start())
    depth = 0
    for end in range(at, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if not depth:
            return source[match.start():end + 1] + '\n'
    raise AssertionError(name)

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define lstrcmpiA strcasecmp
static char long_command[1024]="";
static char command[1024]="C:\\WINNT31\\system32\\DHCPNT31.EXE /auto /apply";
'''
tests = r'''
static void field_case(const char*in,const char*expected,int delim){const char*p=in;char out[64];int d=0;assert(csv_field(&p,in+strlen(in),out,sizeof(out),&d));assert(!strcmp(out,expected)&&d==delim);}
int main(void){unsigned i;uint32_t state=0x174223U;const char*p;char out[16];int delimiter;char*data;
 field_case("\"Startup\",Startup,1,2","Startup",',');
 field_case("\"RLTKLANT DHCP\",\"C:\\winnt31\\system32\\DHCPNT31.EXE /auto /apply\"","RLTKLANT DHCP",',');
 field_case("\"A,B\",rest","A,B",',');field_case("\"A\"\"B\"\r\n","A\"B",'\n');
 field_case("plain\r\n","plain",'\n');field_case("\"\"","",'\n');field_case(",","",',');
 p="\"missing";assert(!csv_field(&p,p+strlen(p),out,sizeof(out),&delimiter));
 p="\"closed\"junk,";assert(!csv_field(&p,p+strlen(p),out,sizeof(out),&delimiter));
 p="\"line\nbreak\",";assert(!csv_field(&p,p+strlen(p),out,sizeof(out),&delimiter));
 p="1234567890123456,";assert(!csv_field(&p,p+strlen(p),out,sizeof(out),&delimiter));
 assert(owned_command("c:\\winnt31\\system32\\dhcpnt31.exe /AUTO /APPLY"));
 assert(owned_command("C:\\\\winnt31\\\\system32\\\\DHCPNT31.EXE /auto /apply"));
 assert(owned_command("\"C:\\winnt31\\system32\\DHCPNT31.EXE\" /auto /apply"));
 assert(!owned_command("C:\\other\\DHCPNT31.EXE /auto /apply"));
 assert(!owned_command("C:\\WINNT31\\system32\\DHCPNT31.EXE /auto /apply /extra"));
 assert(!owned_command("C:\\WINNT31\\system32\\DHCPNT31.EXE /auto\n/apply"));
 assert(safe_text("RltBoot")&&safe_text("My Startup Group"));
 assert(!safe_text("")&&!safe_text("x][DeleteGroup(Startup)")&&!safe_text("x\"y")&&!safe_text("x\ny"));
 for(i=0;i<65536;i++){
  unsigned j,n=i%512;const char*end;data=malloc(n?n:1);assert(data);
  for(j=0;j<n;j++){state=state*1664525U+1013904223U;data[j]=(char)(state>>24);}
  p=data;end=data+n;(void)csv_field(&p,end,out,sizeof(out),&delimiter);assert(p>=data&&p<=end);free(data);
 }
 puts("PASS: Program Manager CSV truncation/escaping/bounds, exact command ownership, injection rejection and 65536 exact-length fuzz inputs");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-startup-parser-') as directory:
    code, executable = Path(directory)/'test.c', Path(directory)/'test'
    code.write_text(prefix + ''.join(function(n) for n in ['safe_text', 'csv_field', 'normalized_command', 'owned_command']) + tests)
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', str(code), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
