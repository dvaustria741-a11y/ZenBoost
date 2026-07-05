#include "InputHook.h"
#include "../../include/ZenBoost.h"
#include <android/input.h>
#include <dobby.h>
#include <atomic>

namespace InputHook {

using fn_getEvent = int32_t (*)(AInputQueue*, AInputEvent**);
static fn_getEvent orig_getEvent = nullptr;

static std::atomic<float> s_tx{0}, s_ty{0};
static std::atomic<bool>  s_down{false};

static int32_t hook_getEvent(AInputQueue* queue, AInputEvent** event) {
    int32_t result = orig_getEvent(queue, event);
    if (result >= 0 && *event) {
        if (AInputEvent_getType(*event) == AINPUT_EVENT_TYPE_MOTION) {
            int32_t action = AMotionEvent_getAction(*event) & AMOTION_EVENT_ACTION_MASK;
            s_tx   = AMotionEvent_getX(*event, 0);
            s_ty   = AMotionEvent_getY(*event, 0);
            s_down = (action == AMOTION_EVENT_ACTION_DOWN ||
                      action == AMOTION_EVENT_ACTION_MOVE);
        }
    }
    return result;
}

void install() {
    void* libAndroid = dlopen("libandroid.so", RTLD_NOW);
    if (!libAndroid) { LOGE("dlopen libandroid.so: %s", dlerror()); return; }

    void* sym = dlsym(libAndroid, "AInputQueue_getEvent");
    if (!sym) { LOGE("AInputQueue_getEvent not found"); return; }

    DobbyHook(sym, (void*)hook_getEvent, (void**)&orig_getEvent);
    LOGI("[InputHook] Installed — touch events intercepted.");
}

float getLastTouchX() { return s_tx.load(); }
float getLastTouchY() { return s_ty.load(); }
bool  getTouchDown()  { return s_down.load(); }

} // namespace InputHook