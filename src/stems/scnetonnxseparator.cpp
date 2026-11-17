#include "stems/scnetonnxseparator.h"

#include <dsp/transforms/FFT.h>
#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "stems/onnxstemruntime.h"
#include "stems/stemseparatorbackend.h"

namespace mixxx::stems {
namespace {

constexpr int kSampleRate = 44100;
constexpr int kChannels = 2;
constexpr int kStemChannels = kStemCount * 2;
constexpr int kFftSize = 4096;
constexpr int kHopLength = 1024;
constexpr int kBins = kFftSize / 2 + 1;
constexpr int kSpecFrames = 476;
constexpr SINT kSegmentFrames = 485100;
constexpr int kSourceCount = 4;

struct Spectrum {
    std::vector<float> re;
    std::vector<float> im;
    int frames = 0;
};

std::string failure(std::string* pError, const std::string& message) {
    if (pError != nullptr) {
        *pError = message;
    }
    return message;
}

SINT reflectIndex(SINT index, SINT length) {
    if (length <= 1) {
        return 0;
    }
    const SINT period = 2 * (length - 1);
    SINT reflected = index % period;
    if (reflected < 0) {
        reflected += period;
    }
    if (reflected >= length) {
        reflected = period - reflected;
    }
    return reflected;
}

/// STFT with the SCNet framing: 4096-point FFT, 1024 hop, no window,
/// normalized by sqrt(n), centered with reflect padding.
Spectrum analyze(const std::vector<float>& samples) {
    Spectrum spectrum;
    spectrum.frames = 1 + static_cast<int>(samples.size()) / kHopLength;
    spectrum.re.assign(static_cast<std::size_t>(spectrum.frames) * kBins, 0.0f);
    spectrum.im.assign(static_cast<std::size_t>(spectrum.frames) * kBins, 0.0f);

    const int pad = kFftSize / 2;
    const double scale = 1.0 / std::sqrt(static_cast<double>(kFftSize));
    std::vector<double> frame(kFftSize);
    std::vector<double> real(kFftSize);
    std::vector<double> imaginary(kFftSize);
    FFTReal fft(kFftSize);

    for (int m = 0; m < spectrum.frames; ++m) {
        const SINT position = static_cast<SINT>(m) * kHopLength - pad;
        for (int i = 0; i < kFftSize; ++i) {
            const SINT index = position + i;
            frame[static_cast<std::size_t>(i)] =
                    index >= 0 && index < static_cast<SINT>(samples.size())
                    ? samples[static_cast<std::size_t>(index)]
                    : samples[static_cast<std::size_t>(
                              reflectIndex(index, static_cast<SINT>(samples.size())))];
        }
        fft.forward(frame.data(), real.data(), imaginary.data());
        for (int bin = 0; bin < kBins; ++bin) {
            const std::size_t target =
                    static_cast<std::size_t>(m) * kBins + bin;
            spectrum.re[target] = static_cast<float>(real[bin] * scale);
            spectrum.im[target] = static_cast<float>(imaginary[bin] * scale);
        }
    }
    return spectrum;
}

/// Inverse of analyze(): squared-window overlap-add with a rectangular
/// window, then center-pad removal and crop to `length`.
std::vector<float> synthesize(const std::vector<float>& re,
        const std::vector<float>& im,
        int frames,
        SINT length) {
    const std::size_t outLength =
            static_cast<std::size_t>((frames - 1) * kHopLength + kFftSize);
    const int pad = kFftSize / 2;
    std::vector<double> output(outLength, 0.0);
    std::vector<double> envelope(outLength, 0.0);
    std::vector<double> real(kBins);
    std::vector<double> imaginary(kBins);
    std::vector<double> time(kFftSize);
    FFTReal fft(kFftSize);

    for (int m = 0; m < frames; ++m) {
        for (int bin = 0; bin < kBins; ++bin) {
            const std::size_t index =
                    static_cast<std::size_t>(m) * kBins + bin;
            real[bin] = re[index];
            imaginary[bin] = im[index];
        }
        fft.inverse(real.data(), imaginary.data(), time.data());
        const std::size_t start = static_cast<std::size_t>(m) * kHopLength;
        for (int i = 0; i < kFftSize; ++i) {
            output[start + i] += time[static_cast<std::size_t>(i)];
            envelope[start + i] += 1.0;
        }
    }

    std::vector<float> result(static_cast<std::size_t>(length), 0.0f);
    for (SINT i = 0; i < length; ++i) {
        const std::size_t index = static_cast<std::size_t>(pad + i);
        if (index < outLength && envelope[index] > 1e-8) {
            result[static_cast<std::size_t>(i)] =
                    static_cast<float>(output[index] / envelope[index]);
        }
    }
    return result;
}

/// Runs one 11 s segment through the graph. Returns source-major channels:
/// [source][channel], source order drums, bass, other, vocals.
std::vector<std::vector<float>> runSegment(Ort::Session& session,
        const std::string& inputName,
        const std::string& outputName,
        const std::vector<std::vector<float>>& segment) {
    const int channels = static_cast<int>(segment.size());
    const std::size_t length = segment[0].size();
    const std::size_t paddedLength = std::max(
            length, static_cast<std::size_t>((kSpecFrames - 1) * kHopLength));
    const double scale = 1.0 / std::sqrt(static_cast<double>(kFftSize));

    std::vector<float> spec(
            static_cast<std::size_t>(2 * channels * kBins * kSpecFrames), 0.0f);
    for (int channel = 0; channel < channels; ++channel) {
        std::vector<float> padded(paddedLength, 0.0f);
        std::copy(segment[channel].begin(), segment[channel].end(), padded.begin());
        const Spectrum spectrum = analyze(padded);
        const std::size_t reBase =
                static_cast<std::size_t>(2 * channel) * kBins * kSpecFrames;
        const std::size_t imBase = reBase + static_cast<std::size_t>(kBins) * kSpecFrames;
        for (int frame = 0; frame < kSpecFrames; ++frame) {
            for (int bin = 0; bin < kBins; ++bin) {
                const std::size_t source =
                        static_cast<std::size_t>(frame) * kBins + bin;
                const std::size_t target =
                        static_cast<std::size_t>(bin) * kSpecFrames + frame;
                // analyze() already applies the 1/sqrt(n) normalization;
                // scaling again here fed the graph input 36 dB too quiet.
                spec[reBase + target] = spectrum.re[source];
                spec[imBase + target] = spectrum.im[source];
            }
        }
    }

    const std::array<int64_t, 4> inputShape{
            1, 2 * channels, kBins, kSpecFrames};
    Ort::MemoryInfo memoryInfo =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input = Ort::Value::CreateTensor<float>(memoryInfo,
            spec.data(),
            spec.size(),
            inputShape.data(),
            inputShape.size());
    const char* inputNames[] = {inputName.c_str()};
    const char* outputNames[] = {outputName.c_str()};
    auto outputs = session.Run(Ort::RunOptions{nullptr},
            inputNames,
            &input,
            1,
            outputNames,
            1);
    if (outputs.size() != 1) {
        throw std::runtime_error("SCNet returned an unexpected number of outputs");
    }
    const auto shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    if (shape.size() != 4) {
        throw std::runtime_error("SCNet output is not rank 4");
    }
    const std::size_t expected = static_cast<std::size_t>(
            2 * kSourceCount * channels * kBins * kSpecFrames);
    const std::size_t actual = static_cast<std::size_t>(outputs[0]
                    .GetTensorTypeAndShapeInfo()
                    .GetElementCount());
    if (actual != expected) {
        throw std::runtime_error("SCNet output has an unexpected size");
    }

    const float* pOutput = outputs[0].GetTensorData<float>();
    std::vector<std::vector<float>> stems;
    stems.reserve(static_cast<std::size_t>(kSourceCount * channels));
    for (int source = 0; source < kSourceCount; ++source) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t base = static_cast<std::size_t>(
                    2 * (source * channels + channel) * kBins * kSpecFrames);
            std::vector<float> re(
                    static_cast<std::size_t>(kSpecFrames) * kBins, 0.0f);
            std::vector<float> im(
                    static_cast<std::size_t>(kSpecFrames) * kBins, 0.0f);
            for (int frame = 0; frame < kSpecFrames; ++frame) {
                for (int bin = 0; bin < kBins; ++bin) {
                    const std::size_t sourceIndex =
                            static_cast<std::size_t>(frame) * kBins + bin;
                    const std::size_t modelIndex =
                            static_cast<std::size_t>(bin) * kSpecFrames + frame;
                    re[sourceIndex] =
                            static_cast<float>(pOutput[base + modelIndex] / scale);
                    im[sourceIndex] = static_cast<float>(
                            pOutput[base + static_cast<std::size_t>(kBins) * kSpecFrames +
                                    modelIndex] /
                            scale);
                }
            }
            stems.push_back(synthesize(re, im, kSpecFrames, static_cast<SINT>(length)));
        }
    }
    return stems;
}

} // namespace

