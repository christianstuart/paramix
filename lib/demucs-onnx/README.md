# demucs-onnx (vendored)

Header-only C++ runtime for HTDemucs exported at the complex-as-channels
boundary.

- Upstream: <https://github.com/HRSadeghi/demucs-onnx>
- Vendored from commit: `757ab9face37c20edd2e08ac82410da82320763b`
- Imported files: `cpp/include/demucs/*.hpp`
- License: MIT (see `LICENSE`)

Only the header-only library is vendored. Model weights are **not** included.
The upstream runtime code is MIT, but the HTDemucs v3/v4 weights are Meta's and
are licensed **CC BY-NC 4.0** (non-commercial), so they must not be
redistributed with Paramix without a licensing decision.

## Local changes

Paramix adds a `Device::WebGpu` case to `session.hpp`: it parses `webgpu` and
appends the runtime's generic `WebGPU` execution provider. This is the GPU path
that works with the exported HTDemucs graph on macOS; the CoreML provider builds
a session but fails on the first `Run` with a dynamic-shape error, so Paramix
does not compile the CoreML path in. Everything else is upstream.

Used by `src/stems/demucsonnxseparator.cpp` to provide the ONNX Runtime
session, STFT, chunking, track-level normalization, overlap-add and bag
blending.
