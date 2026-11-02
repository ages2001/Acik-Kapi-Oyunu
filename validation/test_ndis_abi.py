"""Compare miniport registration ABI with the installed DDK on x86 and x64."""
import os, pathlib, re, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[1]
fields = ['MajorNdisVersion', 'MinorNdisVersion', 'Reserved', 'CheckForHangHandler',
          'InitializeHandler', 'TransferDataHandler', 'ReturnPacketHandler',
          'CoRequestHandler', 'CancelSendPacketsHandler', 'PnPEventNotifyHandler',
          'AdapterShutdownHandler', 'Reserved4']
# The packaged DDK has unrelated duplicate enums and a missing comma in a
# WAN prototype. Extract its unchanged C layout macros, substituting only
# pointer typedefs for callback types; do not patch the reference headers.
ddk = (pathlib.Path(os.environ['MINGW_DDK_INCLUDE']) / 'ndis.h').read_text()
macros = []
for version in ['30', '40', '50', '51']:
    matches = []
    lines = ddk.splitlines()
    for start, line in enumerate(lines):
        if line.startswith('#define NDIS'+version+'_MINIPORT_CHARACTERISTICS_S '):
            block = [line]
            while block[-1].endswith(chr(92)):
                block.append(lines[start + len(block)])
            matches.append('\n'.join(block))
    macros.append(matches[-1])
layout = '\n'.join(macros)
types = sorted(set(re.findall(r'\bW_[A-Z_]+\b', layout)))
reference = '#include <windows.h>\n' + ''.join('typedef void *'+t+';\n' for t in types) + layout + '\ntypedef struct { NDIS51_MINIPORT_CHARACTERISTICS_S } NDIS_MINIPORT_CHARACTERISTICS;\n'
for arch in ['i686', 'x86_64']:
    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        blobs = []
        for name, header in [('project', '#define RTL_NDIS5\n#include "ndiskit.h"'),
                             ('ddk', reference)]:
            values = ['sizeof(NDIS_MINIPORT_CHARACTERISTICS)'] + [
                '__builtin_offsetof(NDIS_MINIPORT_CHARACTERISTICS, '+f+')' for f in fields]
            src = td / (name+'.c')
            src.write_text(header+'\nconst unsigned int layout[] __attribute__((section(".abidata"))) = {'+','.join(values)+'};\n')
            obj = td / (name+'.o'); binary = td / (name+'.bin')
            subprocess.run([arch+'-w64-mingw32-gcc','-c','-I'+os.environ['MINGW_DDK_INCLUDE'],'-I'+str(root/'RLTKLANT/src'),str(src),'-o',str(obj)], check=True)
            subprocess.run([arch+'-w64-mingw32-objcopy','-O','binary','--only-section=.abidata',str(obj),str(binary)],check=True)
            blobs.append(binary.read_bytes()[:4*len(values)])
        assert blobs[0] == blobs[1], (arch, blobs)
        print('PASS:',arch,'NDIS 5.1 size and field offsets; 5.0 prefix boundary')
