# demucs-onnx (vendored)

> **Shipping runtime (2026-11-16):** this vendored runtime is the separator
> Paramix ships. The app bundles `models/htdemucs.onnx`; see
> [`docs/third-party-models.md`](../../docs/third-party-models.md) for the
> weight licence and the accepted risk.

Header-only C++ runtime for HTDemucs exported at the complex-as-channels
boundary.

- Upstream: <https://github.com/HRSadeghi/demucs-onnx>
- Vendored from commit: `757ab9face37c20edd2e08ac82410da82320763b`
- Imported files: `cpp/include/demucs/*.hpp`
- License: MIT (see `LICENSE`)

Only the header-only library is vendored. Model weights are **not** included.
The upstream runtime code is MIT, but the HTDemucs v3/v4 weights are Meta's and
have **no explicit weights licence**: the maintainer provides them for
scientific purposes only. "CC BY-NC 4.0" is a conservative shorthand, not an
upstream grant. Paramix bundles the converted graph with that risk accepted;
see [`docs/third-party-models.md`](../../docs/third-party-models.md).

## Local changes

Paramix adds a `Device::WebGpu` case to `session.hpp`: it parses `webgpu` and
appends the runtime's generic `WebGPU` execution provider. This is the GPU path
that works with the exported HTDemucs graph on macOS; the CoreML provider builds
a session but fails on the first `Run` with a dynamic-shape error, so Paramix
does not compile the CoreML path in. Everything else is upstream.

Used by `src/stems/demucsonnxseparator.cpp` to provide the ONNX Runtime
session, STFT, chunking, track-level normalization, overlap-add and bag
blending.
