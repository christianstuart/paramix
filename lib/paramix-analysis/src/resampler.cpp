#include "resampler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <numeric>
#include <utility>
#include <vector>

#include "float32.h"
#include "kernel.h"
#include "window.h"

namespace paramix::analysis {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kBeta = 8.0;
constexpr double kKernelRadius = 8.0;

double normalizedKernel(double u, double cutoff, double radius) {
    if (std::fabs(u) >= radius) {
        return 0.0;
    }
    const double x = kPi * cutoff * u;
    const double sinc = std::fabs(x) < 1e-12 ? 1.0 : std::sin(x) / x;
    const double ratio = u / radius;
    const double window = bessi0(kBeta * std::sqrt(1.0 - ratio * ratio)) /
            bessi0(kBeta);
    return cutoff * sinc * window;
}

class CopyStage final : public SampleProcessor {
  public:
    explicit CopyStage(SampleProcessor* sink)
            : m_sink(sink) {
    }

    void onSamples(const float* samples, int count) override {
        if (count > 0) {
            m_sink->onSamples(samples, count);
        }
    }

    void flush() override {
        m_sink->flush();
    }

  private:
    SampleProcessor* m_sink;
};

/// Exact mode-6 half-band decimator (44.1 kHz -> 22.05 kHz).
class HalfBandStage final : public SampleProcessor {
  public:
    explicit HalfBandStage(SampleProcessor* sink)
            : m_sink(sink) {
    }

    void onSamples(const float* samples, int count) override {
        for (int i = 0; i < count; ++i) {
            m_buffer.push_back(samples[i]);
        }
        m_totalInput += count;
        emit(false);
        sendPending();
    }

    void flush() override {
        emit(true);
        sendPending();
        m_sink->flush();
    }

  private:
    float sampleAt(std::int64_t index) const {
        if (index < m_base ||
                index >= m_base + static_cast<std::int64_t>(m_buffer.size())) {
            return 0.0f;
        }
        return m_buffer[static_cast<std::size_t>(index - m_base)];
    }

    void emit(bool final) {
        const std::int64_t availableLast = final
                ? m_totalInput - 1
                : m_base + static_cast<std::int64_t>(m_buffer.size()) - 1;
        const float* taps = mode6Taps();
        for (;;) {
            const std::int64_t output = m_nextOutput;
            if (final) {
                if (output >= m_totalInput / 2) {
                    break;
                }
            } else if (2 * output > availableLast) {
                break;
            }

            float accumulator = 0.0f;
            for (int k = 15; k >= 0; --k) {
                const std::int64_t index = 2 * output - 1 - 2 * k;
                if (index >= 0) {
                    // The reference computes the product and the sum in
                    // double precision and rounds both back to float32
                    // (Math.fround). Doing the same avoids double-rounding
                    // drift versus a direct float multiply/add.
                    const float product = f32(static_cast<double>(taps[k]) *
                            static_cast<double>(sampleAt(index)));
                    accumulator = f32(static_cast<double>(accumulator) +
                            static_cast<double>(product));
                }
            }
            const std::int64_t evenIndex = 2 * output - 16;
            if (evenIndex >= 0) {
                accumulator = f32(static_cast<double>(accumulator) +
                        static_cast<double>(sampleAt(evenIndex)));
            }
            m_pending.push_back(f32(0.5 * static_cast<double>(accumulator)));
            ++m_nextOutput;

            if (m_pending.size() >= 4096) {
                sendPending();
            }
        }

        const std::int64_t keepFrom = 2 * m_nextOutput - 40;
        while (!m_buffer.empty() && m_base < keepFrom) {
            m_buffer.pop_front();
            ++m_base;
        }
    }

    void sendPending() {
        if (!m_pending.empty()) {
            m_sink->onSamples(m_pending.data(),
                    static_cast<int>(m_pending.size()));
            m_pending.clear();
        }
    }

    SampleProcessor* m_sink;
    std::deque<float> m_buffer;
    std::vector<float> m_pending;
    std::int64_t m_base = 0;
    std::int64_t m_totalInput = 0;
    std::int64_t m_nextOutput = 0;
};

/// General polyphase resampler using the same Kaiser-windowed sinc kernel.
/// The cutoff follows the lower of the two Nyquist frequencies.
class PolyphaseStage final : public SampleProcessor {
  public:
    PolyphaseStage(int inputRate, SampleProcessor* sink)
            : m_sink(sink) {
        const int divisor = std::gcd(inputRate, kAnalysisRate);
        m_up = inputRate / divisor;
        m_down = kAnalysisRate / divisor;
        m_cutoff = std::min(1.0,
                static_cast<double>(kAnalysisRate) /
                        static_cast<double>(inputRate));
        m_radius = kKernelRadius / m_cutoff;
        buildTables();
        m_keepRadius = static_cast<std::int64_t>(std::ceil(m_radius)) + 2;
    }

    void onSamples(const float* samples, int count) override {
        for (int i = 0; i < count; ++i) {
            m_buffer.push_back(samples[i]);
        }
        m_totalInput += count;
        emit(false);
        sendPending();
    }

    void flush() override {
        m_totalOutput = (m_totalInput * m_down) / m_up;
        emit(true);
        sendPending();
        m_sink->flush();
    }

  private:
    struct Tap {
        int offset;
        double weight;
    };

    float sampleAt(std::int64_t index) const {
        if (index < m_base ||
                index >= m_base + static_cast<std::int64_t>(m_buffer.size())) {
            return 0.0f;
        }
        return m_buffer[static_cast<std::size_t>(index - m_base)];
    }

