#include <android/dlext.h>
#include <dlfcn.h>
#include <jni.h>
#include <unistd.h>

#include <cstdio>

using JniGetCreatedJavaVms = auto(JavaVM **, jsize, jsize *) -> jint;
using AndroidGetExportedNamespace = auto(const char *) -> android_namespace_t *;

// See https://en.wikipedia.org/wiki/CPUID for a list of x86 cpu features.
// The field names are based on the short name provided in the wikipedia tables.
typedef struct {
    int fpu : 1;
    int tsc : 1;
    int cx8 : 1;
    int clfsh : 1;
    int mmx : 1;
    int aes : 1;
    int erms : 1;
    int f16c : 1;
    int fma4 : 1;
    int fma3 : 1;
    int vaes : 1;
    int vpclmulqdq : 1;
    int bmi1 : 1;
    int hle : 1;
    int bmi2 : 1;
    int rtm : 1;
    int rdseed : 1;
    int clflushopt : 1;
    int clwb : 1;

    int sse : 1;
    int sse2 : 1;
    int sse3 : 1;
    int ssse3 : 1;
    int sse4_1 : 1;
    int sse4_2 : 1;
    int sse4a : 1;

    int avx : 1;
    int avx2 : 1;

    int avx512f : 1;
    int avx512cd : 1;
    int avx512er : 1;
    int avx512pf : 1;
    int avx512bw : 1;
    int avx512dq : 1;
    int avx512vl : 1;
    int avx512ifma : 1;
    int avx512vbmi : 1;
    int avx512vbmi2 : 1;
    int avx512vnni : 1;
    int avx512bitalg : 1;
    int avx512vpopcntdq : 1;
    int avx512_4vnniw : 1;
    int avx512_4vbmi2 : 1;
    int avx512_second_fma : 1;
    int avx512_4fmaps : 1;
    int avx512_bf16 : 1;
    int avx512_vp2intersect : 1;
    int amx_bf16 : 1;
    int amx_tile : 1;
    int amx_int8 : 1;

    int pclmulqdq : 1;
    int smx : 1;
    int sgx : 1;
    int cx16 : 1;  // aka. CMPXCHG16B
    int sha : 1;
    int popcnt : 1;
    int movbe : 1;
    int rdrnd : 1;

    int dca : 1;
    int ss : 1;
    // Make sure to update X86FeaturesEnum below if you add a field here.
} X86Features;

typedef struct {
    X86Features features;
    int family;
    int model;
    int stepping;
    char vendor[13];  // 0 terminated string
} X86Info;

// Calls cpuid and returns an initialized X86info.
// This function is guaranteed to be malloc, memset and memcpy free.
using get_x86_info_t = X86Info(void);

int main() {
    dlopen("/apex/com.android.art/lib64/libartpalette.so", RTLD_NOW);
    dlopen("/apex/com.android.art/lib64/libartbase.so", RTLD_NOW);
    dlopen("/apex/com.android.art/lib64/libartpalette.so", RTLD_NOW);
    dlopen("/apex/com.android.art/lib64/libprofile.so", RTLD_NOW);
    auto *libart_handle = dlopen("/apex/com.android.art/lib64/libart.so", RTLD_NOW);
    if (nullptr == libart_handle) {
        printf("Failed opening libart.so: [%s]\n", dlerror());
        return 1;
    }

    auto *jni_get_created_java_vms = reinterpret_cast<JniGetCreatedJavaVms*>(
            dlsym(libart_handle, "JNI_GetCreatedJavaVMs")
    );
    if (nullptr == jni_get_created_java_vms) {
        printf("Failed retrieving JNI_GetCreatedJavaVMs from libart.so: [%s]\n", dlerror());
        return 1;
    }

    printf("JNI_GetCreatedJavaVMs found at: [%p]\n", jni_get_created_java_vms);

    auto *get_x86_info = reinterpret_cast<get_x86_info_t*>(
            dlsym(libart_handle, "GetX86Info")
    );
    if (nullptr == get_x86_info) {
        printf("Failed retrieving GetX86Info from libart.so: [%s]\n", dlerror());
        return 1;
    }

    get_x86_info();
    printf("Got info!\n");

    JavaVM *jvm = nullptr;
    jsize n_vms;
    jni_get_created_java_vms(&jvm, 1, &n_vms);
    printf("Got (%d) Java VMs\n", n_vms);

    sleep(1000);

    dlclose(libart_handle);
}