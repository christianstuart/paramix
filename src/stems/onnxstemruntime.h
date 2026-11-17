#pragma once

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace mixxx::stems::onnx {

/// Bounds the inference pool so the engine, GUI and waveform keep their cores.
inline int boundedThreadCount() {
    const int hardwareThreads = static_cast<int>(std::thread::hardware_concurrency());
    return std::max(1, std::min(4, hardwareThreads / 2));
}

inline Ort::SessionOptions makeSessionOptions(int threads) {
    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.SetExecutionMode(ORT_SEQUENTIAL);
    options.SetInterOpNumThreads(1);
    options.SetIntraOpNumThreads(threads);
    return options;
}

inline bool isGpuProvider(const std::string& provider) {
    return provider != "CPUExecutionProvider" &&
            provider.find("Azure") == std::string::npos;
}

inline std::string providerNameFor(const std::string& device) {
    if (device == "cuda") {
        return "CUDAExecutionProvider";
    }
    if (device == "tensorrt" || device == "trt") {
        return "TensorrtExecutionProvider";
    }
    if (device == "dml" || device == "directml") {
        return "DmlExecutionProvider";
    }
    if (device == "coreml") {
        return "CoreMLExecutionProvider";
    }
    if (device == "rocm") {
        return "ROCMExecutionProvider";
    }
    if (device == "webgpu") {
        return "WebGpuExecutionProvider";
    }
    return device;
}

inline std::string providerLabel(const std::string& provider) {
    if (provider == "CUDAExecutionProvider") {
        return "cuda";
    }
    if (provider == "TensorrtExecutionProvider") {
        return "tensorrt";
    }
    if (provider == "DmlExecutionProvider") {
        return "dml";
    }
    if (provider == "CoreMLExecutionProvider") {
        return "coreml";
    }
    if (provider == "ROCMExecutionProvider") {
        return "rocm";
    }
    if (provider == "WebGpuExecutionProvider") {
        return "webgpu";
    }
    if (provider == "CPUExecutionProvider") {
        return "cpu";
    }
    return provider;
}

/// Opens a session with the requested provider. `auto` walks the providers
/// this runtime actually reports, preferring the platform's natural GPU API,
/// and falls back to CPU when none initialises.
inline std::unique_ptr<Ort::Session> openSession(Ort::Env& env,
        const std::string& modelPath,
        int threads,
        const std::string& requestedDevice,
        std::string* pChosenDevice) {
    auto open = [&](Ort::SessionOptions options) {
#ifdef _WIN32
        const std::wstring widePath(modelPath.begin(), modelPath.end());
        return std::make_unique<Ort::Session>(env, widePath.c_str(), options);
#else
        return std::make_unique<Ort::Session>(env, modelPath.c_str(), options);
#endif
    };
    if (requestedDevice == "cpu") {
        *pChosenDevice = "cpu";
        return open(makeSessionOptions(threads));
    }

    std::vector<std::string> candidates;
    if (requestedDevice == "auto") {
        const std::vector<std::string> available = Ort::GetAvailableProviders();
        std::vector<std::string> preferred;
#if defined(_WIN32)
        preferred = {"DmlExecutionProvider",
                "CUDAExecutionProvider",
                "TensorrtExecutionProvider",
                "ROCMExecutionProvider",
                "WebGpuExecutionProvider"};
#elif defined(__APPLE__)
        preferred = {"CoreMLExecutionProvider",
                "WebGpuExecutionProvider",
                "CUDAExecutionProvider",
                "ROCMExecutionProvider"};
#elif defined(__linux__)
        preferred = {"CUDAExecutionProvider",
                "ROCMExecutionProvider",
                "TensorrtExecutionProvider",
                "WebGpuExecutionProvider"};
#endif
        for (const std::string& provider : preferred) {
            if (std::find(available.begin(), available.end(), provider) !=
                    available.end()) {
                candidates.push_back(provider);
            }
        }
        for (const std::string& provider : available) {
            if (isGpuProvider(provider) &&
                    std::find(candidates.begin(), candidates.end(), provider) ==
                            candidates.end()) {
                candidates.push_back(provider);
            }
        }
    } else {
        candidates.push_back(providerNameFor(requestedDevice));
    }

    for (const std::string& provider : candidates) {
        try {
            Ort::SessionOptions options = makeSessionOptions(threads);
            if (provider == "DmlExecutionProvider") {
                // DirectML's graph fusion rejects the quantized SCNet graphs;
                // DirectML also requires memory-pattern planning off.
                options.DisableMemPattern();
                options.AddConfigEntry("ep.dml.disable_graph_fusion", "1");
                options.AppendExecutionProvider(provider, {});
            } else {
                options.AppendExecutionProvider(provider, {});
            }
            *pChosenDevice = providerLabel(provider);
            return open(std::move(options));
        } catch (const std::exception&) {
            // Try the next accelerator, then CPU.
        }
    }
    *pChosenDevice = "cpu";
    return open(makeSessionOptions(threads));
}

} // namespace mixxx::stems::onnx
