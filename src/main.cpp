#include "../include/ZenBoost.h"
#include "hooks/EGLHook.h"
#include "hooks/InputHook.h"
#include "features/FeatureManager.h"

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;

    LOGI("============================");
    LOGI("  ZenBoost v" ZB_VERSION " loading");
    LOGI("  Minecraft Bedrock 26.30.5 ");
    LOGI("============================");

    FeatureManager::init();
    EGLHook::install();
    InputHook::install();

    LOGI("[ZenBoost] Ready — %d features, GUI on FAB (top-right).",
         FeatureManager::count());
    return JNI_VERSION_1_6;
}

// Some loaders call _init instead of / in addition to JNI_OnLoad
extern "C" __attribute__((constructor))
void zb_constructor() {
    LOGI("[ZenBoost] constructor() called — waiting for JNI_OnLoad.");
}