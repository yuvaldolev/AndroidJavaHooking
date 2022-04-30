#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <jni.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <thread>

#include "payload_dex_data.hpp"

#define LOG_TAG "jni_test"
#define LOG(format, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, format, ##__VA_ARGS__)

#define PAYLOAD_CLASS "com/example/payload/Payload"
#define PAYLOAD_METHOD "start"

using JniGetCreatedJavaVms = auto(JavaVM **, jsize, jsize *) -> jint;
using AndroidGetExportedNamespace = auto(const char *) -> android_namespace_t *;

static auto hook(JNIEnv *env, jclass clazz) {
    LOG("Hooking!");
}

static auto inject_payload_class(
        JNIEnv *env,
        jobject class_loader,
        jobject payload_dex,
        const char *payload_class_name) -> jclass {
    // Create an array with a single item which contains the payload DEX byte buffer.
    auto byte_buffer_class = env->FindClass("java/nio/ByteBuffer");
    auto payload_dex_array = env->NewObjectArray(1, byte_buffer_class, payload_dex);

    // Create a BaseDexClassLoader instance.
    // new BaseDexClassLoader(payload_dex, class_loader)
    auto base_dex_class_loader_class = env->FindClass("dalvik/system/BaseDexClassLoader");
    auto base_dex_class_loader_ctor = env->GetMethodID(base_dex_class_loader_class, "<init>",
                                                   "([Ljava/nio/ByteBuffer;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    auto base_dex_class_loader = env->NewObject(base_dex_class_loader_class, base_dex_class_loader_ctor, payload_dex_array, nullptr, class_loader);

    // Load the payload class using the created BaseDexClassLoader.
    // base_dex_class_loader.loadClass(payload_class)
    auto load_class_method = env->GetMethodID(base_dex_class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    auto payload_class_name_jstring = env->NewStringUTF(payload_class_name);
    auto payload_class = static_cast<jclass>(env->CallObjectMethod(base_dex_class_loader, load_class_method, payload_class_name_jstring));
    env->DeleteLocalRef(payload_class_name_jstring);

    return payload_class;
}

static auto set_context_class_loader(JNIEnv *env, jobject class_loader) -> void {
    // Thread.currentThread().setContextClassLoader(class_loader)
    auto thread_class = env->FindClass("java/lang/Thread");
    auto get_current_thread_method = env->GetStaticMethodID(thread_class, "currentThread", "()Ljava/lang/Thread;");
    auto current_thread = env->CallStaticObjectMethod(thread_class, get_current_thread_method);
    auto set_context_class_loader_method = env->GetMethodID(thread_class, "setContextClassLoader", "(Ljava/lang/ClassLoader;)V");
    env->CallVoidMethod(current_thread, set_context_class_loader_method, class_loader);
}

static auto get_com_android_art_namespace() -> android_namespace_t* {
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

    return android_get_exported_namespace("com_android_art");
}

static auto inject(JavaVM *jvm, jobject class_loader) -> void {
    // Wait for the application to start.
    LOG("Waiting for the application to start...");
    sleep(5);

    // Attach the current thread to the JVM.
    LOG("Attaching the injector thread to the JVM");
    JNIEnv *env;
    jvm->AttachCurrentThread(&env, nullptr);

    // Set the default class loader for this thread to the one set for the main thread.
    LOG("Setting the context class loader");
    set_context_class_loader(env, class_loader);

    // Load the payload DEX to a ByteBuffer.
    LOG("Loading the payload DEX");
    auto payload_dex_byte_buffer = env->NewDirectByteBuffer(payload_dex_data, payload_dex_data_len);

    // Load the payload class from the payload DEX.
    LOG("Injecting the payload class");
    auto payload_class = inject_payload_class(env, class_loader, payload_dex_byte_buffer,
                                              PAYLOAD_CLASS);

    // Register the native methods.
    LOG("Registering native methods");
    JNINativeMethod native_methods[] = {
        { "hook", "()V", reinterpret_cast<void*>(hook) },
    };
    env->RegisterNatives(payload_class, native_methods, 1);

    // Execute the payload method.
    LOG("Executing the payload method");
    auto payload_method = env->GetStaticMethodID(payload_class, PAYLOAD_METHOD, "()V");
    env->CallStaticVoidMethod(payload_class, payload_method);

    // Cleanup.
    LOG("Cleaning up post injection");
    env->DeleteLocalRef(payload_dex_byte_buffer);
    env->DeleteGlobalRef(class_loader);
    jvm->DetachCurrentThread();
}

static auto get_context_class_loader(JNIEnv *env) -> jobject {
    // Thread.currentThread().getContextClassLoader()
    auto thread_class = env->FindClass("java/lang/Thread");
    auto get_current_thread_method = env->GetStaticMethodID(thread_class, "currentThread", "()Ljava/lang/Thread;");
    auto current_thread = env->CallStaticObjectMethod(thread_class, get_current_thread_method);
    auto get_context_class_loader_method = env->GetMethodID(thread_class, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    return env->CallObjectMethod(current_thread, get_context_class_loader_method);
}

static auto get_jvm() -> JavaVM* {
    auto *com_android_art_namespace = get_com_android_art_namespace();
    LOG("com_android_art namespace: [%p]", com_android_art_namespace);

    android_dlextinfo dlext_info = {
            .flags = ANDROID_DLEXT_USE_NAMESPACE,
            .library_namespace = com_android_art_namespace,
    };

    // Get the Java VMs.
    auto *libart_handle = android_dlopen_ext(
            "/apex/com.android.art/lib64/libart.so",
            RTLD_LAZY,
            &dlext_info
    );
    if (nullptr == libart_handle) {
        LOG("Failed loading libart.so: [%s]", dlerror());
        return nullptr;
    }

    LOG("Loaded libart.so at: [%p]", libart_handle);

    auto *jni_get_created_java_vms = reinterpret_cast<JniGetCreatedJavaVms*>(
            dlsym(libart_handle, "JNI_GetCreatedJavaVMs")
    );
    if (nullptr == jni_get_created_java_vms) {
        LOG("Failed retrieving JNI_GetCreatedJavaVMs: [%s]", dlerror());
        dlclose(libart_handle);
        return nullptr;
    }

    LOG("Retrieved JNI_GetCreatedJavaVMs from libart.so at: [%p]", jni_get_created_java_vms);

    JavaVM *jvm;
    jsize n_vms;
    jni_get_created_java_vms(&jvm, 1, &n_vms);
    LOG("Got (%d) Java VMs", n_vms);

    dlclose(libart_handle);

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

    // Start a new thread to perform the Java code injection.
    LOG("Starting the injector thread");
    auto thread = std::thread{ inject, jvm, class_loader };
    thread.detach();
}
