#include <dlfcn.h>
#include <jni.h>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ANeuralNetworksModel;
struct ANeuralNetworksCompilation;
struct ANeuralNetworksExecution;
struct ANeuralNetworksDevice;
struct ANeuralNetworksOperandType {
    int32_t type;
    uint32_t dimensionCount;
    const uint32_t* dimensions;
    float scale;
    int32_t zeroPoint;
};

constexpr int32_t kNnapiOk = 0;
constexpr int32_t kDeviceAccelerator = 4;
constexpr int32_t kTensorFloat32 = 3;
constexpr int32_t kInt32 = 1;
constexpr int32_t kAdd = 0;
constexpr int32_t kFusedNone = 0;

using GetDeviceCount = int32_t (*)(uint32_t*);
using GetDevice = int32_t (*)(uint32_t, ANeuralNetworksDevice**);
using DeviceGetName = int32_t (*)(const ANeuralNetworksDevice*, const char**);
using DeviceGetType = int32_t (*)(const ANeuralNetworksDevice*, int32_t*);
using ModelCreate = int32_t (*)(ANeuralNetworksModel**);
using ModelFree = void (*)(ANeuralNetworksModel*);
using ModelAddOperand = int32_t (*)(ANeuralNetworksModel*, const ANeuralNetworksOperandType*);
using ModelSetOperandValue = int32_t (*)(ANeuralNetworksModel*, int32_t, const void*, size_t);
using ModelAddOperation = int32_t (*)(ANeuralNetworksModel*, int32_t, uint32_t, const uint32_t*, uint32_t, const uint32_t*);
using ModelIdentifyInputsAndOutputs = int32_t (*)(ANeuralNetworksModel*, uint32_t, const uint32_t*, uint32_t, const uint32_t*);
using ModelFinish = int32_t (*)(ANeuralNetworksModel*);
using CompilationCreateForDevices = int32_t (*)(ANeuralNetworksModel*, const ANeuralNetworksDevice* const*, uint32_t, ANeuralNetworksCompilation**);
using CompilationFinish = int32_t (*)(ANeuralNetworksCompilation*);
using CompilationFree = void (*)(ANeuralNetworksCompilation*);
using ExecutionCreate = int32_t (*)(ANeuralNetworksCompilation*, ANeuralNetworksExecution**);
using ExecutionSetInput = int32_t (*)(ANeuralNetworksExecution*, int32_t, const ANeuralNetworksOperandType*, const void*, size_t);
using ExecutionSetOutput = int32_t (*)(ANeuralNetworksExecution*, int32_t, const ANeuralNetworksOperandType*, void*, size_t);
using ExecutionCompute = int32_t (*)(ANeuralNetworksExecution*);
using ExecutionFree = void (*)(ANeuralNetworksExecution*);

std::string jsonEscape(const char* value) {
    std::string out = "\"";
    for (const unsigned char c : std::string(value == nullptr ? "" : value)) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(c >> 4) & 0xf];
                    out += hex[c & 0xf];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += "\"";
    return out;
}