class ScnetOnnxSeparator::Impl final {
  public:
    explicit Impl(const ScnetOnnxConfig& config)
            : m_env(ORT_LOGGING_LEVEL_WARNING, "paramix-scnet"),
              m_sources{"drums", "bass", "other", "vocals"},
              m_device("cpu"),
              m_step(config.step),
              m_fade(config.fade) {
        if (!(m_step > 0.0f && m_step < 1.0f)) {
            throw std::runtime_error("step must be in (0, 1)");
        }
        if (!(m_fade >= 0.0f && m_fade < 1.0f)) {
            throw std::runtime_error("fade must be in [0, 1)");
        }
        m_session = onnx::openSession(m_env,
                config.modelPath,
                config.threads > 0 ? config.threads : onnx::boundedThreadCount(),
                config.device,
                &m_device);
        Ort::AllocatorWithDefaultOptions allocator;
        m_inputName = m_session->GetInputNameAllocated(0, allocator).get();
        m_outputName = m_session->GetOutputNameAllocated(0, allocator).get();

        const auto inputType = m_session->GetInputTypeInfo(0);
        const auto inputShape = inputType.GetTensorTypeAndShapeInfo().GetShape();
        if (inputType.GetTensorTypeAndShapeInfo().GetElementType() !=
                        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
                inputShape.size() != 4 ||
                (inputShape[1] > 0 && inputShape[1] != 4) ||
                (inputShape[2] > 0 && inputShape[2] != kBins) ||
                (inputShape[3] > 0 && inputShape[3] != kSpecFrames)) {
            throw std::runtime_error(
                    "model input is not SCNet [1, 4, 2049, 476]");
        }
        const auto outputType = m_session->GetOutputTypeInfo(0);
        const auto outputShape = outputType.GetTensorTypeAndShapeInfo().GetShape();
        if (outputType.GetTensorTypeAndShapeInfo().GetElementType() !=
                        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
                outputShape.size() != 4 ||
                (outputShape[1] > 0 && outputShape[1] != 16)) {
            throw std::runtime_error(
                    "model output is not SCNet [1, 16, 2049, 476]");
        }
    }

