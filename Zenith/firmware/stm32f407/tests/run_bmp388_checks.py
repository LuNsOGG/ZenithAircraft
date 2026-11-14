"""Run actual driver/application ARM C on Unicorn with a mocked sensor.

Requires Python and unicorn==2.1.4. Dependencies may be installed ONLY in
build/checks/python_deps with pip --target; this script never installs packages,
flashes, deletes files, or connects to a debugger. GCC tools are located under
LOCALAPPDATA/stm32cube/bundles, or supplied via --toolchain-bin.
"""
import argparse
from decimal import Decimal, getcontext
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "checks" / "acquisition"
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(ROOT / "build" / "checks" / "python_deps"))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--toolchain-bin", type=Path)
args = parser.parse_args()
if args.toolchain_bin:
    tools = args.toolchain_bin
else:
    candidates = sorted((Path(os.environ["LOCALAPPDATA"]) / "stm32cube/bundles/gnu-tools-for-stm32").glob("*/bin"))
    tools = candidates[-1]

# Independent high-precision polynomial evaluation and calibration scaling.
# Vectors cover all fourteen nonzero coefficients, including signed ones.
getcontext().prec = 60
D = Decimal
q = lambda value, exponent: D(value) / (D(2) ** exponent)
t1, t2, t3 = q(25000, -8), q(20000, 30), q(-5, 48)
c = [q(16500-16384, 20), q(16300-16384, 29), q(7, 32), q(-3, 37),
     q(12500, -3), q(500, 6), q(-12, 8), q(6, 15), q(-900, 48), q(7, 48), q(-4, 65)]
vectors = []
for raw_t in (5000000, 6400000, 7700000, 10000000):
    dt = D(raw_t) - t1
    temp = dt*t2 + dt*dt*t3
    for raw_p in (0, 4000000, 8000000, 16000000):
        p = D(raw_p)
        pressure = c[4] + c[5]*temp + c[6]*temp**2 + c[7]*temp**3
        pressure += p*(c[0] + c[1]*temp + c[2]*temp**2 + c[3]*temp**3)
        pressure += p**2*c[8] + p**2*temp*c[9] + p**3*c[10]
        vectors.append(f"{{{raw_p}U, {raw_t}U, {float(pressure):.17g}, {float(temp):.17g}}}")
(OUT / "golden_vectors.h").write_text(
    "static const struct {uint32_t p,t; double pressure,temperature;} golden[] = {\n" +
    ",\n".join(vectors) + "\n};\n", encoding="ascii")
linker = OUT / "checks.ld"
linker.write_text("""ENTRY(Test_Run)
MEMORY { FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1M
         RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K }
SECTIONS {
 .text : { *(.text*) *(.rodata*) } > FLASH
 .ARM.exidx : { *(.ARM.exidx*) } > FLASH
 .data : { *(.data*) } > RAM AT> FLASH
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
""", encoding="ascii")
elf = OUT / "checks.elf"
subprocess.run([str(tools / "arm-none-eabi-gcc.exe"), "-mcpu=cortex-m3", "-mthumb", "-mfloat-abi=soft",
    "-std=gnu11", "-O0", "-g3", "-Wall", "-Wextra", "-Werror", "-ffunction-sections", "-fdata-sections",
    "-ICore/Inc", f"-I{OUT}", "tests/check_bmp388.c", "Core/Src/bmp388.c", "Core/Src/application.c",
    "-nostdlib", f"-Wl,-T,{linker}", "-Wl,--gc-sections", "-Wl,--start-group", "-lc", "-lm", "-lgcc",
    "-Wl,--end-group", "-o", str(elf)], cwd=ROOT, check=True)
nm = subprocess.check_output([str(tools / "arm-none-eabi-nm.exe"), "-n", str(elf)], text=True)
symbols = {parts[2]: int(parts[0],16) for line in nm.splitlines() if len(parts := line.split()) == 3}

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x08000000, 0x100000)
uc.mem_map(0x20000000, 0x20000)
data = elf.read_bytes()
assert data[:5] == b"\x7fELF\x01", "Expected ELF32"
phoff = struct.unpack_from("<I", data, 28)[0]
phentsize, phnum = struct.unpack_from("<HH", data, 42)
for i in range(phnum):
    kind, offset, vaddr, _, filesz, memsz, _, _ = struct.unpack_from("<8I", data, phoff + i*phentsize)
    if kind == 1 and memsz:
        if filesz:
            uc.mem_write(vaddr, data[offset:offset+filesz])
uc.reg_write(UC_ARM_REG_SP, 0x20020000)
uc.reg_write(UC_ARM_REG_LR, symbols["Test_Done"] | 1)
uc.emu_start(symbols["Test_Run"] | 1, symbols["Test_Done"], count=10000000)
assert uc.reg_read(UC_ARM_REG_PC) == symbols["Test_Done"], "Instruction limit reached before tests completed"
read_word = lambda name: struct.unpack("<I", uc.mem_read(symbols[name], 4))[0]
count, failures, first = [read_word(name) for name in ("test_count", "test_failures", "test_first_failure")]
print(f"ARM software checks: {count} assertions, {failures} failures; first failing C line: {first}")
if failures or count < 80:
    raise SystemExit(1)
