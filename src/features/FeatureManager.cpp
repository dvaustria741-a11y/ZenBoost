#include "FeatureManager.h"
#include "../../include/ZenBoost.h"
#include <sys/mman.h>
#include <GLES2/gl2ext.h>

// malloc_trim exists in Android bionic (API 26+) but is absent from the NDK's
// libc link stubs, causing an "undefined symbol" linker error.
// Declaring it weak lets the linker succeed; the null check guards the call at
// runtime in case the device somehow doesn't have it.
extern "C" __attribute__((weak)) int malloc_trim(size_t pad);

static std::vector<std::shared_ptr<Feature>> features;

namespace FeatureManager {

// ── memory helpers ────────────────────────────────────────────────────────────
static uint64_t getMemUsageKB() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            uint64_t kb = 0;
            sscanf(line.c_str(), "VmRSS: %llu kB", &kb);
            return kb;
        }
    }
    return 0;
}

static void doMemClean() {
    if (malloc_trim) malloc_trim(0);
    // Release unused anonymous mappings
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("anon") != std::string::npos) {
            uintptr_t start, end;
            if (sscanf(line.c_str(), "%lx-%lx", &start, &end) == 2) {
                madvise(reinterpret_cast<void*>(start), end - start, MADV_DONTNEED);
            }
        }
    }
    LOGI("[MemClean] Done. RSS now: %llu KB", getMemUsageKB());
}

// ── feature definitions ───────────────────────────────────────────────────────
void init() {
    features.clear();

    // 1. FPS Uncap — controlled in EGLHook via isEnabled("fps_uncap")
    {
        auto f = std::make_shared<Feature>("fps_uncap", "FPS Uncap",
            "Removes the 60 FPS cap by patching eglSwapInterval to 0.");
        features.push_back(f);
    }

    // 2. VSync — off by default; enabling re-enables VSync cap
    {
        auto f = std::make_shared<Feature>("vsync", "VSync",
            "Forces eglSwapInterval(1) to re-enable vertical sync.");
        features.push_back(f);
    }

    // 3. Memory Cleaner
    {
        auto f = std::make_shared<Feature>("mem_clean", "Memory Cleaner",
            "Runs malloc_trim + MADV_DONTNEED every 30 s to reduce RAM usage.");
        f->hasTick = true;
        static int memTick = 0;
        f->onTick = [f]() {
            if (!f->enabled) return;
            if (++memTick >= 1800) {   // ~30 s at 60 fps
                doMemClean();
                memTick = 0;
            }
        };
        features.push_back(f);
    }

    // 4. Thread Priority Boost
    {
        auto f = std::make_shared<Feature>("thread_boost", "Thread Boost",
            "Raises the render thread priority (nice -10) for smoother frames.");
        f->onEnable  = []() {
            int r = setpriority(PRIO_PROCESS, 0, -10);
            LOGI("[ThreadBoost] setpriority -> %d", r);
        };
        f->onDisable = []() { setpriority(PRIO_PROCESS, 0, 0); };
        features.push_back(f);
    }

    // 5. Frame Limiter (target 120 FPS)
    {
        auto f = std::make_shared<Feature>("frame_limit", "120 FPS Limit",
            "Caps rendering to exactly 120 FPS using a sleep-based limiter.");
        features.push_back(f);
    }

    // 6. Boost Mode (meta-toggle: enables FPS Uncap + Mem Cleaner + Thread Boost)
    {
        auto f = std::make_shared<Feature>("boost_mode", "Boost Mode",
            "One-click: enables FPS Uncap, Memory Cleaner, and Thread Priority Boost.");
        f->onEnable  = []() {
            setEnabled("fps_uncap",   true);
            setEnabled("mem_clean",   true);
            setEnabled("thread_boost",true);
        };
        f->onDisable = []() {
            setEnabled("fps_uncap",   false);
            setEnabled("mem_clean",   false);
            setEnabled("thread_boost",false);
        };
        features.push_back(f);
    }

    // 7. HUD Overlay — show FPS + memory in-game; controlled in ZenGui
    {
        auto f = std::make_shared<Feature>("hud", "HUD Overlay",
            "Shows live FPS, frame time, and memory usage on screen.");
        f->enabled = true;  // on by default
        features.push_back(f);
    }

    // 8. Low-End Mode — reduce GLES call overhead
    {
        auto f = std::make_shared<Feature>("low_end", "Low-End Mode",
            "Disables dithering, depth testing for UI, and sets low GL hint flags.");
        f->onEnable  = []() {
            glDisable(GL_DITHER);
            glHint(GL_GENERATE_MIPMAP_HINT,        GL_FASTEST);
            glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES, GL_FASTEST);
        };
        f->onDisable = []() {
            glEnable(GL_DITHER);
            glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
        };
        features.push_back(f);
    }

    LOGI("[FeatureManager] %zu features registered.", features.size());
}

void tick() {
    for (auto& f : features)
        if (f->hasTick && f->enabled && f->onTick) f->onTick();
}

int count() { return (int)features.size(); }

std::vector<std::shared_ptr<Feature>>& getAll() { return features; }

std::shared_ptr<Feature> get(const std::string& id) {
    for (auto& f : features) if (f->id == id) return f;
    return nullptr;
}

bool isEnabled(const std::string& id) {
    auto f = get(id); return f && f->enabled;
}

void setEnabled(const std::string& id, bool on) {
    auto f = get(id);
    if (!f || f->enabled == on) return;
    f->enabled = on;
    if (on  && f->onEnable ) f->onEnable();
    if (!on && f->onDisable) f->onDisable();
}

void toggle(const std::string& id) {
    setEnabled(id, !isEnabled(id));
}

} // namespace FeatureManager