    Ort::Env m_env;
    std::unique_ptr<Ort::Session> m_session;
    std::string m_inputName;
    std::string m_outputName;
    std::vector<std::string> m_sources;
    std::string m_device;
    std::string m_lastError;
    float m_step;
    float m_fade;
};

ScnetOnnxSeparator::ScnetOnnxSeparator(std::unique_ptr<Impl> pImpl)
        : m_pImpl(std::move(pImpl)) {
}

ScnetOnnxSeparator::~ScnetOnnxSeparator() = default;

std::unique_ptr<ScnetOnnxSeparator> ScnetOnnxSeparator::create(
        const ScnetOnnxConfig& config, std::string* pError) {
    if (config.modelPath.empty()) {
        failure(pError, "No SCNet ONNX model was selected.");
        return nullptr;
    }
    try {
        auto pImpl = std::make_unique<Impl>(config);
        return std::unique_ptr<ScnetOnnxSeparator>(
                new ScnetOnnxSeparator(std::move(pImpl)));
    } catch (const std::exception& error) {
        failure(pError, error.what());
        return nullptr;
    }
}

int ScnetOnnxSeparator::sampleRate() const {
    return kSampleRate;
}

int ScnetOnnxSeparator::channels() const {
    return kChannels;
}

const std::vector<std::string>& ScnetOnnxSeparator::sources() const {
    return m_pImpl->m_sources;
}

