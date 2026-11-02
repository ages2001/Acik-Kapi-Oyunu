"""Build an NT 3.1 diagnostic adaptation of original FP32; never fake missing APIs."""
from pathlib import Path
import ast
import hashlib
import json
import subprocess
import sys
import pefile
from original import original_player
from original_icon import prepare_icon

root = Path(__file__).resolve().parents[2]
work = root/'fp32'
out = work/'build'
src = original_player()
tool = root/'tools/usr/bin'
libs = root/'tools/usr/i686-w64-mingw32/lib'
out.mkdir(exist_ok=True)
prepare_icon(out, root/'tools/usr')
def run(args):
    subprocess.run([str(a) for a in args], check=True)
def compile_c(name, flags=()):
    run([tool/'clang-19', '--target=i686-w64-windows-gnu', '-isystem', root/'tools/usr/share/mingw-w64/include',
         '-I',root/'compat',
         '-march=i386', '-mno-sse', '-mno-sse2', '-ffreestanding', '-fno-stack-protector', '-fno-builtin',
         '-Os', *flags, '-c', out/(name+'.c'), '-o', out/(name+'.obj')])

# Reuse the FP10 API implementations without running or overwriting its build.
metadata = {}
for node in ast.parse((root/'compat/build_trace.py').read_text()).body:
    if isinstance(node, ast.Assign):
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id in ('implemented','user_impl','gdi_impl','ole_impl'):
                metadata[target.id] = ast.literal_eval(node.value)
sources = ['trace', 'kernel', 'locale', 'user', 'gdi', 'registry', 'ole']
for name in sources:
    text = (root/'compat'/f'{name}.c').read_text()
    text = text.replace(r'C:\\FP10\\', r'C:\\FP32\\')
    if name == 'kernel':
        text = text.replace('fp_CreateFileW(', 'fp_BaseCreateFileW(').replace('fp_GetFileAttributesExW(', 'fp_BaseGetFileAttributesExW(')
        text = text.replace('static LPCWSTR legacy_file_path(', 'LPCWSTR fp_LegacyFilePath(').replace('legacy_file_path(name)', 'fp_LegacyFilePath(name)')
    if name == 'user':
        text = text.replace('c.hIcon=x->hIcon;', 'c.hIcon=x->hIcon; if(!IS_INTRESOURCE(x->lpszClassName)&&eqname(x->lpszClassName,L"ShockwaveFlash")){HICON icon=LoadIconW(GetModuleHandleW(L"F32U.DLL"),MAKEINTRESOURCEW(200));if(icon)c.hIcon=icon;}')
        text = text.replace('Flash Player 10.3.183.90', 'Flash Player 32.0.0.465').replace('About Flash Player 10', 'About Flash Player 32')
        text = text.replace('Copyright 1996-2011 Adobe Systems Incorporated.', 'Copyright 1996-2020 Adobe. All Rights Reserved.')
    (out/(name+'.c')).write_text(text)
    compile_c(name, ['-DFP_OPTIONAL_TRACE'] if name == 'trace' else ['-DFP_SOFTWARE_RGB'] if name == 'gdi' else
              ['-DFP_CPU_MODE_LABEL=L"SSE2"', '-DFP_WINDOW_CREATE_DIAGNOSTICS'] if name == 'user' else
              ['-DFP_QUERY_XMM'] if name == 'kernel' else [])
(out/'trace-gdi.c').write_text((out/'trace.c').read_text())
compile_c('trace-gdi', ['-DFP_GDI','-DFP_OPTIONAL_TRACE'])
(out/'trace-crypto.c').write_text((out/'trace.c').read_text())
compile_c('trace-crypto', ['-DFP_CRYPTO','-DFP_OPTIONAL_TRACE'])

extra = {}
extra_source = work/'compat/kernel_extra.c'
if extra_source.exists():
    (out/'kernel_extra.c').write_text(extra_source.read_text())
    compile_c('kernel_extra')
    extra = json.loads((work/'compat/kernel_extra_exports.json').read_text())
