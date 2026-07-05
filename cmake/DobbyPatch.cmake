# DobbyPatch.cmake - run via PATCH_COMMAND after Dobby is cloned
# Called as: cmake -DDOBBY_SRC=<SOURCE_DIR> -P cmake/DobbyPatch.cmake

# Fix 1: registers-arm.h includes "core/arch/Cpu.h" which was deleted from
# the Dobby repo in 2023. Create a stub that forwards to CpuRegister.h.
file(WRITE "${DOBBY_SRC}/source/core/arch/Cpu.h"
"#pragma once\n#include \"CpuRegister.h\"\n")

# Fix 2: closure_bridge_arm64.asm uses Apple Mach-O @PAGE/@PAGEOFF relocations.
# Android NDK clang assembles for Linux/ELF and rejects these.
# Replace with ELF-compatible :lo12: syntax.
set(ASM "${DOBBY_SRC}/source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.asm")
file(READ "${ASM}" SRC)
string(REPLACE ")@PAGE" ")" SRC "${SRC}")
string(REPLACE "common_closure_bridge_handler)@PAGEOFF"
               ":lo12:cdecl(common_closure_bridge_handler)" SRC "${SRC}")
file(WRITE "${ASM}" "${SRC}")

message(STATUS "[DobbyPatch] Applied armeabi-v7a Cpu.h stub and arm64 ELF ASM fix")
