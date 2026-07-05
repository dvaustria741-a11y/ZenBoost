# DobbyPatch.cmake - applied after Dobby clone via PATCH_COMMAND
# cmake -DDOBBY_SRC=<SOURCE_DIR> -P cmake/DobbyPatch.cmake

# ── Fix 1 (armeabi-v7a): Cpu.h was deleted in 2023 refactor but
#    registers-arm.h still includes it. ───────────────────────────────────────
file(WRITE "${DOBBY_SRC}/source/core/arch/Cpu.h"
    "#pragma once\n#include \CpuRegister.h\\n")

# ── Fix 2 (arm64-v8a): closure_bridge_arm64.asm loads the address of the
#    external symbol common_closure_bridge_handler using Apple Mach-O
#    @PAGE/@PAGEOFF relocations.  For Android (Linux ELF) shared libraries the
#    correct form is a GOT-indirect load (adrp :got: / ldr :got_lo12:) which
#    generates R_AARCH64_ADR_GOT_PAGE + R_AARCH64_LD64_GOT_LO12_NC —
#    relocations the dynamic linker accepts in a shared library.
#    The bare adrp/add pair (R_AARCH64_ADR_PREL_PG_HI21 / ADD_ABS_LO12_NC)
#    is rejected by ld.lld for external symbols in shared objects. ─────────────
set(ASM "${DOBBY_SRC}/source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.asm")
file(READ "${ASM}" SRC)
string(REPLACE
    "adrp TMP_REG_0, cdecl(common_closure_bridge_handler)@PAGE\nadd TMP_REG_0, TMP_REG_0, cdecl(common_closure_bridge_handler)@PAGEOFF"
    "adrp TMP_REG_0, :got:common_closure_bridge_handler\nldr TMP_REG_0, [TMP_REG_0, :got_lo12:common_closure_bridge_handler]"
    SRC "${SRC}")
file(WRITE "${ASM}" "${SRC}")

# ── Fix 3: os_arch_features.h — OSMemory used before it is declared due to
#    circular includes.  Replace with direct mprotect call. ───────────────────
set(ARCH_H "${DOBBY_SRC}/common/os_arch_features.h")
file(READ "${ARCH_H}" SRC)
string(REPLACE
    "#include <sys/types.h>"
    "#include <sys/types.h>\n#include <sys/mman.h>\n#include <unistd.h>"
    SRC "${SRC}")
string(REPLACE
    "  auto page = (void *)ALIGN_FLOOR(address, OSMemory::PageSize());\n  if (!OSMemory::SetPermission(page, OSMemory::PageSize(), kReadExecute)) {\n    return;\n  }"
    "  long _page_sz = ::sysconf(_SC_PAGESIZE);\n  auto page = (void *)ALIGN_FLOOR(address, _page_sz);\n  ::mprotect(page, (size_t)_page_sz, PROT_READ | PROT_EXEC);"
    SRC "${SRC}")
file(WRITE "${ARCH_H}" "${SRC}")

# ── Fix 4: RuntimeModule::load_address renamed to ::base in ProcessRuntime.h;
#    ProcessRuntime.cc never updated.  Also fix MemRange::start() called
#    without () — clang 18 (NDK r27b) no longer accepts the bare reference. ───
set(PRT "${DOBBY_SRC}/source/Backend/UserMode/PlatformUtil/Linux/ProcessRuntime.cc")
file(READ "${PRT}" SRC)
string(REPLACE "module.load_address" "module.base" SRC "${SRC}")
string(REPLACE "(a.start < b.start)" "(a.start() < b.start())" SRC "${SRC}")
file(WRITE "${PRT}" "${SRC}")

# ── Fix 5: Same load_address -> base drift in SymbolResolver. ────────────────
set(SR "${DOBBY_SRC}/builtin-plugin/SymbolResolver/elf/dobby_symbol_resolver.cc")
file(READ "${SR}" SRC)
string(REPLACE "load_address" "base" SRC "${SRC}")
file(WRITE "${SR}" "${SRC}")

message(STATUS "[DobbyPatch] applied: Cpu.h stub, GOT-indirect ASM, OSMemory mprotect, load_address->base (x2), start()")