metadata['implemented'].update(extra)
(out/'dynamic_shell.c').write_text((work/'compat/dynamic_shell.c').read_text())
compile_c('dynamic_shell')
(out/'profile.c').write_text((work/'compat/profile.c').read_text())
compile_c('profile')
(out/'save_paths.c').write_text((work/'compat/save_paths.c').read_text())
compile_c('save_paths')
metadata['implemented'].update({'CreateDirectoryW':8,'RemoveDirectoryW':4,'CreateFileW':28,'GetFileAttributesW':4,'DeleteFileW':4,'CopyFileW':12,'MoveFileExW':12,'CreateFileA':28,'GetFileAttributesA':4,'DeleteFileA':4,'CreateDirectoryA':8,'FindFirstFileW':8,'FindNextFileW':8,'FindClose':4,'GetFileAttributesExW':12,'GetLongPathNameW':12})
metadata['implemented']['GetProcAddress'] = 8
(out/'shell_args.c').write_text((work/'compat/shell_args.c').read_text())
compile_c('shell_args')
(out/'shell_strings.c').write_text((work/'compat/shell_strings.c').read_text())
compile_c('shell_strings')
(out/'ole_extra.c').write_text((work/'compat/ole_extra.c').read_text())
compile_c('ole_extra')
(out/'automation_strings.c').write_text((work/'compat/automation_strings.c').read_text())
compile_c('automation_strings')
metadata['ole_impl']['CoCreateGuid'] = 4
metadata['ole_impl']['StringFromGUID2'] = 12
(out/'user_extra.c').write_text((work/'compat/user_extra.c').read_text())
compile_c('user_extra')
metadata['user_impl'].update({'SetWindowLongW':12,'SetWindowLongA':12})
(out/'crypto_rng.c').write_text((work/'compat/crypto_rng.c').read_text())
compile_c('crypto_rng')
(out/'process_memory.c').write_text((work/'compat/process_memory.c').read_text())
compile_c('process_memory')
impl = {'kernel32.dll': metadata['implemented'], 'user32.dll': metadata['user_impl'],
        'gdi32.dll': metadata['gdi_impl'], 'ole32.dll': metadata['ole_impl'],
        'oleaut32.dll': {'SysAllocString':4, 'SysFreeString':4},
        'advapi32.dll': {'RegCreateKeyExA':36, 'RegCreateKeyExW':36,
                         'CryptAcquireContextA':20,'CryptAcquireContextW':20,
                         'CryptGenRandom':12,'CryptReleaseContext':8},
        'shell32.dll': {'CommandLineToArgvW':8},
        'shlwapi.dll': {'StrStrIW':8, 'StrRStrIW':12},
        'psapi.dll': {'GetProcessMemoryInfo':12}}
objects = {'kernel32.dll':['kernel','locale','dynamic_shell','profile','save_paths'] + (['kernel_extra'] if extra else []),
           'user32.dll':['user','user_extra'], 'gdi32.dll':['gdi'], 'ole32.dll':['ole','ole_extra'], 'oleaut32.dll':['automation_strings'], 'advapi32.dll':['registry','crypto_rng'],
           'shell32.dll':['shell_args'], 'shlwapi.dll':['shell_strings'],
           'psapi.dll':['process_memory']}
names = {'kernel32.dll':'F32K.DLL', 'advapi32.dll':'F32A.DLL', 'user32.dll':'F32U.DLL',
         'gdi32.dll':'F32G.DLL', 'ole32.dll':'F32O.DLL', 'oleaut32.dll':'F32OA.DLL',
         'comdlg32.dll':'F32CD.DLL', 'shell32.dll':'F32S.DLL', 'ws2_32.dll':'F32W.DLL',
         'shlwapi.dll':'F32SH.DLL', 'mscms.dll':'F32CM.DLL', 'wininet.dll':'F32I.DLL',
         'crypt32.dll':'F32C.DLL', 'urlmon.dll':'F32R.DLL', 'version.dll':'F32V.DLL',
         'winmm.dll':'F32M.DLL', 'dsound.dll':'F32D.DLL', 'psapi.dll':'F32P.DLL'}
