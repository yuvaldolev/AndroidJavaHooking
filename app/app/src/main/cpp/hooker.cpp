#include "hooker.hpp"
#include "log.hpp"

Hooker::Hooker(JNIEnv *env) :
    m_art_method_field{ get_art_method_field(env) },
    m_trampoline_factory{ ENTRYPOINT_FROM_QUICK_COMPILED_CODE_OFFSET } {
}

auto Hooker::hook(
        JNIEnv *env,
        jobject target,
        jobject hook,
        jobject backup /* = nullptr */) -> void {
    // Backup the target method and hook it with the hook method.
    LOG("Backing up and hooking method");
    backup_and_hook(
            get_art_method(env, target),
            get_art_method(env, hook),
            get_art_method(env, backup)
    );

    // Keep a global reference to the hook method so that it won't be Garbage Collected.
    LOG("Allocating a global reference to the hook method");
    env->NewGlobalRef(hook);

    // If we where given a backup method, keep a global reference to it as well.
    if (nullptr != backup) {
        LOG("Allocating a global reference to the backup method");
        env->NewGlobalRef(backup);
    }
}

auto Hooker::get_art_method_field(JNIEnv *env) -> jfieldID {
    // Executable.artMethod
    auto executable_class = env->FindClass("java/lang/reflect/Executable");
    return env->GetFieldID(executable_class, "artMethod", "J");
}

auto Hooker::set_non_compilable(void *method) -> void {
    // Retrieve the method's access flags.
    auto access_flags = get_access_flags(method);

    // Set the method to non-compilable.
    access_flags |= ACCESS_FLAG_COMPILE_DONT_BOTHER;
    access_flags &= ~ACCESS_FLAG_PRE_COMPILED;

    // Update the method's access flags.
    set_access_flags(method, access_flags);
}

auto Hooker::get_access_flags(void *method) -> uint32_t {
    return *reinterpret_cast<uint32_t*>(
            reinterpret_cast<uint8_t*>(method) + ACCESS_FLAGS_OFFSET
    );
}

auto Hooker::set_access_flags(void *method, uint32_t new_flags) -> void {
    auto *current_flags = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uint8_t*>(method) + ACCESS_FLAGS_OFFSET
    );
    
    *current_flags = new_flags;
}

auto Hooker::get_art_method(JNIEnv *env, jobject method) const -> void* {
    if (nullptr == method) {
        return nullptr;
    }

    return reinterpret_cast<void*>(env->GetLongField(method, m_art_method_field));
}

auto Hooker::backup_and_hook(void *target, void *hook, void *backup) -> void {
    LOG("Target method is at: [%p], hook method is at: [%p], backup method is at: [%p]",
            target, hook, backup);

    // Set the target method to non-compilable, so that we won't have
    // to worry about `hotness_count_`. 
    LOG("Setting the target method to non-compilable");
    set_non_compilable(target);

    if (nullptr != backup) {
        // Set the backup method to non-compilable.
        LOG("Setting the backup method to non-compilable");
        set_non_compilable(backup);

        // Backup the target method to the backup method if one has been given.
        // We use the same method of hooking the target method for backing it up as well.
        // We hook the backup method and redirect back to the original target method.
        // The only difference is that the entry point is now hardcoded instead of
        // reading it from the `ArtMethod` struct since it will be overriden
        // when we hook the target method.
        LOG("Backing up the target method");
        replace_method(backup, target, true);
    }

    // Hook the target method with the hook method.
    LOG("Hooking the target method");
    replace_method(target, hook);
}

auto Hooker::replace_method(void *from, void *to, bool backup /* = false */) -> void {
    LOG("Replacing method from [%p] to [%p]", from, to);

    // Retrieve the original entrypoint if this is a backup method.
    void *original_entrypoint = nullptr;
    if (backup) {
        // For the backup method, the entrypoint is hardcoded in the `to`
        // method (which is the original target method).
        original_entrypoint = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(to) + ENTRYPOINT_FROM_QUICK_COMPILED_CODE_OFFSET
        );
        LOG("Original target method entrypoint: [%p]", original_entrypoint);
    }

    // Retrieve the new trampoline entrypoint.
    auto *new_entrypoint = m_trampoline_factory.get_trampoline(to, original_entrypoint);

    // Retrieve the `from` method's entrypoint.
    auto **from_entrypoint = reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(from) + ENTRYPOINT_FROM_QUICK_COMPILED_CODE_OFFSET
    );

    // Replace the entrypoint with the trampoline.
    LOG("Replacing the method's entry point from [%p] to [%p]", *from_entrypoint, new_entrypoint);
    *from_entrypoint = new_entrypoint;

    // Make sure the method goes through the `entry_point_from_quick_compiled_code_`,
    // where our trampoline will jump to the hook, instead of using the fast path.
    uint32_t access_flags = get_access_flags(from);
    access_flags &= ~ACCESS_FLAG_FAST_INTERPRETER_TO_INTERPRETER_INVOKE;
    set_access_flags(from, access_flags);
}
