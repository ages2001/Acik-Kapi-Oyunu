"""Check the observed LFENCE adapter's register/flags contract in x86 emulation.

This checks the trampoline, not CPU memory-ordering semantics or other FP32 code.
"""
from pathlib import Path
import json
import random
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EFLAGS, UC_X86_REG_EIP,
)

root = Path(__file__).resolve().parents[2]
manifest = json.loads((root/'fp32/reports/lfence-probe.json').read_text())
pe = pefile.PE(str(root/'fp32/build/FP32LF.EXE'))
base = pe.OPTIONAL_HEADER.ImageBase
uc = Uc(UC_ARCH_X86, UC_MODE_32)
uc.mem_map(base, pe.OPTIONAL_HEADER.SizeOfImage)
uc.mem_write(base, pe.get_memory_mapped_image())
uc.mem_map(0x20000000, 0x1000)
uc.mem_map(0x30000000, 0x10000)
rng = random.Random(32)
regs = [UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
        UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
        UC_X86_REG_EFLAGS]
for trial in range(512):
    expected = {reg: rng.getrandbits(32) for reg in regs}
    expected[UC_X86_REG_EAX] = 0x20000000
    expected[UC_X86_REG_ESP] = 0x30008000
    expected[UC_X86_REG_EFLAGS] = 2 | (rng.getrandbits(12) & 0xcd5)
    for reg, value in expected.items():
        uc.reg_write(reg, value)
    value = rng.getrandbits(32)
    uc.mem_write(0x20000004, value.to_bytes(4, 'little'))
    uc.emu_start(int(manifest['site'], 16), int(manifest['return_va'], 16), count=32)
    expected[UC_X86_REG_EAX] = value
    assert uc.reg_read(UC_X86_REG_EIP) == int(manifest['return_va'], 16)
    for reg, value in expected.items():
        assert uc.reg_read(reg) == value, (trial, reg, hex(value), hex(uc.reg_read(reg)))
result = {'cases': 512, 'passed': 512, 'image_sha256': manifest['output_sha256'],
          'scope': 'Trampoline register/flags/stack/displaced-load checks only; not full FP32 or memory-ordering verification.'}
(root/'fp32/reports/lfence-trampoline-test.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
