// JNI shim exposing bionic's mallopt(M_PURGE_ALL) to React Native.
//
// Why: on Android, scudo retains free'd pages in its arena rather
// than returning them to the kernel. After a sequence of large
// allocations and frees (e.g. loading and releasing model weights),
// the process's RSS stays high even though we hold no live data —
// enough that lowmemorykiller can reap us before the next allocation
// completes. M_PURGE_ALL madvise(MADV_DONTNEED)s fully-free pages,
// converting "freed by us, hoarded by scudo" into "returned to OS".
//
// Why dlsym: minSdkVersion is 24, but `mallopt` itself lands at
// API 26, M_PURGE at API 28, and M_PURGE_ALL at API 34. Linking
// against `mallopt` directly would break the loader on API 24/25,
// so we resolve the symbol at runtime and silently no-op on devices
// that don't expose it. We pull the option constants from
// <malloc.h> rather than hand-rolling them — the values have changed
// across NDK releases (M_PURGE_ALL was -102 in early bionic) and
// drifting from the canonical header silently degrades the call.

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <malloc.h>
#include <cstdint>
#include <sstream>
#include <string>

typedef int (*mallopt_fn_t)(int, int);

// The `appmodules` target compile flags include -DLOG_TAG="ReactNative",
// which would clash if we redefined plain `LOG_TAG` here. Distinct name
// to avoid -Wmacro-redefined.
#define HW_INFO_LOG_TAG "PocketPalHardwareInfo"
#define HW_INFO_LOGI(...) \
    __android_log_print(ANDROID_LOG_INFO, HW_INFO_LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pocketpal_HardwareInfoModule_nativePurgeAll(JNIEnv* /* env */, jobject /* this */) {
    static mallopt_fn_t mallopt_fn =
        reinterpret_cast<mallopt_fn_t>(dlsym(RTLD_DEFAULT, "mallopt"));
    if (mallopt_fn == nullptr) {
        HW_INFO_LOGI("mallopt unavailable on this device");
        return JNI_FALSE;
    }
    // Bionic mallopt returns 1 on success, 0 on unknown option.
    // Try the aggressive variant first; fall back to M_PURGE on
    // devices that ship the symbol but not the M_PURGE_ALL option.
    int rc = mallopt_fn(M_PURGE_ALL, 0);
    if (rc == 0) {
        rc = mallopt_fn(M_PURGE, 0);
        HW_INFO_LOGI("M_PURGE_ALL unsupported, fell back to M_PURGE rc=%d", rc);
    } else {
        HW_INFO_LOGI("M_PURGE_ALL rc=%d", rc);
    }
    return rc == 1 ? JNI_TRUE : JNI_FALSE;
}


// NNAPI device inventory is available from Android 10 (API 29). Resolve the
// public NDK entry points at runtime so this app's minSdk (24) does not acquire
// hard loader dependencies on APIs absent from older Android releases.
struct ANeuralNetworksDevice;
using nnapi_get_device_count_fn = int32_t (*)(uint32_t*);
using nnapi_get_device_fn = int32_t (*)(uint32_t, ANeuralNetworksDevice**);
using nnapi_device_get_name_fn = int32_t (*)(const ANeuralNetworksDevice*, const char**);
using nnapi_device_get_type_fn = int32_t (*)(const ANeuralNetworksDevice*, int32_t*);
using nnapi_device_get_version_fn = int32_t (*)(const ANeuralNetworksDevice*, const char**);
using nnapi_device_get_feature_level_fn = int32_t (*)(const ANeuralNetworksDevice*, int64_t*);

static std::string nnapi_json_escape(const char* value) {
    std::string out = "\\\"";
    const std::string input = value == nullptr ? "" : value;
    for (unsigned char c : input) {
        switch (c) {
            case '\\"': out += "\\\\\""; break;
            case '\\\\': out += "\\\\\\"; break;
            case '\\b': out += "\\\\b"; break;
            case '\\f': out += "\\\\f"; break;
            case '\\n': out += "\\\\n"; break;
            case '\\r': out += "\\\\r"; break;
            case '\\t': out += "\\\\t"; break;
            default:
                if (c < 0x20) {
                    static const char hex[] = "0123456789abcdef";
                    out += "\\\\u00";
                    out += hex[(c >> 4) & 0x0f];
                    out += hex[c & 0x0f];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += "\\\"";
    return out;
}

static const char* nnapi_device_type_name(int32_t type) {
    switch (type) {
        case 1: return "CPU";
        case 2: return "GPU";
        case 3: return "ACCELERATOR";
        default: return "OTHER";
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_pocketpal_HardwareInfoModule_nativeGetNnapiInfo(JNIEnv* env, jobject /* this */) {
    void* nnapi = dlopen("libneuralnetworks.so", RTLD_NOW | RTLD_LOCAL);
    if (nnapi == nullptr) {
        const char* error = dlerror();
        std::ostringstream json;
        json << "{\\\"available\\\":false,\\\"error\\\":"
             << nnapi_json_escape(error == nullptr ? "Unable to load libneuralnetworks.so" : error)
             << ",\\\"devices\\\":[]}";
        return env->NewStringUTF(json.str().c_str());
    }

    const auto getDeviceCount = reinterpret_cast<nnapi_get_device_count_fn>(
        dlsym(nnapi, "ANeuralNetworks_getDeviceCount"));
    const auto getDevice = reinterpret_cast<nnapi_get_device_fn>(
        dlsym(nnapi, "ANeuralNetworks_getDevice"));
    const auto getName = reinterpret_cast<nnapi_device_get_name_fn>(
        dlsym(nnapi, "ANeuralNetworksDevice_getName"));
    const auto getType = reinterpret_cast<nnapi_device_get_type_fn>(
        dlsym(nnapi, "ANeuralNetworksDevice_getType"));
    const auto getVersion = reinterpret_cast<nnapi_device_get_version_fn>(
        dlsym(nnapi, "ANeuralNetworksDevice_getVersion"));
    const auto getFeatureLevel = reinterpret_cast<nnapi_device_get_feature_level_fn>(
        dlsym(nnapi, "ANeuralNetworksDevice_getFeatureLevel"));

    if (getDeviceCount == nullptr || getDevice == nullptr ||
        getName == nullptr || getType == nullptr) {
        dlclose(nnapi);
        return env->NewStringUTF(
            "{\\\"available\\\":false,\\\"error\\\":"
            "\\\"NNAPI device-discovery symbols are unavailable\\\",\\\"devices\\\":[]}");
    }

    uint32_t deviceCount = 0;
    const int32_t countStatus = getDeviceCount(&deviceCount);
    if (countStatus != 0) {
        dlclose(nnapi);
        std::ostringstream json;
        json << "{\\\"available\\\":false,\\\"error\\\":"
             << nnapi_json_escape(("ANeuralNetworks_getDeviceCount failed: " +
                                   std::to_string(countStatus)).c_str())
             << ",\\\"devices\\\":[]}";
        return env->NewStringUTF(json.str().c_str());
    }

    // Defensive cap: device inventories are small; never trust a vendor
    // implementation to return an unbounded count.
    if (deviceCount > 64) deviceCount = 64;

    std::ostringstream json;
    json << "{\\\"available\\\":true,\\\"devices\\\":[";
    bool first = true;

    for (uint32_t index = 0; index < deviceCount; ++index) {
        ANeuralNetworksDevice* device = nullptr;
        if (getDevice(index, &device) != 0 || device == nullptr) continue;

        const char* name = nullptr;
        int32_t type = 0;
        if (getName(device, &name) != 0 || getType(device, &type) != 0) continue;

        const char* version = nullptr;
        if (getVersion != nullptr) getVersion(device, &version);

        int64_t featureLevel = -1;
        if (getFeatureLevel != nullptr) getFeatureLevel(device, &featureLevel);

        if (!first) json << ",";
        first = false;
        json << "{\\\"name\\\":" << nnapi_json_escape(name)
             << ",\\\"type\\\":" << nnapi_json_escape(nnapi_device_type_name(type))
             << ",\\\"version\\\":" << nnapi_json_escape(version)
             << ",\\\"featureLevel\\\":" << featureLevel << "}";
    }

    json << "]}";
    dlclose(nnapi);
    return env->NewStringUTF(json.str().c_str());
}
