# Third-party models and separation runtimes

**Status:** compliance reference for W3 (packaging) and ADR-005 (model distribution).
**Last verified:** 2026-11-16.

> **Shipping decision (2026-11-16):** the UI has one mode. The bundled Demucs
> checkpoint (`models/htdemucs.onnx`) is the only separation model shipped. SCNet
> remains experimental and is not offered in the UI, and the Open-Unmix backend
> was removed. The maintainer has accepted the remaining licensing risk; this
> file stays as the factual record, not a release gate.

This file records the licence of every model weight and inference component that
Paramix uses or has considered. It separates **code licences** from **weight
licences**: a permissive repository licence does not automatically cover the
pretrained weights.

Paramix is GPL-2.0. A component may only be linked, bundled, or redistributed if
its licence permits it and is compatible with GPL-2.0. Here, "GPL-2.0-permissible"
means compatible with GPL-2.0 distribution; it does not require the model itself
to be GPL-2.0. MIT is compatible; Apache-2.0, CC BY-NC, and GPL-3.0 are not
treated as compatible here. Nothing in this file is legal advice; it is the
evidence the project relies on until counsel says otherwise.

## 1. Shipping model

The UI does not name the model and has no model choice; separation always uses
the bundled checkpoint.

| Model | Code / weights | Stems | Notes |
|---|---|---|---|
| Demucs `htdemucs.onnx` | MIT / research-only | 4 | Bundled; the only shipping separation model; licensing risk accepted by the maintainer |

SCNet/SCNet-large remain in the tree as an experimental backend
(`scnetonnxseparator.*`) but are not offered in the UI. Open-Unmix has been
removed from the codebase; its licence record is kept below for reference.

### 1.1 Experimental: SCNet

SCNet is MIT code + MIT weights and has real-model tests, but it was slower and
lower quality in the app than the alternatives. It is kept for evaluation:
13 MB (base) / 45 MB (large) int8 ONNX, ~9.0 / 9.7 dB SDR. See the history in
this file for the licence evidence.

### 1.2 SCNet details (experimental)

- **Authors:** Tong et al., *SCNet: Sparse Compression Network for Music Source
  Separation*, ICASSP 2024.
