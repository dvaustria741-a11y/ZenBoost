#include "EGLHook.h"
#include "../../include/ZenBoost.h"
#include "../features/FeatureManager.h"
#include "../gui/ZenGui.h"
#include <dobby.h>
#include <chrono>
#include <thread>

namespace EGLHook {

using fn_SwapInterval = EGLBoolean (*)(EGLDisplay, EGLint);
using fn_SwapBuffers  = EGLBoolean (*)(EGLDisplay, EGLSurface);

static fn_SwapInterval orig_SwapInterval = nullptr;
static fn_SwapBuffers  orig_SwapBuffers  = nullptr;

static float s_fps         = 0.0f;
static float s_frameTimeMs = 0.0f;

static std::chrono::steady_clock::time_point s_lastSwap;
static bool s_guiReady = false;

// ── eglSwapInterval hook ──────────────────────────────────────────────────────
static EGLBoolean hook_SwapInterval(EGLDisplay dpy, EGLint interval) {
    // FPS Uncap overrides any interval the game sets
    if (FeatureManager::isEnabled("fps_uncap") &&
        !FeatureManager::isEnabled("vsync"))
        interval = 0;
    else if (FeatureManager::isEnabled("vsync"))
        interval = 1;
    return orig_SwapInterval(dpy, interval);
}

// ── eglSwapBuffers hook ───────────────────────────────────────────────────────
static EGLBoolean hook_SwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // ── frame timing ─────────────────────────────────────────────────────────
    auto now = std::chrono::steady_clock::now();
    s_frameTimeMs = std::chrono::duration<float, std::milli>(now - s_lastSwap).count();
    if (s_frameTimeMs > 0.0f) s_fps = 1000.0f / s_frameTimeMs;
    s_lastSwap = now;

    // ── 120 FPS limiter ───────────────────────────────────────────────────────
    if (FeatureManager::isEnabled("frame_limit")) {
        constexpr float TARGET_MS = 1000.0f / 120.0f;
        float remaining = TARGET_MS - s_frameTimeMs;
        if (remaining > 0.5f)
            std::this_thread::sleep_for(
                std::chrono::duration<float, std::milli>(remaining));
    }

    // ── feature tick ─────────────────────────────────────────────────────────
    FeatureManager::tick();

    // ── GUI overlay ──────────────────────────────────────────────────────────
    if (!s_guiReady) {
        ZenGui::init();
        s_guiReady = true;
    }
    ZenGui::render(s_fps, s_frameTimeMs);

    return orig_SwapBuffers(dpy, surface);
}

void install() {
    void* libEGL = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (!libEGL) { LOGE("dlopen libEGL.so failed: %s", dlerror()); return; }

    void* symInterval = dlsym(libEGL, "eglSwapInterval");
    void* symBuffers  = dlsym(libEGL, "eglSwapBuffers");
    if (!symInterval || !symBuffers) {
        LOGE("dlsym EGL symbols missing"); return;
    }

    DobbyHook(symInterval, (void*)hook_SwapInterval, (void**)&orig_SwapInterval);
    DobbyHook(symBuffers,  (void*)hook_SwapBuffers,  (void**)&orig_SwapBuffers);

    s_lastSwap = std::chrono::steady_clock::now();
    LOGI("[EGLHook] Installed — eglSwapInterval + eglSwapBuffers hooked.");
}

float getFPS()         { return s_fps;         }
float getFrameTimeMs() { return s_frameTimeMs; }

} // namespace EGLHook