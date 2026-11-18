#include "onset.h"

#include "fft.h"
#include "float32.h"
#include "logmag.h"

namespace paramix::analysis {

BurstAnalyzer::BurstAnalyzer()
        : m_window(makeWindowTable()) {
}

void BurstAnalyzer::processBurst(const float* samples, int burstIndex) {
    float windowed[kWindowLength];
    for (int i = 0; i < kWindowLength; ++i) {
        windowed[i] = f32(samples[i] * m_window[static_cast<std::size_t>(i)]);
    }

    double re[kWindowLength];
    double im[kWindowLength];
    realFft512(windowed, re, im);

    float magnitude[kSpectrumBins];
    magnitudeSpectrum(re, im, magnitude);

    float logMagnitude[kSpectrumBins];
    for (int k = 0; k < kSpectrumBins; ++k) {
        logMagnitude[k] = log10p1(magnitude[k]);
    }

    // Bass envelope for the downbeat salience stage.
    m_bass.push_back(f32(f32(logMagnitude[1] + logMagnitude[0]) +
            logMagnitude[2]));

    // Fine flux: every burst against the previous burst.
    float rectified[kFluxBins];
    for (int k = 0; k < kFluxBins; ++k) {
        const float difference = f32(logMagnitude[k] - m_fineHistory[k]);
        rectified[k] = difference > 0.0f ? difference : 0.0f;
    }
    m_onsets.push_back(sumLanes4(rectified, kFluxBins));
    for (int k = 0; k < kFluxBins; ++k) {
        m_fineHistory[k] = logMagnitude[k];
    }

    // Coarse flux: every 8th burst against the burst 8 back. The envelope is
    // the mean of consecutive coarse flux values.
    if ((burstIndex & 7) == 0) {
        for (int k = 0; k < kFluxBins; ++k) {
            const float difference = f32(logMagnitude[k] - m_coarseHistory[k]);
            rectified[k] = difference > 0.0f ? difference : 0.0f;
        }
        const float coarseFlux = sumLanes4(rectified, kFluxBins);
        const float ring = f32(f32(coarseFlux + m_previousCoarse) * f32(0.5f));
        m_previousCoarse = coarseFlux;
        m_envelope.push_back(ring);
        for (int k = 0; k < kFluxBins; ++k) {
            m_coarseHistory[k] = logMagnitude[k];
        }
    }
}

} // namespace paramix::analysis
