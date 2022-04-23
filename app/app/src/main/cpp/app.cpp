#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <sys/mman.h>
#include <unistd.h>

#include <thread>
#include <asm-generic/fcntl.h>

#include "payload_dex_data.hpp"

#define LOG_TAG "jni_test"
#define LOG(format, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, format, ##__VA_ARGS__)

#define PAYLOAD_CLASS "com/example/payload/Payload"
#define PAYLOAD_METHOD "start"

using JniGetCreatedJavaVms = auto(JavaVM **, jsize, jsize *) -> jint;
using AndroidGetExportedNamespace = auto(const char *) -> android_namespace_t *;

static auto write_all(int fd, const void *data, size_t size) -> int {
    auto *cursor = reinterpret_cast<const uint8_t*>(data);
    size_t bytes_remaining = size;

    while (0 < bytes_remaining) {
        ssize_t bytes_written = write(fd, cursor, bytes_remaining);
        if (-1 == bytes_written) {
            if (EINTR == errno) {
                // We were interrupted while writing, continue.
                continue;
            }

            // Write failed.
            return 1;
        }

        cursor += bytes_written;
        bytes_remaining -= bytes_written;
    }

    return 0;
}

static auto load_payload_class(
        JNIEnv *env,
        jobject class_loader,
        jobject payload_dex,
        const char *payload_class_name) -> jclass {
    // Create a PathClassLoader instance.
    // new PathClassLoader(payload_dex, class_loader)
    auto base_dex_class_loader_class = env->FindClass("dalvik/system/BaseDexClassLoader");
    auto base_dex_class_loader_ctor = env->GetMethodID(base_dex_class_loader_class, "<init>",
                                                   "([Ljava/nio/ByteBuffer;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    auto byte_buffer_class = env->FindClass("java/nio/ByteBuffer");
    auto payload_dex_array = env->NewObjectArray(1, byte_buffer_class, payload_dex);
    auto base_dex_class_loader = env->NewObject(base_dex_class_loader_class, base_dex_class_loader_ctor, payload_dex_array, nullptr, class_loader);

    // Load the payload class using the created PathClassLoader.
    // base_dex_class_loader.loadClass(payload_class)
    auto load_class_method = env->GetMethodID(base_dex_class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    auto payload_class_name_jstring = env->NewStringUTF(payload_class_name);
    auto payload_class = static_cast<jclass>(env->CallObjectMethod(base_dex_class_loader, load_class_method, payload_class_name_jstring));
    env->DeleteLocalRef(payload_class_name_jstring);

    return payload_class;
}

static auto set_context_class_loader(JNIEnv *env, jobject class_loader) -> void {
    // Thread.currentThread().setContextClassLoader(class_loader)
    auto thread = env->FindClass("java/lang/Thread");
    auto get_current_thread = env->GetStaticMethodID(thread, "currentThread", "()Ljava/lang/Thread;");
    auto current_thread = env->CallStaticObjectMethod(thread, get_current_thread);
    auto set_context_class_loader_method = env->GetMethodID(thread, "setContextClassLoader", "(Ljava/lang/ClassLoader;)V");
    env->CallVoidMethod(current_thread, set_context_class_loader_method, class_loader);
}

static auto get_default_namespace() -> android_namespace_t* {
#if defined(__aarch64__)
    static constexpr size_t ANDROID_GET_EXPORTED_NAMESPACE_OFFSET = 0x64bf0;
#elif defined(__x86_64__)
    static constexpr size_t ANDROID_GET_EXPORTED_NAMESPACE_OFFSET = 0x6b580;
#else
# error Unsupported architecture
#endif

    auto *android_get_device_api_level = reinterpret_cast<uint8_t*>(
            dlsym(RTLD_DEFAULT, "android_get_device_api_level")
    );
    if (nullptr == android_get_device_api_level) {
        LOG("Failed retrieving android_get_device_api_level: [%s]", dlerror());
        return nullptr;
    }

    auto *android_get_exported_namespace = reinterpret_cast<AndroidGetExportedNamespace*>(
            android_get_device_api_level + ANDROID_GET_EXPORTED_NAMESPACE_OFFSET
    );

    return android_get_exported_namespace("default");
}

static auto inject(JavaVM *jvm, jobject class_loader) -> void {
    // Wait for the application to start.
    sleep(5);

    // Attach the current thread to the JVM.
    JNIEnv *env;
    jvm->AttachCurrentThread(&env, nullptr);

    // Set the default class loader for this thread to the one set for the main thread.
    LOG("Setting the context class loader");
    set_context_class_loader(env, class_loader);

    // Load the payload DEX to a ByteBuffer.
    LOG("Loading the payload DEX");
    auto payload_dex_byte_buffer = env->NewDirectByteBuffer(payload_dex_data, payload_dex_data_len);

    // Load the payload class from the payload DEX.
    LOG("Loading the payload class");
    auto payload_class = load_payload_class(env, class_loader, payload_dex_byte_buffer, PAYLOAD_CLASS);

    // Execute the payload method.
    LOG("Executing the payload method");
    auto payload_method = env->GetStaticMethodID(payload_class, PAYLOAD_METHOD, "()V");
    env->CallStaticVoidMethod(payload_class, payload_method);

    // Cleanup.
    env->DeleteGlobalRef(class_loader);
    jvm->DetachCurrentThread();
}

static auto get_context_class_loader(JNIEnv *env) -> jobject {
    // Thread.currentThread().getContextClassLoader()
    auto thread = env->FindClass("java/lang/Thread");
    auto get_current_thread = env->GetStaticMethodID(thread, "currentThread", "()Ljava/lang/Thread;");
    auto current_thread = env->CallStaticObjectMethod(thread, get_current_thread);
    auto get_context_class_loader_method = env->GetMethodID(thread, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    return env->CallObjectMethod(current_thread, get_context_class_loader_method);
}

static auto get_jvm() -> JavaVM* {
    auto *default_namespace = get_default_namespace();
    LOG("default namespace: [%p]", default_namespace);

    android_dlextinfo dlext_info = {
            .flags = ANDROID_DLEXT_USE_NAMESPACE,
            .library_namespace = default_namespace,
    };

    // Get the Java VMs.
    auto *libandroid_runtime_handle = android_dlopen_ext(
            "/system/lib64/libandroid_runtime.so",
            RTLD_LAZY,
            &dlext_info
    );
    if (nullptr == libandroid_runtime_handle) {
        LOG("Failed loading libandroid_runtime.so: [%s]", dlerror());
        return nullptr;
    }

    LOG("Loaded libandroid_runtime.so at: [%p]", libandroid_runtime_handle);

    auto *jni_get_created_java_vms = reinterpret_cast<JniGetCreatedJavaVms*>(
            dlsym(libandroid_runtime_handle, "JNI_GetCreatedJavaVMs")
    );
    if (nullptr == jni_get_created_java_vms) {
        LOG("Failed retrieving JNI_GetCreatedJavaVMs: [%s]", dlerror());
        dlclose(libandroid_runtime_handle);
        return nullptr;
    }

    LOG("Found JNI_GetCreatedJavaVMs at: [%p]", jni_get_created_java_vms);

    JavaVM *jvm;
    jsize n_vms;
    jni_get_created_java_vms(&jvm, 1, &n_vms);
    LOG("Got (%d) Java VMs", n_vms);

    dlclose(libandroid_runtime_handle);

    return jvm;
}

static auto __attribute__ ((constructor)) setup() -> void {
    LOG("SO is running: pid=(%d)", getpid());

    // Retrieve the JVM.
    auto *jvm = get_jvm();
    if (nullptr == jvm) {
        return;
    }

    // Attach the current thread to the JVM.
    JNIEnv *env;
    jvm->AttachCurrentThread(&env, nullptr);

    // Retrieve the class loader.
    LOG("Getting the context class loader");
    auto class_loader = env->NewGlobalRef(get_context_class_loader(env));

    auto thread = std::thread{ inject, jvm, class_loader };
    thread.detach();
}