    void buildTables() {
        m_tables.resize(static_cast<std::size_t>(m_down));
        for (int phase = 0; phase < m_down; ++phase) {
            const double fraction = static_cast<double>(phase) /
                    static_cast<double>(m_down);
            const int first = static_cast<int>(
                    std::ceil(fraction - m_radius));
            const int last = static_cast<int>(
                    std::floor(fraction + m_radius));
            std::vector<Tap> taps;
            double sum = 0.0;
            for (int offset = first; offset <= last; ++offset) {
                const double u = fraction - static_cast<double>(offset);
                const double weight = normalizedKernel(u, m_cutoff, m_radius);
                if (weight != 0.0) {
                    taps.push_back(Tap{offset, weight});
                    sum += weight;
                }
            }
            if (sum != 0.0) {
                for (Tap& tap : taps) {
                    tap.weight /= sum;
                }
            }
            m_tables[static_cast<std::size_t>(phase)] = std::move(taps);
        }
    }

    void emit(bool final) {
        const std::int64_t availableLast = final
                ? m_totalInput - 1
                : m_base + static_cast<std::int64_t>(m_buffer.size()) - 1;
        for (;;) {
            const std::int64_t output = m_nextOutput;
            if (final) {
                if (output >= m_totalOutput) {
                    break;
                }
            } else {
                const std::int64_t positionNumerator = output * m_up;
                const std::int64_t need = positionNumerator / m_down +
                        static_cast<std::int64_t>(std::ceil(m_radius));
                if (need > availableLast) {
                    break;
                }
                const std::int64_t availableCount = availableLast + 1;
                if ((positionNumerator + m_up) > availableCount * m_down) {
                    break;
                }
            }

            const std::int64_t numerator = output * m_up;
            const std::int64_t base = numerator / m_down;
            const int phase = static_cast<int>(numerator % m_down);
            const std::vector<Tap>& taps = m_tables[static_cast<std::size_t>(phase)];

            double accumulator = 0.0;
            for (const Tap& tap : taps) {
                const float sample = sampleAt(base + tap.offset);
                if (sample != 0.0f) {
                    accumulator += tap.weight * static_cast<double>(sample);
                }
            }
            m_pending.push_back(f32(accumulator));
            ++m_nextOutput;

            if (m_pending.size() >= 4096) {
                sendPending();
            }
        }

        const std::int64_t nextNumerator = m_nextOutput * m_up;
        const std::int64_t nextBase = nextNumerator / m_down;
        const std::int64_t keepFrom = nextBase - m_keepRadius;
        while (!m_buffer.empty() && m_base < keepFrom) {
            m_buffer.pop_front();
            ++m_base;
        }
    }

    void sendPending() {
        if (!m_pending.empty()) {
            m_sink->onSamples(m_pending.data(),
                    static_cast<int>(m_pending.size()));
            m_pending.clear();
        }
    }

    SampleProcessor* m_sink;
    int m_up = 1;
    int m_down = 1;
    double m_cutoff = 1.0;
    double m_radius = kKernelRadius;
    std::int64_t m_keepRadius = 10;
    std::vector<std::vector<Tap>> m_tables;
    std::deque<float> m_buffer;
    std::vector<float> m_pending;
    std::int64_t m_base = 0;
    std::int64_t m_totalInput = 0;
    std::int64_t m_nextOutput = 0;
    std::int64_t m_totalOutput = 0;
};

class StageChain final : public SampleProcessor {
  public:
    void add(std::unique_ptr<SampleProcessor> stage) {
        m_stages.push_back(std::move(stage));
    }

    void onSamples(const float* samples, int count) override {
        if (!m_stages.empty()) {
            m_stages.front()->onSamples(samples, count);
        }
    }

    void flush() override {
        if (!m_stages.empty()) {
            m_stages.front()->flush();
        }
    }

  private:
    std::vector<std::unique_ptr<SampleProcessor>> m_stages;
};

} // namespace

std::unique_ptr<SampleProcessor> makeResampler(
        int inputRate, SampleProcessor* sink) {
    auto chain = std::make_unique<StageChain>();
    if (inputRate == kAnalysisRate) {
        chain->add(std::make_unique<CopyStage>(sink));
        return chain;
    }

    if (inputRate > 0 && inputRate % kAnalysisRate == 0) {
        const int factor = inputRate / kAnalysisRate;
        int stages = 0;
        for (int value = factor; value > 1; value >>= 1) {
            if ((value & 1) != 0) {
                stages = 0;
                break;
            }
            ++stages;
        }
        if (stages > 0) {
            // Build the cascade back to front so every stage can hold a raw
            // pointer to its already-constructed successor.
            std::vector<std::unique_ptr<SampleProcessor>> built;
            SampleProcessor* next = sink;
            for (int i = 0; i < stages; ++i) {
                auto stage = std::make_unique<HalfBandStage>(next);
                next = stage.get();
                built.push_back(std::move(stage));
            }
            for (auto it = built.rbegin(); it != built.rend(); ++it) {
                chain->add(std::move(*it));
            }
            return chain;
        }
    }

    chain->add(std::make_unique<PolyphaseStage>(inputRate, sink));
    return chain;
}

std::int64_t resampledFrameCount(std::int64_t inputFrames, int inputRate) {
    if (inputFrames <= 0 || inputRate <= 0) {
        return 0;
    }
    if (inputRate == kAnalysisRate) {
        return inputFrames;
    }
    if (inputRate > 0 && inputRate % kAnalysisRate == 0) {
        std::int64_t frames = inputFrames;
        int factor = inputRate / kAnalysisRate;
        while (factor > 1 && (factor & 1) == 0) {
            frames /= 2;
            factor >>= 1;
        }
        if (factor == 1) {
            return frames;
        }
    }
    return (inputFrames * kAnalysisRate) / inputRate;
}

} // namespace paramix::analysis