std::string ScnetOnnxSeparator::device() const {
    return m_pImpl->m_device;
}

std::string ScnetOnnxSeparator::lastError() const {
    return m_pImpl->m_lastError;
}

bool ScnetOnnxSeparator::separate(const float* pPlanarStereo,
        SINT frames,
        const StemOutputCallback& writeOutput,
        const StemProgressCallback& progress,
        const StemCancelCallback& cancel) {
    m_pImpl->m_lastError.clear();
    if (frames < 0 || (frames > 0 && pPlanarStereo == nullptr) || !writeOutput) {
        m_pImpl->m_lastError = "invalid SCNet separation arguments";
        return false;
    }
    if (frames == 0) {
        return true;
    }

    try {
        std::vector<std::vector<float>> input(
                kChannels, std::vector<float>(static_cast<std::size_t>(frames)));
        // Planar [channels][frames], per the StemSeparator contract.
        for (SINT frame = 0; frame < frames; ++frame) {
            input[0][static_cast<std::size_t>(frame)] =
                    pPlanarStereo[static_cast<std::size_t>(frame)];
            input[1][static_cast<std::size_t>(frame)] =
                    pPlanarStereo[static_cast<std::size_t>(frames) + frame];
        }

        std::vector<double> mono(static_cast<std::size_t>(frames), 0.0);
        double mean = 0.0;
        for (SINT frame = 0; frame < frames; ++frame) {
            mono[static_cast<std::size_t>(frame)] =
                    0.5 * (input[0][static_cast<std::size_t>(frame)] + input[1][static_cast<std::size_t>(frame)]);
            mean += mono[static_cast<std::size_t>(frame)];
        }
        mean /= frames;
        double variance = 0.0;
        for (SINT frame = 0; frame < frames; ++frame) {
            const double value = mono[static_cast<std::size_t>(frame)] - mean;
            variance += value * value;
        }
        const double stddev =
                std::sqrt(variance / std::max(1, static_cast<int>(frames) - 1)) +
                1e-8;

        std::vector<std::vector<float>> normalized(
                kChannels, std::vector<float>(static_cast<std::size_t>(frames)));
        for (int channel = 0; channel < kChannels; ++channel) {
            for (SINT frame = 0; frame < frames; ++frame) {
                normalized[static_cast<std::size_t>(channel)]
                          [static_cast<std::size_t>(frame)] =
                                  static_cast<float>(
                                          (input[static_cast<std::size_t>(channel)]
                                                [static_cast<std::size_t>(frame)] -
                                                  mean) /
                                          stddev);
            }
        }

        const SINT stepFrames = std::max<SINT>(
                1, static_cast<SINT>(kSegmentFrames * m_pImpl->m_step));
        const SINT border = kSegmentFrames - stepFrames;
        const bool wide = border > 0 && frames > 2 * border;
        std::vector<std::vector<float>> padded;
        if (wide) {
            padded.assign(kChannels,
                    std::vector<float>(static_cast<std::size_t>(frames + 2 * border)));
            for (int channel = 0; channel < kChannels; ++channel) {
                for (SINT i = 0; i < frames + 2 * border; ++i) {
                    padded[static_cast<std::size_t>(channel)]
                          [static_cast<std::size_t>(i)] =
                                  normalized[static_cast<std::size_t>(channel)]
                                            [static_cast<std::size_t>(
                                                    reflectIndex(i - border, frames))];
                }
            }
        } else {
            padded = normalized;
        }

        const SINT length = static_cast<SINT>(padded[0].size());
        std::vector<SINT> starts;
        for (SINT start = 0;; start += stepFrames) {
            starts.push_back(start);
            if (start + kSegmentFrames >= length) {
                break;
            }
        }
        const SINT fadeFrames = std::max<SINT>(
                1, static_cast<SINT>(kSegmentFrames * m_pImpl->m_fade));

        std::vector<float> accumulator(
                static_cast<std::size_t>(kSourceCount * kChannels) * length, 0.0f);
        std::vector<float> weights(static_cast<std::size_t>(length), 0.0f);

        for (std::size_t block = 0; block < starts.size(); ++block) {
            if (cancel && cancel()) {
                m_pImpl->m_lastError = "SCNet separation cancelled";
                return false;
            }
            const SINT start = starts[block];
            const SINT blockFrames = std::min(kSegmentFrames, length - start);
            const bool reflect = blockFrames > kSegmentFrames / 2;
            std::vector<std::vector<float>> segment(
                    kChannels, std::vector<float>(kSegmentFrames, 0.0f));
            for (int channel = 0; channel < kChannels; ++channel) {
                for (SINT i = 0; i < kSegmentFrames; ++i) {
                    if (i < blockFrames) {
                        segment[static_cast<std::size_t>(channel)]
                               [static_cast<std::size_t>(i)] =
                                       padded[static_cast<std::size_t>(channel)]
                                             [static_cast<std::size_t>(start + i)];
                    } else if (reflect) {
                        segment[static_cast<std::size_t>(channel)]
                               [static_cast<std::size_t>(i)] =
                                       padded[static_cast<std::size_t>(channel)]
                                             [static_cast<std::size_t>(
                                                     start + reflectIndex(i, blockFrames))];
                    }
                }
            }

            const auto stems = runSegment(*m_pImpl->m_session,
                    m_pImpl->m_inputName,
                    m_pImpl->m_outputName,
                    segment);
            const bool first = block == 0;
            const bool last = block + 1 == starts.size();
            for (SINT i = 0; i < blockFrames; ++i) {
                float weight = 1.0f;
                if (!first && i < fadeFrames && fadeFrames > 1) {
                    weight = static_cast<float>(i) /
                            static_cast<float>(fadeFrames - 1);
                }
                if (!last && i >= kSegmentFrames - fadeFrames &&
                        fadeFrames > 1) {
                    weight = std::min(weight,
                            static_cast<float>(kSegmentFrames - 1 - i) /
                                    static_cast<float>(fadeFrames - 1));
                }
                weights[static_cast<std::size_t>(start + i)] += weight;
                for (int source = 0; source < kSourceCount; ++source) {
                    for (int channel = 0; channel < kChannels; ++channel) {
                        const std::size_t accumulatorIndex =
                                static_cast<std::size_t>(
                                        source * kChannels + channel) *
                                        length +
                                static_cast<std::size_t>(start + i);
                        accumulator[accumulatorIndex] +=
                                stems[static_cast<std::size_t>(
                                        source * kChannels + channel)]
                                     [static_cast<std::size_t>(i)] *
                                weight;
                    }
                }
            }
            if (progress) {
                progress(static_cast<double>(block + 1) /
                        static_cast<double>(starts.size()));
            }
        }

        std::vector<float> output(
                static_cast<std::size_t>(frames) * kStemChannels, 0.0f);
        for (SINT frame = 0; frame < frames; ++frame) {
            const std::size_t sourceFrame =
                    static_cast<std::size_t>(frame + (wide ? border : 0));
            for (int source = 0; source < kSourceCount; ++source) {
                for (int channel = 0; channel < kChannels; ++channel) {
                    const std::size_t accumulatorIndex =
                            static_cast<std::size_t>(
                                    source * kChannels + channel) *
                                    length +
                            sourceFrame;
                    const float weight = weights[sourceFrame];
                    const float value = weight > 0.0f
                            ? accumulator[accumulatorIndex] / weight *
                                    static_cast<float>(stddev)
                            : 0.0f;
                    output[static_cast<std::size_t>(frame) * kStemChannels +
                            static_cast<std::size_t>(source * kChannels + channel)] =
                            value;
                }
            }
        }
        if (!writeOutput(0, output.data(), frames)) {
            m_pImpl->m_lastError = "SCNet separation cancelled";
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        m_pImpl->m_lastError = error.what();
        return false;
    }
}

} // namespace mixxx::stems
