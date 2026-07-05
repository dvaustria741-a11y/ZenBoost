# DobbyPatch.cmake - applied after Dobby clone via PATCH_COMMAND
# cmake -DDOBBY_SRC=<SOURCE_DIR> -P cmake/DobbyPatch.cmake

# ── Fix 1 (armeabi-v7a): Cpu.h was deleted in 2023 refactor but
#    registers-arm.h still includes it. Create a stub forwarding to
#    CpuRegister.h (the replacement file). ────────────────────────────────────
file(WRITE "${DOBBY_SRC}/source/core/arch/Cpu.h"
    "#pragma once\n#include \CpuRegister.h\\n")

# ── Fix 2 (arm64-v8a): closure_bridge_arm64.asm uses Apple Mach-O
#    @PAGE/@PAGEOFF relocations.  Android NDK clang targets Linux/ELF and
#    rejects these.  Replace with ELF :lo12: equivalents. ───────────────────
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

# ── Fix 3: os_arch_features.h uses OSMemory inside a header that is included
#    BEFORE platform.h finishes parsing (circular include), so OSMemory is not
#    yet declared.  Replace the android::make_memory_readable body with a
#    direct mprotect call that needs no forward declaration. ─────────────────
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

# ── Fix 4: RuntimeModule::load_address was renamed to ::base in
#    ProcessRuntime.h but the .cc files still reference load_address.
#    Also fix MemRange::start used as field instead of method (clang 18). ────
set(PRT "${DOBBY_SRC}/source/Backend/UserMode/PlatformUtil/Linux/ProcessRuntime.cc")
file(READ "${PRT}" SRC)
string(REPLACE "module.load_address" "module.base" SRC "${SRC}")
string(REPLACE "(a.start < b.start)" "(a.start() < b.start())" SRC "${SRC}")
file(WRITE "${PRT}" "${SRC}")

# ── Fix 5: Same load_address -> base drift in the SymbolResolver plugin. ────
set(SR "${DOBBY_SRC}/builtin-plugin/SymbolResolver/elf/dobby_symbol_resolver.cc")
file(READ "${SR}" SRC)
string(REPLACE "load_address" "base" SRC "${SRC}")
file(WRITE "${SR}" "${SRC}")

message(STATUS "[DobbyPatch] applied: ELF ASM, Cpu.h stub, OSMemory mprotect, load_address->base (ProcessRuntime + SymbolResolver), start()->")
