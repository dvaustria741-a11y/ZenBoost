#pragma once

#include <jni.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <malloc.h>
#include <sys/resource.h>
#include <pthread.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <memory>
#include <chrono>
#include <thread>
#include <sstream>
#include <fstream>

#define ZB_TAG  "ZenBoost"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  ZB_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, ZB_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, ZB_TAG, __VA_ARGS__)

#define ZB_VERSION "1.0.0"