template <typename T>
T loadSymbol(void* library, const char* name) {
    return reinterpret_cast<T>(dlsym(library, name));
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_pocketpal_HardwareInfoModule_nativeRunNnapiSmokeTest(JNIEnv* env, jobject) {
    void* library = dlopen("libneuralnetworks.so", RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        const char* error = dlerror();
        const std::string result =
            "{\"available\":false,\"executionVerified\":false,\"error\":" +
            jsonEscape(error == nullptr ? "Unable to load libneuralnetworks.so" : error) + "}";
        return env->NewStringUTF(result.c_str());
    }

#define LOAD_NNAPI(name, type) const auto name = loadSymbol<type>(library, #name)
    LOAD_NNAPI(ANeuralNetworks_getDeviceCount, GetDeviceCount);
    LOAD_NNAPI(ANeuralNetworks_getDevice, GetDevice);
    LOAD_NNAPI(ANeuralNetworksDevice_getName, DeviceGetName);
    LOAD_NNAPI(ANeuralNetworksDevice_getType, DeviceGetType);
    LOAD_NNAPI(ANeuralNetworksModel_create, ModelCreate);
    LOAD_NNAPI(ANeuralNetworksModel_free, ModelFree);
    LOAD_NNAPI(ANeuralNetworksModel_addOperand, ModelAddOperand);
    LOAD_NNAPI(ANeuralNetworksModel_setOperandValue, ModelSetOperandValue);
    LOAD_NNAPI(ANeuralNetworksModel_addOperation, ModelAddOperation);
    LOAD_NNAPI(ANeuralNetworksModel_identifyInputsAndOutputs, ModelIdentifyInputsAndOutputs);
    LOAD_NNAPI(ANeuralNetworksModel_finish, ModelFinish);
    LOAD_NNAPI(ANeuralNetworksCompilation_createForDevices, CompilationCreateForDevices);
    LOAD_NNAPI(ANeuralNetworksCompilation_finish, CompilationFinish);
    LOAD_NNAPI(ANeuralNetworksCompilation_free, CompilationFree);
    LOAD_NNAPI(ANeuralNetworksExecution_create, ExecutionCreate);
    LOAD_NNAPI(ANeuralNetworksExecution_setInput, ExecutionSetInput);
    LOAD_NNAPI(ANeuralNetworksExecution_setOutput, ExecutionSetOutput);
    LOAD_NNAPI(ANeuralNetworksExecution_compute, ExecutionCompute);
    LOAD_NNAPI(ANeuralNetworksExecution_free, ExecutionFree);
#undef LOAD_NNAPI

    if (ANeuralNetworks_getDeviceCount == nullptr ||
        ANeuralNetworks_getDevice == nullptr ||
        ANeuralNetworksDevice_getName == nullptr ||
        ANeuralNetworksDevice_getType == nullptr ||
        ANeuralNetworksModel_create == nullptr ||
        ANeuralNetworksModel_free == nullptr ||
        ANeuralNetworksModel_addOperand == nullptr ||
        ANeuralNetworksModel_setOperandValue == nullptr ||
        ANeuralNetworksModel_addOperation == nullptr ||
        ANeuralNetworksModel_identifyInputsAndOutputs == nullptr ||
        ANeuralNetworksModel_finish == nullptr ||
        ANeuralNetworksCompilation_createForDevices == nullptr ||
        ANeuralNetworksCompilation_finish == nullptr ||
        ANeuralNetworksCompilation_free == nullptr ||
        ANeuralNetworksExecution_create == nullptr ||
        ANeuralNetworksExecution_setInput == nullptr ||
        ANeuralNetworksExecution_setOutput == nullptr ||
        ANeuralNetworksExecution_compute == nullptr ||
        ANeuralNetworksExecution_free == nullptr) {
        dlclose(library);
        return env->NewStringUTF(
            "{\"available\":false,\"executionVerified\":false,"
            "\"error\":\"Required NNAPI execution symbols are unavailable\"}");
    }

    uint32_t deviceCount = 0;
    int32_t status = ANeuralNetworks_getDeviceCount(&deviceCount);
    if (status != kNnapiOk) {
        dlclose(library);
        const std::string result =
            "{\"available\":false,\"executionVerified\":false,\"status\":" +
            std::to_string(status) + "}";
        return env->NewStringUTF(result.c_str());
    }

    std::vector<std::pair<ANeuralNetworksDevice*, std::string>> candidates;
    for (uint32_t i = 0; i < deviceCount && i < 64; ++i) {
        ANeuralNetworksDevice* device = nullptr;
        const char* name = nullptr;
        int32_t type = kDeviceUnknown;
        if (ANeuralNetworks_getDevice(i, &device) != kNnapiOk ||
            device == nullptr ||
            ANeuralNetworksDevice_getName(device, &name) != kNnapiOk ||
            ANeuralNetworksDevice_getType(device, &type) != kNnapiOk ||
            type == 2 || type == 3) {
            continue;
        }
        std::string lowered = name == nullptr ? "" : name;
        for (char& c : lowered) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (lowered.find("qualcomm") != std::string::npos ||
            lowered.find("qti") != std::string::npos ||
            lowered.find("hexagon") != std::string::npos) {
            candidates.emplace_back(device, name == nullptr ? "Unknown" : name);
        }
    }

    if (candidates.empty()) {
        dlclose(library);
        return env->NewStringUTF(
            "{\"available\":true,\"executionVerified\":false,"
            "\"error\":\"No Qualcomm NNAPI accelerator device was exposed by this ROM\"}");
    }

    std::string lastError = "No candidate accelerator completed the NNAPI test";
    for (const auto& candidate : candidates) {
        ANeuralNetworksModel* model = nullptr;
        ANeuralNetworksCompilation* compilation = nullptr;
        ANeuralNetworksExecution* execution = nullptr;
        int32_t resultCode = kNnapiOk;

        resultCode = ANeuralNetworksModel_create(&model);
        const uint32_t dimensions[] = {4};
        const ANeuralNetworksOperandType tensorType = {
            kTensorFloat32, 1, dimensions, 0.0f, 0
        };
        const ANeuralNetworksOperandType scalarType = {
            kInt32, 0, nullptr, 0.0f, 0
        };
        const int32_t activation = kFusedNone;

        if (resultCode == kNnapiOk) resultCode = ANeuralNetworksModel_addOperand(model, &tensorType);
        if (resultCode == kNnapiOk) resultCode = ANeuralNetworksModel_addOperand(model, &tensorType);
        if (resultCode == kNnapiOk) resultCode = ANeuralNetworksModel_addOperand(model, &scalarType);
        if (resultCode == kNnapiOk) resultCode = ANeuralNetworksModel_addOperand(model, &tensorType);
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksModel_setOperandValue(model, 2, &activation, sizeof(activation));
        }

        const uint32_t operationInputs[] = {0, 1, 2};
        const uint32_t operationOutputs[] = {3};
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksModel_addOperation(
                model, kAdd, 3, operationInputs, 1, operationOutputs);
        }
        const uint32_t inputIndexes[] = {0, 1};
        const uint32_t outputIndexes[] = {3};
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksModel_identifyInputsAndOutputs(
                model, 2, inputIndexes, 1, outputIndexes);
        }
        if (resultCode == kNnapiOk) resultCode = ANeuralNetworksModel_finish(model);

        const ANeuralNetworksDevice* selectedDevices[] = {candidate.first};
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksCompilation_createForDevices(
                model, selectedDevices, 1, &compilation);
        }
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksCompilation_finish(compilation);
        }
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksExecution_create(compilation, &execution);
        }

        const std::array<float, 4> left = {1.0f, 2.0f, 3.0f, 4.0f};
        const std::array<float, 4> right = {3.5f, 4.5f, 5.5f, 6.5f};
        std::array<float, 4> output = {};
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksExecution_setInput(
                execution, 0, nullptr, left.data(), sizeof(left));
        }
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksExecution_setInput(
                execution, 1, nullptr, right.data(), sizeof(right));
        }
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksExecution_setOutput(
                execution, 0, nullptr, output.data(), sizeof(output));
        }
        if (resultCode == kNnapiOk) {
            resultCode = ANeuralNetworksExecution_compute(execution);
        }

        bool matches = resultCode == kNnapiOk;
        const std::array<float, 4> expected = {4.5f, 6.5f, 8.5f, 10.5f};
        for (size_t i = 0; i < output.size() && matches; ++i) {
            matches = std::fabs(output[i] - expected[i]) < 0.0001f;
        }

        if (execution != nullptr) ANeuralNetworksExecution_free(execution);
        if (compilation != nullptr) ANeuralNetworksCompilation_free(compilation);
        if (model != nullptr) ANeuralNetworksModel_free(model);

        if (matches) {
            dlclose(library);
            const std::string result =
                "{\"available\":true,\"executionVerified\":true,\"outputMatched\":true,\"deviceName\":" +
                jsonEscape(candidate.second.c_str()) + ",\"operation\":\"FLOAT32_ADD\"}";
            return env->NewStringUTF(result.c_str());
        }

        lastError = "NNAPI accelerator compilation/execution failed with status " +
                    std::to_string(resultCode);
        if (resultCode == kNnapiOk) {
            lastError = "Accelerator returned an incorrect result for FLOAT32_ADD";
        }
    }

    dlclose(library);
    const std::string result =
        "{\"available\":true,\"executionVerified\":false,\"error\":" +
        jsonEscape(lastError.c_str()) + "}";
    return env->NewStringUTF(result.c_str());
}
