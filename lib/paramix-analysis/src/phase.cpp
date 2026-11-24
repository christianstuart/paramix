#include "phase.h"

#include <cmath>

#include "float32.h"

namespace paramix::analysis {

namespace {
constexpr float kPeriodConstant = 60.0f;
} // namespace

float circularMean(const std::vector<float>& positions, float bpm, float phase) {
    if (!(bpm > 0.0f)) {
        return phase;
    }
    const float period = f32(kPeriodConstant / bpm);
    const float inverse = f32(f32(1.0f) / period);
    const float base = f32(phase * inverse);

    float sumWeight = 0.0f;
    float sumWeighted = 0.0f;
    for (float position : positions) {
        const float x = f32(std::fmod(static_cast<double>(position),
                static_cast<double>(period)));
        float t = f32(f32(x * inverse) - base);
        if (t < -0.5f) {
            t = f32(t + 1.0f);
        }
        if (!(t < 0.5f)) {
            t = f32(t - 1.0f);
        }
        float u = f32(std::fabs(t) * 64.0f);
        if (u > 1.0f) {
            u = 1.0f;
        }
        const float oneMinus = f32(1.0f - u);
        const float weight = f32(oneMinus * oneMinus);
        sumWeight = f32(sumWeight + weight);
        sumWeighted = f32(sumWeighted + f32(weight * t));
    }
    if (sumWeight > 0.0f) {
        return f32(f32(f32(sumWeighted / sumWeight) + base) * period);
    }
    return f32(period * base);
}

float combPhase(const std::vector<float>& positions,
        const std::vector<float>& weights,
        float bpm) {
    const std::size_t n = positions.size();
    if (n == 0 || !(bpm > 0.0f) || weights.size() < n) {
        return 0.0f;
    }
    const double period = static_cast<double>(f32(kPeriodConstant / bpm));
    // A 30 ms taper covers the attack of a kick or snare without reaching the
    // neighbouring 16th note at typical dance tempos.
    constexpr double kWindow = 0.030;
    constexpr int kSteps = 2048;

    double bestPhase = 0.0;
    double bestScore = -1.0;
    for (int step = 0; step < kSteps; ++step) {
        const double phase = static_cast<double>(step) * period / kSteps;
        double score = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double position = static_cast<double>(positions[i]);
            const double index = std::round((position - phase) / period);
            const double distance = std::fabs(position - (phase + index * period));
            if (distance <= kWindow) {
                score += static_cast<double>(weights[i]) *
                        (1.0 - distance / kWindow);
            }
        }
        if (score > bestScore) {
            bestScore = score;
            bestPhase = phase;
        }
    }
    return f32(bestPhase);
}

PhaseResult estimatePhase(const std::vector<double>& positions,
        const std::vector<float>& weights,
        const std::vector<float>& bassWeights,
        float bpm) {
    PhaseResult result;
    const int n = static_cast<int>(positions.size());
    if (n <= 0 || !(bpm > 0.0f) || weights.size() < positions.size() ||
            bassWeights.size() < positions.size()) {
        return result;
    }

    std::vector<float> positionsF(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        positionsF[static_cast<std::size_t>(i)] =
                f32(positions[static_cast<std::size_t>(i)]);
    }

    const float period = f32(kPeriodConstant / bpm);
    const float phase0 =
            circularMean(positionsF, bpm, combPhase(positionsF, weights, bpm));

    std::vector<float> salience(static_cast<std::size_t>(n));
    float previousSalience = 0.0f;
    float previousPosition = 0.0f;
    for (int i = 0; i < n; ++i) {
        float current = bassWeights[static_cast<std::size_t>(i)];
        const float previous = i == 0
                ? 0.0f
                : bassWeights[static_cast<std::size_t>(i - 1)];
        const float next = i < n - 1
                ? bassWeights[static_cast<std::size_t>(i + 1)]
                : 0.0f;
        if (!(current > previous) || !(current > next)) {
            current = 0.0f;
        }
        const float decay = f32(std::exp(f32(f32(positionsF[static_cast<std::size_t>(i)] -
                                                     previousPosition) *
                f32(-0.02f))));
        const float term = f32(decay * previousSalience);
        const float difference = f32(current - term);
        salience[static_cast<std::size_t>(i)] = difference > 0.0f ? difference : 0.0f;
        previousSalience = current > term ? current : term;
        previousPosition = positionsF[static_cast<std::size_t>(i)];
    }

    float maximum = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float value = salience[static_cast<std::size_t>(i)];
        if (value > maximum) {
            maximum = value;
        }
    }
    for (int i = 0; i < n; ++i) {
        const float value = salience[static_cast<std::size_t>(i)];
        salience[static_cast<std::size_t>(i)] = (value == maximum) ? maximum : 0.0f;
    }

    const float period4 = f32(f32(4.0f) * period);
    const float inversePeriod = f32(f32(1.0f) / period);
    float scores[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float candidates[4] = {phase0, 0.0f, 0.0f, 0.0f};
    for (int c = 0; c < 4; ++c) {
        const float candidate = f32(f32(f32(c) * period) + phase0);
        candidates[c] = candidate;
        float accumulator = 0.0f;
        for (int k = 0; k < n; ++k) {
            const float wrapped = f32(std::fmod(
                    static_cast<double>(positionsF[static_cast<std::size_t>(k)]),
                    static_cast<double>(period4)));
            float x = f32(wrapped - candidate);
            if (x < f32(f32(-2.0f) * period)) {
                x = f32(x + period4);
            }
            if (f32(f32(2.0f) * period) < x) {
                x = f32(x - period4);
            }
            float t = f32(x * inversePeriod);
            t = f32(1.0f - std::fabs(t));
            if (t <= 0.0f) {
                t = 0.0f;
            }
            accumulator = f32(accumulator +
                    f32(f32(t * t) * salience[static_cast<std::size_t>(k)]));
        }
        scores[c] = accumulator;
    }

    int bestCandidate = 0;
    for (int c = 1; c < 4; ++c) {
        if (scores[c] > scores[bestCandidate]) {
            bestCandidate = c;
        }
    }
    float second = 0.0f;
    for (int c = 0; c < 4; ++c) {
        if (c != bestCandidate && scores[c] > second) {
            second = scores[c];
        }
    }

    result.ok = true;
    result.anchor = circularMean(positionsF, bpm, candidates[bestCandidate]);
    result.phase0 = phase0;
    result.candidate = bestCandidate;
    result.bestScore = scores[bestCandidate];
    result.secondScore = second;
    return result;
}

} // namespace paramix::analysis
