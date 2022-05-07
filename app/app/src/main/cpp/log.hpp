#pragma once

#include <android/log.h>

#define LOG_TAG "jni_test"
#define LOG(format, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, format, ##__VA_ARGS__)
