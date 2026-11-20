"""Execute ARM runtime/raster regression checks with a local CPU emulator.

No Vita OS is emulated; installation and on-device performance remain hardware
checks. ELF and symbol parsing here only support our own unstripped ARM build.
"""
from pathlib import Path
import os
import struct
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / ".tools/test-python"))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC

data = (root / os.environ.get("FNAF_CHECK_ELF","build/native_checks.elf")).read_bytes()
assert data[:7] == b"\x7fELF\x01\x01\x01"
phoff, shoff = struct.unpack_from("<II", data, 28)
phsize, phcount, shsize, shcount = struct.unpack_from("<4H", data, 42)
segments = []
for i in range(phcount):
    kind, offset, address, _, file_size, memory_size, _, _ = struct.unpack_from("<8I", data, phoff + i * phsize)
    if kind == 1:
        segments.append((address, offset, file_size, memory_size))
sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shcount)]
symbols = {}
for section in sections:
    if section[1] != 2:
        continue
    strings_section = sections[section[6]]
    strings = data[strings_section[4]:strings_section[4] + strings_section[5]]
    for position in range(section[4], section[4] + section[5], section[9]):
        name, address, size, info, other, index = struct.unpack_from("<IIIBBH", data, position)
        end = strings.find(b"\0", name)
        symbols[strings[name:end].decode("ascii", "replace")] = address
machine = Uc(UC_ARCH_ARM, UC_MODE_ARM)
start = min(item[0] for item in segments) & ~4095
end = (max(item[0] + item[3] for item in segments) + 4095) & ~4095
machine.mem_map(start, end - start)
for address, offset, file_size, memory_size in segments:
    machine.mem_write(address, data[offset:offset + file_size])
machine.mem_map(0x08000000, 2 * 1024 * 1024)
machine.mem_map(0x0F000000, 4096)
machine.reg_write(UC_ARM_REG_SP, 0x081FFF00)
machine.reg_write(UC_ARM_REG_LR, 0x0F000000)
machine.reg_write(UC_ARM_REG_C1_C0_2, 0xF << 20)
machine.reg_write(UC_ARM_REG_FPEXC, 1 << 30)
machine.emu_start(symbols["run_checks"], 0x0F000000, timeout=60000000, count=500000000)
if machine.reg_read(UC_ARM_REG_PC) != 0x0F000000:
    raise RuntimeError("ARM checks timed out before returning")
result = machine.reg_read(UC_ARM_REG_R0)
if result:
    raise AssertionError(f"Actual ARM runtime check {result} failed")
if 'game_audio_checks' in os.environ.get('FNAF_CHECK_ELF',''):
    print('20 game audio ARM checks passed: RAM preload, no file I/O during mixing/looping, memory callback bounds, channel 23, volume, pause, mute and cleanup (mock WAV input/OS, no device output).')
elif 'asset_checks' in os.environ.get('FNAF_CHECK_ELF',''):
    print('Asset reader ARM checks passed: foreground priority, obsolete-job cancellation/demotion, L8 conversion, ownership, malformed files, bounded staging, eviction and cleanup (mock I/O; no device latency measurement).')
elif 'renderer_checks' in os.environ.get('FNAF_CHECK_ELF',''):
    print('Renderer ARM checks passed: shared sprite pages, crop UVs with/without perspective, page ownership/eviction protection, asynchronous misses, fences and bounded cache (mock GPU; no device FPS measurement).')
elif 'game_capture' in os.environ.get('FNAF_CHECK_ELF',''):
    print('124 Android UI ARM checks passed: first-ready camera image, neighbor warming, resident noise, interpolated doors without double images, office preload and prior power-out/touch checks (no Vita OS/GPU emulation).')
else:
    print("Native ARM regression checks passed: legacy runtime/audio plus game clock, AI attacks, power, all night endings, options and save validation (no Vita OS/GPU/audio-device emulation).")
