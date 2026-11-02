"""Compare USB firmware writes to the actual pinned Linux block handlers.

The generated oracle retains the upstream function bodies unchanged. A small
transport shim models register storage/handshakes, not PHY/MCU execution.
Fault injection separately exercises production transfer/wait cancellation.
"""
from pathlib import Path
import hashlib,json,re,subprocess,tempfile
here=Path(__file__).resolve().parent
root=here.parent/'RLTKLANT';src=root/'USBNT/src'
reference=Path('/workspace/rltklant-tools/linux-net-reference/r8152.c')
data=reference.read_bytes()
assert hashlib.sha256(data).hexdigest()=='5bd96f8f64dffd5d57b9b0f5bbe1117454750154b281f6106529ee62c5ae49d7'
text=data.decode()
functions=['rtl_clear_bp','rtl_reset_ocp_base','rtl_phy_patch_request','rtl_patch_key_set',
           'rtl_pre_ram_code','rtl_post_ram_code','rtl_ram_code_speed_up',
           'rtl8152_fw_phy_ver','rtl8152_fw_phy_fixup','rtl8152_fw_phy_union_apply',
           'rtl8152_fw_phy_nc_apply','rtl8152_fw_mac_apply','rtl8152_apply_firmware',
           'r8153_pre_firmware_1','r8153_post_firmware_1','r8153_pre_firmware_2',
           'r8153_post_firmware_2','r8153_post_firmware_3','r8153b_pre_firmware_1',
           'r8153b_post_firmware_1','r8153c_post_firmware_1','r8156a_post_firmware_1']
def function(name,source=text):
    match=re.search(r'^static [^;{}]*\b'+name+r'\([^;{}]*\)\s*\{',source,re.M)
    assert match,name
    start=match.start();pos=match.end();depth=1
    while depth:
        depth+=(source[pos]=='{')-(source[pos]=='}');pos+=1
    return source[start:pos]
bodies='\n\n'.join(function(name) for name in functions)
structs=text[text.index('struct fw_block {'):text.index('enum rtl_version {')]
macros={m[1]:m[0] for m in re.finditer(r'^#define\s+([A-Z][A-Z0-9_]*)[^\n]*$',text,re.M)}
needed=set(re.findall(r'\b[A-Z][A-Z0-9_]*\b',bodies+structs))
selected={}
while True:
    todo=(needed&macros.keys())-selected.keys()
    if not todo:break
    for name in todo:
        selected[name]=macros[name]
        needed.update(re.findall(r'\b[A-Z][A-Z0-9_]*\b',macros[name]))
oracle='\n'.join(selected[n] for n in sorted(selected))+'\n'
oracle+='\n'.join('#define RTL_VER_%02d %d'%(v,v) for v in range(1,18))+'\n'
oracle+=structs+'\n'+(here/'usbfw_oracle_shim.h').read_text()+'\n'+bodies
vendor=(root/'third_party/r8152/vendor-r8152.c').read_bytes()
assert hashlib.sha256(vendor).hexdigest()=='87397a44617a38ed21720cfd4b50f2dbaea263291f94915a18442ca846f7e7ca'
vendor=vendor.decode()
vendor_oracle=(here/'usbfw_vendor_oracle_shim.h').read_text()+'\n'
vendor_oracle+=function('r8156b_flash_used',vendor)+'\n'+function('rtl_ram_code_speed_up',vendor)
vendor_oracle+='\n#undef ocp_read_word\n#undef ocp_word_set_bits\n#undef sram_read\n#undef rtl_ram_code_speed_up\n'
args=[]
for directory in (root/'firmware/usb',root/'firmware/usb-vendor'):
    for row in json.loads((directory/'manifest.json').read_text())['files']:
        for version in row['linux_version_suffixes']:args += [str(directory/row['path']),str(version)]
with tempfile.TemporaryDirectory(prefix='rltk-usbfw-exec-') as td:
    td=Path(td);(td/'oracle.h').write_text(oracle);(td/'vendor-oracle.h').write_text(vendor_oracle)
    exe=td/'test'
    subprocess.run(['gcc','-std=c99','-O1','-Wall','-Wextra','-Werror','-Wno-sign-compare',
                    '-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(td),
                    str(here/'test_usbfw_exec.c'),
                    *(str(src/(name+'.c')) for name in ('usbfw','usbfwsha','usbfwexec')),
                    '-o',str(exe)],check=True)
    subprocess.run([str(exe),*args],check=True)
    for arch in ('i686','x86_64'):
        subprocess.run([arch+'-w64-mingw32-gcc','-std=c99','-Wall','-Wextra','-Werror',
                        '-O2','-ffreestanding','-fno-builtin','-c',str(src/'usbfwexec.c'),
                        '-o',str(td/(arch+'.o'))],check=True)
    print('PASS: USB firmware executor cross-compiles for x86/x64')