- **Code:** MIT, <https://github.com/starrytong/SCNet>.
- **Weights:** MIT. The author explicitly confirmed that the released SCNet and
  SCNet-large checkpoints are MIT, and that redistribution and format-converted
  ONNX exports are permitted, in
  [starrytong/SCNet#35](https://github.com/starrytong/SCNet/issues/35)
  (2026-07-17).
- **ONNX exports:** <https://huggingface.co/audiojs/scnet> and
  <https://huggingface.co/audiojs/scnet-large> (MIT). The int8 files are the
  small artifacts; float32 exports are the reference.
- **Contract:** four stems (drums, bass, other, vocals), 44.1 kHz stereo,
  11-second segments, STFT `n_fft=4096`, `hop=1024`. The host must implement the
  STFT/iSTFT and chunk overlap; the ONNX graph covers the network only.
- **Dataset caveat:** trained on MUSDB18-HQ, which is research/non-commercial.
  The author's explicit MIT grant covers the weights; do not redistribute the
  dataset audio.

### 1.3 Open-Unmix `umxhq`

- **Code:** MIT, <https://github.com/sigsep/open-unmix-pytorch>.
- **Weights:** `umxhq` is MIT, published by the authors on Zenodo record
  [3370489](https://zenodo.org/records/3370489). `umxl` is **CC BY-NC-SA 4.0**
  and must not be used.
- **Contract:** four stems, 44.1 kHz stereo, magnitude spectrogram + multichannel
  Wiener filtering. The repo includes a Python/PyTorch path and ONNX exports
  exist through <https://github.com/audiojs/neural>; there is no official
  upstream ONNX file.
- **Do not use `umx.cpp` as-is:** its default/only published weights are `umxl`
  (non-commercial). Point it at `umxhq` only if the conversion is verified.
- **Role:** not used. The Open-Unmix backend was removed because its quality was
  unacceptable in the app; the licence record is kept only for reference.

### 1.4 Optional vocal-only tier

**Mel-Band RoFormer, Kim Vocal 2** is MIT (relicensed 2026-04-22; the author
permits redistribution) and reaches about 12.1 dB vocal SDR. It is a
**vocals/instrumental** model, not a four-stem separator, so it is not one of the
three defaults. Use it only for an acapella/instrumental tier, and derive the
instrumental as `mixture - vocals`. Source:
<https://huggingface.co/KimberleyJSN/melbandroformer>.

### 1.5 Alternate 4-stem option (not used)

**KUIELab MDX-Net** (MDX Challenge leaderboard A/B) is MIT code and the authors
distribute the per-stem ONNX weights from that MIT repository. Quality is older
(~7 dB SDR), but it is a clean-licensed 4-stem fallback if a permissively
licensed model is ever needed. Source:
<https://github.com/kuielab/mdx-net-submission>.

## 2. Demucs weights (high-quality mode)

Demucs is the bundled high-quality mode. The code is MIT, but the official
pretrained weights are not covered by that licence:

- The maintainer, Alexandre Défossez, in
  [facebookresearch/demucs#327](https://github.com/facebookresearch/demucs/issues/327)
  (2022-05-23): *"The model weights are not covered by the MIT license, and are
  provided only for scientific purposes."*
- The weights were trained on MUSDB18-HQ plus 800 Meta songs with no public
  provenance or licence grant.
- There is no explicit permission to redistribute the checkpoints.

The maintainer has accepted this risk for the bundled high-quality mode. The
weights stay out of git; packaging copies the local `models/htdemucs.onnx` into
the installer.

## 3. ONNX Runtime

- Licence: **MIT**.
- Role: inference library only; no weights.
- Distribution: the shared library per platform (`onnxruntime.dll`,
  `libonnxruntime.so`, `libonnxruntime.dylib`) is bundled with the app.
- Obligations: retain the MIT notice and ONNX Runtime's own third-party notices
  in `NOTICE` and the installer's licence material.
- ADR-001 §2.7 records the dynamic-linking and packaging decision.

### 3.1 GPU execution providers

`[Stems],GpuAcceleration` selects `auto`; the shared session helper walks the
providers the loaded runtime actually reports and prefers the platform API.
Point `STEM_SEPARATION_ORT_ROOT` at the GPU-enabled runtime; CMake copies the
Windows provider DLLs beside the app.

| Platform | Preferred provider | Runtime package |
|---|---|---|
| Windows | DirectML, then CUDA/TensorRT | DirectML ORT build (AMD/Intel/NVIDIA) or CUDA ORT build (NVIDIA) |
| macOS | CoreML, then WebGPU | CoreML ORT build, or a WebGPU-enabled build |
| Linux | CUDA, then ROCm | CUDA/ROCm ORT build |

A provider that cannot initialise is skipped and the backend falls back to CPU;
a provider that initialises but fails on `Run` is retried on CPU by
`StemService`.

**Validated:** Windows DirectML with ONNX Runtime 1.24.4 runs Demucs (and
previously SCNet/Open-Unmix); DirectML needs `ep.dml.disable_graph_fusion=1` and
memory-pattern planning off for quantized graphs. The repository's bundled 1.32
build is CPU-only, so a release needs a GPU-enabled runtime package. macOS
CoreML/WebGPU and Linux CUDA/ROCm are implemented in the provider selector but
have not been run on this Windows machine.

## 4. Experimental backend

SCNet/SCNet-large are available behind a path setting or the old model file but
are not offered in the UI:

- `src/stems/scnetonnxseparator.*`
- `models/scnet.int8.onnx`, `models/scnet-large.int8.onnx`

They are useful for comparison; the only shipping model is Demucs.

## 5. Not selected

| Model | Why not |
|---|---|
| BS-RoFormer "viperx" | Code MIT, but the community checkpoint has no stated weights licence. |
| MDX23C / UVR community weights | Code MIT, but the community checkpoints are mostly unlicensed. |
| Spleeter | Weights MIT, but TensorFlow/TFLite is Apache-2.0, which conflicts with GPL-2.0 unless an ONNX-only path removes the TF dependency. |
| Bandit | Apache-2.0 code; Apache-2.0 is not GPL-2.0-compatible. |
| Open-Unmix `umxl` | CC BY-NC-SA 4.0; non-commercial only. |

Beat tracking is not a model dependency: Queen Mary is already in the build
(`analyzerqueenmarybeats.cpp`). No beat weights are downloaded or shipped.

## 6. Datasets

MUSDB18-HQ is research/non-commercial. Use it for offline evaluation only; never
ship its audio. The SCNet and Open-Unmix weight grants above are the operative
permissions for their released weights; do not treat the dataset itself as
distributable.

## 7. Rules for packaging and model hosting

1. Model weights stay out of git. The one shipping model,
   `models/htdemucs.onnx`, is copied into the installer by packaging when present
   locally; `models/` and `stems/` remain gitignored.
2. A hosted model must have a rights holder grant that is GPL-2.0-compatible.
3. Every manifest entry carries SPDX id and full licence text; the client shows
   both before download and refuses a model without them
   (`docs/model-manifest.md`).
4. Demucs `htdemucs` is bundled with the maintainer's accepted licensing risk;
   it is the only supported separation model.
5. `NOTICE` points to this file for model and inference-runtime attribution.
6. The shipped model is the single model in §1. Do not add a model whose weights
   licence is not recorded here.

## 8. Review

Re-verify this file when a model version, weights host, or upstream licence
statement changes. Record the date and the source in the change. If a claim here
conflicts with another document, this file wins for licensing; update the other
document rather than duplicating a licence claim.
