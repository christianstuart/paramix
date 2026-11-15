# Stem separation with the demucs-onnx runtime

**Status:** Stems 2 spike, validated locally on Windows x64 CPU with ONNX Runtime
1.32.0 and the `HRSadeghi/demucs-onnx` `htdemucs` graph.

## What is wired

- `lib/demucs-onnx/` is a vendored copy of the **header-only, MIT-licensed**
  C++ runtime from <https://github.com/HRSadeghi/demucs-onnx>, commit
  `757ab9face37c20edd2e08ac82410da82320763b`. It provides the ONNX Runtime
  session, execution-provider fallback, STFT, chunking, track-level
  normalization, overlap-add and bag blending.
- `src/stems/demucsonnxseparator.{h,cpp}` wraps `demucs::Separator` and exposes
  Paramix's interleaved eight-channel contract:
  `[drums L/R, bass L/R, other L/R, vocals L/R]`.
- `src/stems/stemseparatorfactory.{h,cpp}` is the availability seam. Without
  ORT it returns null and a reason; with ORT it creates the separator.
- `cmake/modules/Findonnxruntime.cmake` and the `STEM_SEPARATION` option are
  described in the PR that added the build integration.

The model contract is the demucs-onnx one:

```text
inputs   mix [1, 2, 343980]          time-domain chunk, 7.8 s at 44.1 kHz
         mag [1, 4, 2048, 336]       CaC spectrogram, plane 2c real / 2c+1 imag
outputs  spec [1, 4, 4, 2048, 336]   per-source CaC spectrogram
         wav [1, 4, 2, 343980]       per-source time-branch waveform
stem[s] = wav[s] + ISTFT(spec[s])
```

## GPU acceleration

`Settings -> Stems -> Performance -> GPU acceleration` writes
`[Stems],GpuAcceleration`. The key is stored only when it differs from the
detected default, so a machine that has never touched the setting follows the
hardware probe. `src/stems/stemgpudetect.*` decides that default:

| Platform | Default-on when |
|---|---|
| Windows | DXGI reports a non-software adapter with ≥ 1 GiB dedicated video memory from NVIDIA or AMD |
| macOS arm64 | always (Apple Silicon's unified-memory GPU is a real accelerator) |
| macOS x86_64 | an `IOAccelerator` that is not `IntelAccelerator` is present |
| Linux | a DRM card from NVIDIA, or an AMD card with ≥ 1 GiB `mem_info_vram_total` |

The runtime must also report a GPU execution provider
(`hasGpuInferenceProvider()`), so a CPU-only ONNX Runtime build stays on the CPU
even on a machine with a discrete GPU. Enabled means `device = "auto"`: the
vendored runtime walks TensorRT → CUDA → ROCm → WebGPU and falls back to CPU if
none of them initialises. A provider that initialises but fails on `Run` is
handled by `StemService`, which re-runs the job on the CPU and disables GPU
acceleration for the rest of the process.

CoreML is deliberately not compiled in (`DEMUCS_WITH_COREML` is never
defined): with the current `htdemucs.onnx` export it builds a session for
1217/2390 nodes and then fails at run time with
`Error executing model: Error in dynamically resizing for sequence length`.
WebGPU runs the same graph correctly.

Measured on an Intel i9-9980HK / AMD Radeon Pro 5300M (macOS x86_64, ONNX
Runtime 1.23.0, 161 MB `htdemucs.onnx`, 30 s stereo clip):

| Provider | Time | Real-time factor |
|---|---|---|
| CPU (4 threads) | 15.3 s | 1.96× |
| WebGPU, batch 1 | 9.7 s | 3.08× |
| WebGPU, batch 4 | 10.4 s | 2.89× |

The host STFT/iSTFT, normalisation, overlap-add and the interleaved repack stay
on the CPU, so the end-to-end gain is smaller than the network's own speedup.

## Local build

```powershell
# ONNX Runtime 1.32.0, Windows x64 CPU package
gh release download v1.32.0 -R microsoft/onnxruntime `
  -p "onnxruntime-win-x64-1.32.0.zip" -D buildenv
Expand-Archive buildenv/onnxruntime-win-x64-1.32.0.zip `
  -DestinationPath buildenv/onnxruntime-win-x64-1.32.0

cmake -S . -B build `
  -DSTEM_SEPARATION=ON `
  -DSTEM_SEPARATION_ORT_ROOT=buildenv/onnxruntime-win-x64-1.32.0/onnxruntime-win-x64-1.32.0
cmake --build build --target mixxx-test
```

## Test model

The CI and unit tests use a tiny synthetic graph with the same contract, not
pretrained weights:

```sh
python tools/make_test_demucs_onnx.py --out-dir build/testmodel
PARAMIX_TEST_DEMUCS_ONNX=build/testmodel/testdemucs.onnx \
  build/mixxx-test --gtest_filter=DemucsOnnxSeparatorTest.*
```

## Real model spike

The exported graphs are on Hugging Face:

```sh
curl -L -o models/htdemucs.onnx \
  https://huggingface.co/HRSadeghi/demucs-onnx/resolve/main/htdemucs.onnx
curl -L -o models/htdemucs.json \
  https://huggingface.co/HRSadeghi/demucs-onnx/resolve/main/htdemucs.json
PARAMIX_TEST_DEMUCS_ONNX=models/htdemucs.onnx \
  build/mixxx-test --gtest_filter=DemucsOnnxSeparatorTest.SeparatesTinyModelAndReportsProgress
```

On this machine the real graph separated one padded 7.8 s window in about 5 s
on the CPU provider. The test asserts finite output, contiguous offsets,
progress and non-zero energy.

## Licensing

The runtime code is MIT. The **HTDemucs weights are not MIT**: Meta's v3/v4
checkpoints are **CC BY-NC 4.0** (non-commercial). The plan/board text that
called the weights MIT is wrong. They are suitable for local validation, but
they must not be bundled or redistributed with Paramix until a licensing
decision lands (ADR-005 / packaging).

## Known limits in this spike

- The output is raw, unclipped float audio. The shared scalar limiter from the
  DSP follow-up is not wired into this wrapper yet; the cache writer will own
  the scalar gain.
- Only four-source models are accepted; `htdemucs_6s` is rejected.
- Cancellation is implemented by unwinding from the output sink, because
  `demucs::Separator` has no cancellation callback.
- The caller supplies planar stereo at `sampleRate()` (44.1 kHz). Resampling
  from the source track rate is not wired yet; the vendored `resample.hpp` is
  available for it.
- The CI job below covers Linux x64 CPU only. Windows and macOS jobs follow
  once the packaging/runtime distribution decision is made.