p = pefile.PE(str(src))
native = {f.name.lower():f for f in (root/'nt31/system32').iterdir() if f.is_file()}
report = []
for kind, entries in [('load',p.DIRECTORY_ENTRY_IMPORT), ('delay',p.DIRECTORY_ENTRY_DELAY_IMPORT)]:
    for lib in entries:
        key = lib.dll.decode().lower()
        target = names[key]
        provider = 'wsock32.dll' if key == 'ws2_32.dll' else key
        available = set()
        if provider in native:
            q = pefile.PE(str(native[provider]))
            for e in q.DIRECTORY_ENTRY_EXPORT.symbols:
                available.add('#'+str(e.ordinal))
                if e.name:
                    available.add(e.name.decode())
        defs = ['LIBRARY '+target, 'EXPORTS']
        assembly = ['bits 32', 'section .text', 'extern _FpMissing@8']
        missing = []
        for idx, item in enumerate(lib.imports):
            name = item.name.decode() if item.name else '#'+str(item.ordinal)
            exported = name if item.name else 'ordinal'+str(item.ordinal)
            # pefile supplies convenience names for known ordinal imports too.
            # Their numeric lookup contract must still be preserved in the proxy.
            ordinal = (f' @{item.ordinal}'+('' if item.name else ' NONAME')) if item.import_by_ordinal else ''
            if name in impl.get(key, {}):
                defs.append(f'{exported}=_fp_{name}@{impl[key][name]}{ordinal}')
            elif name in available:
                defs.append(f'{exported}={provider[:-4]}.{name}{ordinal}')
            else:
                missing.append(name)
                defs.append(f'{exported}=missing{idx}{ordinal}')
                assembly += [f'global _missing{idx}',f'_missing{idx}:','mov eax,[esp]','push eax',
                             f'push label{idx}','call _FpMissing@8','int3','section .rdata',
                             f"label{idx}: db '{key}!{name}',0",'section .text']
        stem = target[:-4]
        (out/(stem+'.def')).write_text('\n'.join(defs)+'\n')
        (out/(stem+'.asm')).write_text('\n'.join(assembly)+'\n')
        run([tool/'nasm','-f','win32',out/(stem+'.asm'),'-o',out/(stem+'.obj')])
        run([tool/'lld-link-19','/nologo','/dll','/subsystem:windows,3.10','/osversion:3.10',
             '/entry:DllMain@12','/nodefaultlib','/safeseh:no','/machine:x86',
             '/def:'+str(out/(stem+'.def')),'/out:'+str(out/target),
             out/('trace-gdi.obj' if key=='gdi32.dll' else 'trace-crypto.obj' if key=='advapi32.dll' else 'trace.obj'),out/(stem+'.obj'),
             *[out/(n+'.obj') for n in objects.get(key,[])],
             *([out/'original-icon.obj'] if key=='user32.dll' else []),
             *[libs/('lib'+n+'.a') for n in ('kernel32','user32','gdi32','advapi32')]])
        q = pefile.PE(str(out/target))
        for e in q.DIRECTORY_ENTRY_EXPORT.symbols:
            if e.forwarder and e.forwarder.startswith(b'_'):
                assert e.forwarder.startswith(('_'+provider[:-4]+'.').encode())
                q.set_bytes_at_rva(e.address,e.forwarder[1:]+b'\0')
        q.write(str(out/target))
        address = lib.struct.Name if kind=='load' else lib.struct.szName
        assert len(target) <= len(lib.dll)
        if kind=='delay':
            assert lib.struct.grAttrs & 1, 'Expected RVA-based delay descriptor'
        p.set_bytes_at_rva(address,target.encode()+bytes(len(lib.dll)+1-len(target)))
        report.append({'kind':kind,'original':key,'proxy':target,'unimplemented_calls_abort':missing})

# This is a diagnostic copy: changing header minimums does not assert OS support.
p.OPTIONAL_HEADER.MajorOperatingSystemVersion = 3
p.OPTIONAL_HEADER.MinorOperatingSystemVersion = 10
p.OPTIONAL_HEADER.MajorSubsystemVersion = 3
p.OPTIONAL_HEADER.MinorSubsystemVersion = 10
p.OPTIONAL_HEADER.DATA_DIRECTORY[4].VirtualAddress = 0
p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size = 0
p.write(str(out/'FP32TEST.EXE'))
sys.path.insert(0,str(root/'analysis'))
from pad_legacy_pe import pad_image
padding = [pad_image(out/'FP32TEST.EXE')]+[pad_image(out/n) for n in names.values()]
ordinal_contracts = []
for lib in p.DIRECTORY_ENTRY_IMPORT+p.DIRECTORY_ENTRY_DELAY_IMPORT:
    target=names[lib.dll.decode().lower()]
    exports=pefile.PE(str(out/target)).DIRECTORY_ENTRY_EXPORT.symbols
    by_ordinal={e.ordinal:e for e in exports}
    for item in lib.imports:
        if not item.import_by_ordinal:continue
        assert item.ordinal in by_ordinal,(target,item.ordinal)
        entry=by_ordinal[item.ordinal]
        if item.name:assert entry.name==item.name,(target,item.ordinal,item.name,entry.name)
        ordinal_contracts.append({'proxy':target,'ordinal':item.ordinal,
                                  'name':item.name.decode() if item.name else None})
(work/'reports/ordinal-contracts.json').write_text(json.dumps(ordinal_contracts,indent=2)+'\n')
(work/'reports/build-proxies.json').write_text(json.dumps(report,indent=2)+'\n')
(work/'reports/build-padding.json').write_text(json.dumps(padding,indent=2)+'\n')
manifest = {n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ['FP32TEST.EXE',*names.values()]}
(work/'reports/build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Built diagnostic FP32 plus',len(names),'proxies; unimplemented calls abort, CPU code unchanged.')
