"""Package the current development work without representing it as a release."""
from pathlib import Path
import hashlib,json,struct,zipfile,shutil,argparse
parser=argparse.ArgumentParser()
parser.add_argument("--revision", default="")
args=parser.parse_args()
if args.revision and not args.revision.isalnum():parser.error("Revision must be alphanumeric")
suffix=("-"+args.revision) if args.revision else ""
root=Path('/workspace/fp10-port');stage=root/('deliverables/FP10WORK'+suffix)
if stage.exists():raise SystemExit('Staging directory already exists; preserve or move it before rebuilding.')
stage.mkdir()
def add(src,dst):
 target=stage/dst;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,target)
readme='''FLASH PLAYER LEGACY PORT - DEVELOPMENT SNAPSHOT, 2026-10-29

THIS IS NOT A COMPLETED PORT OR A FINAL RELEASE.

SETUP/SETUP.EXE is the tested Windows NT 3.1 development installer. It installs
Flash Player 10.3.183.90 and its compatibility DLLs into C:\\FP10, creates player
and uninstall items in Program Manager, and associates .swf files. It rejects
other operating systems in this milestone build. The original Adobe projector
has been modified; its original digital signature is not valid for this build.

RUNTIME contains exactly the payload embedded in this installer, extracted and
checked against its build manifest. Use SETUP.EXE for shell integration.
The audio driver is NOT installed automatically by this development setup.
The supplied original AUDIONT archive is under DRIVERS; its own README describes
installation and the required restart. WINAUDIO is retained for the later
Windows 3.1 / Windows 95 work. Operating system drivers use normal system paths.

Verified on QEMU: NT 3.1, Pentium II with no SSE/SSE2, 64 MB RAM, VGA16, AC97.
Observed: basic AVM1 motion, AS3 graphics, a short FLV1/MP3 video/audio test,
and short Pacman gameplay. The installer baseline passed 14 graphics checks.
The separate current experiment passed 24 graphics checks, including a fix for
pixels written outside complex clipping regions; see the before/after logs.
Registry isolation passed 32 checks. Setup, two shortcuts, SWF association,
uninstall restoration, reboot cleanup, reinstallation, in-use protection and
modified-file protection were exercised. See REPORTS for precise limits.

NOT COMPLETE: Mario interaction/rendering/performance; general SWF compatibility;
all codecs; automatic driver provisioning; remaining OS ports; GPU acceleration.
The Mario menu was observed at about 5.5 screen changes/second before the latest
cache experiment. This is not an exact SWF FPS measurement or acceptable final
performance. Some unavailable API calls still log and terminate with code 120.
No general guarantee of SSE-free execution for every Flash runtime path is made.

EXPERIMENT contains the latest RGB/bitmap-cache runtime experiment. It is NOT the
installer payload and has not completed validation. Keep it separate from the
installed baseline. SOURCE contains current code and generated proxy definitions.
SOURCE/compat/gdi-before-cache.c is the graphics baseline before the current
cache experiment. Building needs the Linux clang/lld/mingw/nasm environment
referenced by the scripts; this archive does not contain that toolchain.

INPUTS contains the original supplied Adobe FP10 and FP32 executables. FP32 has
NOT been ported. The plan remains: finish the NT 3.1 FP10 milestone, then port
FP32 for the requested Windows families. Win32s installation was checked with
FreeCell, not with Flash. Other OS disks and their operating system files are
not bundled here.

TESTS/SWF contains every successfully transferred user SWF, using DOS-compatible
names. The manifest maps these to the original filenames. isaac.swf and smbc.swf
were not transferred because each exceeded the attachment tool's size limit;
they are not present and were not tested. Most supplied games remain untested.
TESTS/generated contains small development fixtures; AS3REF9.SWF is the Ruffle
simple-shapes test with only its SWF version byte adapted from 15 to 9.

REFERENCE/WINSIMD.zip is the original supplied reference, not a prerequisite of
the SSE-free runtime. The earlier WINSIMD threading test did not pass.

All content files are listed with SHA-256 hashes in MANIFEST.json (except that
manifest itself). The ZIP was CRC-tested after creation. The ZIP contains a
complete snapshot of the included runtime/source/test material, not proof that
the requested compatibility work is complete.
'''
(stage/'README.TXT').write_text(readme)
setup=(root/'installer/SETUP.EXE').read_bytes();magic,base,count=struct.unpack_from('<III',setup,len(setup)-12)
assert magic==0x31504b46
expected={x['name']:x for x in json.loads((root/'installer/manifest.json').read_text())}
for i in range(count):
 name,off,size,crc=struct.unpack_from('<16sIII',setup,base+i*28);name=name.rstrip(b'\0').decode();data=setup[base+off:base+off+size]
 assert hashlib.sha256(data).hexdigest()==expected[name]['sha256']
 target=stage/'RUNTIME'/name;target.parent.mkdir(exist_ok=True);target.write_bytes(data)
