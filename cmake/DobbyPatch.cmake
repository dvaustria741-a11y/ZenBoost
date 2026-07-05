# DobbyPatch.cmake - applied after Dobby clone via PATCH_COMMAND
# cmake -DDOBBY_SRC=<SOURCE_DIR> -P cmake/DobbyPatch.cmake

# Fix 1: Cpu.h was deleted in 2023 refactor but registers-arm.h still includes it.
# Create a stub forwarding to CpuRegister.h (the replacement file).
file(WRITE "${DOBBY_SRC}/source/core/arch/Cpu.h"
    "#pragma once\n#include \"CpuRegister.h\"\n")

# Fix 2: closure_bridge_arm64.asm uses Apple Mach-O @PAGE/@PAGEOFF relocations.
# Android NDK clang targets Linux/ELF and rejects these.
# Replace the two offending full lines with ELF :lo12: equivalents.
set(ASM "${DOBBY_SRC}/source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.asm")
file(READ "${ASM}" SRC)
string(REPLACE
    "adrp TMP_REG_0, cdecl(common_closure_bridge_handler)@PAGE"
    "adrp TMP_REG_0, cdecl(common_closure_bridge_handler)"
    SRC "${SRC}")
string(REPLACE
    "add TMP_REG_0, TMP_REG_0, cdecl(common_closure_bridge_handler)@PAGEOFF"
    "add TMP_REG_0, TMP_REG_0, :lo12:cdecl(common_closure_bridge_handler)"
    SRC "${SRC}")
file(WRITE "${ASM}" "${SRC}")

message(STATUS "[DobbyPatch] arm64 ELF ASM fix and armeabi-v7a Cpu.h stub applied")
