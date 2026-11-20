"""Build fixed, release-specific Forceware display policy probes for an i386 Linux guest."""
import hashlib
from pathlib import Path
import subprocess
import pefile

ROOT = Path(__file__).resolve().parent
RELEASE_MINIPORT = "375884957ab5b4e72ae56b9feb1bc1865c30cf2ab1df80b2407cc0ded5ffc4f5"
LIMITS_VA = 0xd6a9eb
LIMITS_SIZE = 52


def build(upstream, miniport, dest):
    src = upstream / "sources"
    commit = subprocess.check_output(["git", "-C", upstream, "rev-parse", "HEAD"], text=True).strip()
    if commit != "91ad42bc4fab7d710ce6861e044620fe63d28d38":
        raise ValueError("Expected Forceware release 382.69-2026-11-09.")
    if hashlib.sha256(miniport.read_bytes()).hexdigest() != RELEASE_MINIPORT:
        raise ValueError("The input does not match the published November 9 miniport.")
    gcc = ["gcc", "-m32", "-Os", "-ffreestanding", "-fno-builtin", "-fno-pie", "-fno-pic",
           "-fno-stack-protector", "-fno-asynchronous-unwind-tables", "-mpreferred-stack-boundary=2",
           "-nostdlib", "-static", "-no-pie", "-Wl,--build-id=none"]
    subprocess.run(["as", "--32", src / "gp107/chip-limits.s", "-o", "limits.o"], cwd=dest, check=True)
    subprocess.run(["ld", "-m", "elf_i386", "-T", src / "gp107/chip-limits.ld", "limits.o", "-o", "limits.elf"], cwd=dest, check=True)
    subprocess.run(["objcopy", "-O", "binary", "-j", ".text", "limits.elf", "limits.bin"], cwd=dest, check=True)
    symbols = subprocess.check_output(["nm", "-n", "limits.elf"], cwd=dest, text=True)
    addresses = {line.split()[-1]: int(line.split()[0], 16) for line in symbols.splitlines() if line and len(line.split()) == 3}
    if addresses["gp107_chip_limit"] != LIMITS_VA:
        raise ValueError("Unexpected GP107 entry point.")
    offset = LIMITS_VA - addresses["gp107_register_finish"]
    assembled = (dest / "limits.bin").read_bytes()[offset:]
    with pefile.PE(str(miniport), fast_load=True) as pe:
        released = pe.get_data(LIMITS_VA - pe.OPTIONAL_HEADER.ImageBase, LIMITS_SIZE)
    if len(assembled) != LIMITS_SIZE or assembled != released:
        raise ValueError("The GP107 limit function and its table differ from the released machine code.")
    # Only this verified pure function is included. The other routines in
    # chip-limits.s depend on a running Windows kernel and a GPU.
    (dest / "limitfn.bin").write_bytes(assembled)
    subprocess.run(["objcopy", "-I", "binary", "-O", "elf32-i386", "-B", "i386", "--rename-section",
                    ".data=.limitfn,alloc,load,readonly,code,contents", "limitfn.bin", "limitfn.o"], cwd=dest, check=True)
    subprocess.run([*gcc, "-Wl,-z,noexecstack,--section-start=.limitfn=0xd6a9eb,--defsym=gp107_chip_limit=0xd6a9eb",
                    ROOT / "gp107_limits.c", "limitfn.o", "-o", "gp107"], cwd=dest, check=True)
    tests = {
        "dp-trained": ["dp-trained/test-policy.c", "dp-trained/dp-policy.c"],
        "dp-startup": ["dp-startup/test-policy.c", "dp-startup/query-policy.c", "dp-trained/dp-policy.c"],
        "hdmi-recovery": ["hdmi-recovery/recovery-test.c"],
    }
    for name, files in tests.items():
        subprocess.run([*gcc, *(src / path for path in files), "-o", dest / name], check=True)
    return ["gp107", *tests]