add(root/'installer/SETUP.EXE','SETUP/SETUP.EXE');add(root/'installer/manifest.json','SETUP/PAYLOAD.JSON')
for p in [root/'compat/FPTRACE.EXE']+[root/'compat'/r['proxy'] for r in json.loads((root/'compat/trace-proxies.json').read_text())]:add(p,'EXPERIMENT/'+p.name)
for directory in ['compat','analysis','bin','src','installer']:
 for p in (root/directory).rglob('*'):
  if p.is_file() and p.suffix.lower() in ['.c','.h','.py','.sh','.asm','.def','.json','.md','.txt','.ico','.rc'] and '__pycache__' not in p.parts:
   add(p,'SOURCE/'+str(p.relative_to(root)))
for p in (root/'reports').glob('*'):
 if p.is_file() and p.suffix.lower() in ['.md','.txt','.json','.wav']:
  add(p,'REPORTS/'+p.name)
for name in ['60-color','64-video','68-action','141-about','182-as3-reference','206-pacman-game','227-installed-shortcuts','241-registry-clean-test','245-clean-install-result','246-clean-associated-player','250-uninstall-retry-result','282-resume','290-gdi-fixed']:
 p=root/'screenshots'/f'{name}.png'
 if p.exists():add(p,'EVIDENCE/'+p.name)
attachments=Path('/workspace/attachments')
for filename,destination in [
 ('flashplayer10_3r183_90_win_sa_debug.exe','INPUTS/FLASH10.EXE'),
 ('flashplayer32_0r0_465_win_sa.exe','INPUTS/FLASH32.EXE'),
 ('AUDIONT-1.1.zip','DRIVERS/AUDIONT.zip'),
 ('winaudio-1.1.zip','DRIVERS/WINAUDIO.zip'),
 ('winsimd.zip','REFERENCE/WINSIMD.zip')]:
 paths=list(attachments.glob('*/'+filename));assert len(paths)==1,(filename,paths);add(paths[0],destination)
for row in json.loads((root/'reports/swf-inputs.json').read_text()):
 paths=list(attachments.glob('*/'+row['file']));assert len(paths)==1
 assert hashlib.sha256(paths[0].read_bytes()).hexdigest()==row['sha256'];add(paths[0],'TESTS/SWF/'+row['iso_name'])
add(root/'reports/swf-inputs.json','TESTS/SWF-MANIFEST.JSON')
for p in (root/'compat').glob('*.SWF'):add(p,'TESTS/generated/'+p.name)
license_root=root/'reference/ruffle'
for p in license_root.glob('LICENSE*'):
 if p.is_file():add(p,'TESTS/reference-licenses/'+p.name)
for p in (license_root/'tests/tests/swfs/avm2/graphics_simple_shapes').glob('*.as'):add(p,'TESTS/reference-source/'+p.name)
add(root/'compat/GDITEST.EXE','TESTS/native/GDITEST.EXE')
entries=[]
for p in sorted(stage.rglob('*')):
 if p.is_file():entries.append({'path':str(p.relative_to(stage)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(stage/'MANIFEST.json').write_text(json.dumps(entries,indent=2))
output=root/('deliverables/FP10-NT31-Development-2026-10-29'+suffix+'.zip')
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=False) as z:
 for p in sorted(stage.rglob('*')):
  if p.is_file():z.write(p,'FP10WORK/'+str(p.relative_to(stage)))
with zipfile.ZipFile(output) as z:assert z.testzip() is None
result={'file':str(output),'bytes':output.stat().st_size,'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'content_files':len(entries),'zip_crc_test':'PASS','installer_payload_matches_manifest':True}
(output.with_suffix('.json')).write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
