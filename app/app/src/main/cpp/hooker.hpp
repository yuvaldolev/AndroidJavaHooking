#pragma once

#include <jni.h>

#include <cinttypes>

#include "math.hpp"
#include "trampoline_generator.hpp"

class Hooker {
public:
    // Should be `round_up_to_pointer_size(4 * 4 + 2 * 2) + sizeof(void*)`
    // for Android versions below 12.
    static constexpr uint32_t ENTRYPOINT_FROM_QUICK_COMPILED_CODE_OFFSET =
        round_up_to_pointer_size(4 * 3 + 2 * 2) + sizeof(void*);
    static constexpr uint32_t ACCESS_FLAGS_OFFSET = 4;
    static constexpr uint32_t ACCESS_FLAG_COMPILE_DONT_BOTHER = 0x02000000;
    static constexpr uint32_t ACCESS_FLAG_FAST_INTERPRETER_TO_INTERPRETER_INVOKE = 0x40000000;
    // Should be `0x00200000` for Android versions below 12.
    static constexpr uint32_t ACCESS_FLAG_PRE_COMPILED = 0x00800000;

    Hooker(JNIEnv *env);

    auto hook(
            JNIEnv *env,
            jobject target,
            jobject hook,
            jobject backup = nullptr
    ) -> void;

private:
    static auto get_art_method_field(JNIEnv *env) -> jfieldID;

    static auto set_non_compilable(void *method) -> void;

    static auto get_access_flags(void *method) -> uint32_t;

    static auto set_access_flags(void *method, uint32_t new_flags) -> void;

    auto get_art_method(JNIEnv *env, jobject method) const -> void*;

    auto backup_and_hook(void *target, void *hook, void *backup) -> void;

    auto replace_method(void *from, void *to, bool backup = false) -> void;

    jfieldID m_art_method_field; 
    TrampolineGenerator m_trampoline_generator;
};
