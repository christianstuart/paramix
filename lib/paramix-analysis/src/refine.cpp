#include "refine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "float32.h"
#include "window.h"

namespace paramix::analysis {

namespace {

constexpr int kBurstsPerBatch = kEnvelopeBatch * 8;
constexpr int kRefineRadius = 12;
constexpr int kRefineLimit = 343;
constexpr float kFrameSeconds = f32(256.0 / 22050.0);
constexpr float kInverseAnalysisRate = f32(1.0 / 22050.0);
constexpr double kRefineScale = 86.1328125; // 22050 / 256

float thresholdScale(float sum, int count) {
    return f32(f32(sum * f32(-1.25f)) / f32(count));
}

// Returns the 0/1/2 slope shift chosen by the native picker.
int pickShift(float cur, const float* slots) {
    const float d0 = f32(cur - slots[3]);
    const float d1 = f32(slots[3] - slots[2]);
    const float d2 = f32(slots[2] - slots[1]);
    int u = 0;
    if (d0 > 0.0f) {
        if (d1 > d0) {
            u = 1;
        }
        if (d2 > std::max(d0, d1)) {
            u = 2;
        }
    }
    return u;
}

void pushPeak(std::vector<PickedPeak>& peaks,
        const float* slots,
        int frame,
        int batch,
        bool finalize,
        int frameOffset) {
    const float cur = slots[0];
    bool accepted = true;
    for (int t = 1; t < 7; ++t) {
        if (!(slots[t] < cur)) {
            accepted = false;
            break;
        }
    }
    if (!accepted) {
        return;
    }
    const int u = pickShift(cur, slots);
    PickedPeak peak;
    peak.frame = frame;
    peak.shift = u;
    peak.flux = cur;
    peak.batch = batch;
    peak.finalize = finalize;
    peak.rawPosition = f32(static_cast<double>(f32(frame - u + frameOffset)) *
            static_cast<double>(kFrameSeconds));
    peaks.push_back(peak);
}

} // namespace

std::vector<PickedPeak> pickPeaks(const std::vector<float>& envelope) {
    std::vector<PickedPeak> peaks;
    const int envelopeSize = static_cast<int>(envelope.size());
    float slots[7] = {};

    const int fullBatches = envelopeSize / kEnvelopeBatch;
    for (int batch = 0; batch < fullBatches; ++batch) {
        const int first = batch * kEnvelopeBatch;
        float sum = 0.0f;
        for (int i = 0; i < kEnvelopeBatch; ++i) {
            sum = f32(sum + envelope[static_cast<std::size_t>(first + i)]);
        }
        const float scale = thresholdScale(sum, kEnvelopeBatch);
        float thresholded[kEnvelopeBatch];
        for (int i = 0; i < kEnvelopeBatch; ++i) {
            const float value = f32(envelope[static_cast<std::size_t>(first + i)] +
                    scale);
            thresholded[i] = value > 0.0f ? value : 0.0f;
        }
        for (int j = 0; j < kEnvelopeBatch; ++j) {
            const int frame = first + j - 4;
            pushPeak(peaks, slots, frame, batch, false, -1);
            const float next[7] = {
                    slots[4],
                    slots[2],
                    slots[3],
                    slots[0],
                    slots[5],
                    slots[6],
                    thresholded[j]};
            std::copy(next, next + 7, slots);
        }
    }

    // The finalize pass runs one more picker call over the partial last batch
    // with no refinement. Its frame base is one frame higher than the main
    // loop because the native picker subtracts the frame count, not 43.
    const int remaining = envelopeSize - fullBatches * kEnvelopeBatch;
    if (remaining > 0) {
        const int first = fullBatches * kEnvelopeBatch;
        float sum = 0.0f;
        for (int i = 0; i < remaining; ++i) {
            sum = f32(sum + envelope[static_cast<std::size_t>(first + i)]);
        }
        const float scale = thresholdScale(sum, remaining);
        float thresholded[kEnvelopeBatch];
        for (int i = 0; i < remaining; ++i) {
            const float value = f32(envelope[static_cast<std::size_t>(first + i)] +
                    scale);
            thresholded[i] = value > 0.0f ? value : 0.0f;
        }
        for (int j = 0; j < remaining; ++j) {
            const int frame = first + j - 4;
            pushPeak(peaks, slots, frame, fullBatches, true, 0);
            const float next[7] = {
                    slots[4],
                    slots[2],
                    slots[3],
                    slots[0],
                    slots[5],
                    slots[6],
                    thresholded[j]};
            std::copy(next, next + 7, slots);
        }
    }

    return peaks;
}

std::vector<RefinedPeak> refinePeaks(
        const std::vector<PickedPeak>& peaks, const std::vector<float>& onsets) {
    std::vector<RefinedPeak> refined;
    refined.reserve(peaks.size());
    const int onsetCount = static_cast<int>(onsets.size());

    for (const PickedPeak& peak : peaks) {
        RefinedPeak out;
        out.flux = peak.flux;
        out.finalize = peak.finalize;
        out.frame = peak.frame;
        if (peak.finalize) {
            out.position = peak.rawPosition;
            refined.push_back(out);
            continue;
        }

        const double d = peak.rawPosition * kRefineScale;
        const long rounded = (d <= -0.5 || d >= 0.5)
                ? static_cast<long>(std::floor(d + 0.5))
                : 0L;
        const int c8 = static_cast<int>(((rounded % 43) + 43) % 43) * 8;
        int low = c8 - kRefineRadius;
        int high = c8 + kRefineRadius;
        if (high > kRefineLimit) {
            high = kRefineLimit;
        }
        if (low < 0) {
            low = 0;
        }
        int best = c8;
        float bestValue = 0.0f;
        const int base = peak.batch * kBurstsPerBatch;
        for (int k = low; k < high; ++k) {
            const int index = base + k;
            if (index < 0 || index >= onsetCount) {
                continue;
            }
            const float value = onsets[static_cast<std::size_t>(index)];
            if (value > bestValue) {
                bestValue = value;
                best = k;
            }
        }

        const float subProduct = f32(static_cast<double>(f32(best - c8 - 3)) *
                static_cast<double>(f32(32.0f)));
        const float basePosition = f32(static_cast<double>(f32(rounded)) *
                static_cast<double>(kFrameSeconds));
        float position = f32(static_cast<double>(subProduct) *
                        static_cast<double>(kInverseAnalysisRate) +
                static_cast<double>(basePosition));
        if (position <= 0.0f) {
            position = 0.0f;
        }

        out.position = position;
        out.subIndex = best;
        refined.push_back(out);
    }

    return refined;
}

} // namespace paramix::analysis
