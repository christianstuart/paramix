#pragma once

#include <string>

#include "preferences/usersettings.h"

namespace mixxx::stems {

/// `[Stems],GpuAcceleration`: ask the separator for a GPU execution provider.
/// Stored as `1`/`0`; when the key is absent the default comes from
/// gpuAccelerationDefault(), so a machine that has never written the setting
/// still gets the right answer.
constexpr char kGpuAccelerationKey[] = "GpuAcceleration";

/// True when the machine has a hardware GPU that is worth using for inference.
///
/// The check is deliberately conservative, because the alternative is a
/// default that silently moves stem separation onto a slower device:
/// a discrete adapter with its own video memory on Windows, an AMD or NVIDIA
/// accelerator on an Intel Mac, an Apple Silicon GPU (unified memory, but still
/// a real GPU), and a non-Intel DRM card with dedicated VRAM on Linux.
/// Software renderers, Intel integrated graphics and CPU-only machines return
/// false, so acceleration stays opt-in there.
bool hasDedicatedGpu();

/// True when the linked ONNX Runtime exposes a GPU execution provider. False in
/// builds without ONNX Runtime. This answers "could a GPU be used at all?",
/// not "is this machine's GPU suitable?" — that is hasDedicatedGpu().
bool hasGpuInferenceProvider();

/// Default for a machine that has never written `[Stems],GpuAcceleration`:
/// acceleration on only when there is both a suitable GPU and a runtime
/// provider that can drive it.
bool gpuAccelerationDefault();

/// Reads `[Stems],GpuAcceleration`, falling back to gpuAccelerationDefault().
bool isGpuAccelerationEnabled(UserSettingsPointer pConfig);

/// Writes `[Stems],GpuAcceleration`; removes the key when the value equals the
/// detected default so a future hardware change can pick the better default.
void setGpuAccelerationEnabled(UserSettingsPointer pConfig, bool enabled);

/// Execution provider to request from DemucsOnnxSeparator: `auto` when GPU
/// acceleration is wanted, otherwise `cpu`. `auto` resolves against the
/// runtime's actual provider list and degrades to CPU when setup fails.
std::string stemDeviceForGpuAcceleration(bool enabled);

} // namespace mixxx::stems
