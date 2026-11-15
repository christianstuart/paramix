#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "analyzer/analyzer.h"
#include "library/dao/analysisdao.h"
#include "util/math.h"
#include "util/performancetimer.h"
#include "util/sample.h"
#include "waveform/waveform.h"

// NOTS vrince some test to segment sound, to apply color in the waveform
// #define TEST_HEAT_MAP
#ifdef TEST_HEAT_MAP
class QImage;
#endif

class EngineFilterIIRBase;
class QSqlDatabase;

struct WaveformStride {
    WaveformStride(double samples, double averageSamples, int stemCount)
            : m_position(0),
              m_stemCount(stemCount),
              m_length(samples),
              m_averageLength(averageSamples),
              m_averagePosition(0),
              m_averageDivisor(0),
              m_stemSamples(0),
              m_postScaleConversion(static_cast<float>(
                      std::numeric_limits<unsigned char>::max())) {
        reset();
    }

    inline void reset() {
        m_position = 0;
        m_averageDivisor = 0;
        for (int i = 0; i < ChannelCount; ++i) {
            m_overallData[i] = 0.0f;
            m_averageOverallData[i] = 0.0f;
            SampleUtil::clear(m_filteredData[i], BandCount);
            SampleUtil::clear(m_averageFilteredData[i], BandCount);
            for (int stemIdx = 0; stemIdx < m_stemCount; ++stemIdx) {
                m_stemData[i][stemIdx] = 0.0f;
                m_stemEnergy[i][stemIdx] = 0.0;
                for (int band = 0; band < 3; ++band) {
                    m_stemFilteredData[i][stemIdx][band] = 0.0f;
                }
            }
        }
        m_stemSamples = 0;
    }

    inline void store(WaveformData* data, WaveformStemColumn* columns = nullptr) {
        for (int i = 0; i < ChannelCount; ++i) {
            WaveformData& datum = *(data + i);
            datum.filtered.all = static_cast<unsigned char>(std::min(255.0,
                    m_postScaleConversion * m_overallData[i] + 0.5));
            datum.filtered.low = static_cast<unsigned char>(std::min(255.0,
                    m_postScaleConversion * m_filteredData[i][Low] + 0.5));
            datum.filtered.mid = static_cast<unsigned char>(std::min(255.0,
                    m_postScaleConversion * m_filteredData[i][Mid] + 0.5));
            datum.filtered.high = static_cast<unsigned char>(std::min(255.0,
                    m_postScaleConversion * m_filteredData[i][High] + 0.5));
            for (int stemIdx = 0; stemIdx < m_stemCount; stemIdx++) {
                datum.stems[stemIdx] = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_stemData[i][stemIdx] + 0.5));
            }
        }
        if (columns) {
            storeStemDetail(columns);
        }

        m_averageDivisor++;
        // Reset the stride counters
        for (int i = 0; i < ChannelCount; ++i) {
            m_averageOverallData[i] += m_overallData[i];
            m_overallData[i] = 0.0f;
            for (int f = 0; f < BandCount; ++f) {
                m_averageFilteredData[i][f] += m_filteredData[i][f];
                m_filteredData[i][f] = 0.0f;
            }
            for (int stemIdx = 0; stemIdx < m_stemCount; ++stemIdx) {
                m_stemData[i][stemIdx] = 0.0f;
                m_stemEnergy[i][stemIdx] = 0.0;
                for (int band = 0; band < 3; ++band) {
                    m_stemFilteredData[i][stemIdx][band] = 0.0f;
                }
            }
        }
        m_stemSamples = 0;
    }

    /// Per-stem display detail for the column(s) just written. Peak and RMS are
    /// stored as amplitudes, the band energies as relative 2-bit values and the
    /// attack as the peak-to-RMS ratio, which stays high for transients and
    /// low for sustained material.
    inline void storeStemDetail(WaveformStemColumn* columns) {
        if (m_stemCount <= 0) {
            return;
        }
        for (int stemIdx = 0; stemIdx < m_stemCount; ++stemIdx) {
            const float peakLeft = m_stemData[Left][stemIdx];
            const float peakRight = m_stemData[Right][stemIdx];
            const float peak = std::max(peakLeft, peakRight);

            float bandPeak[3];
            for (int band = 0; band < 3; ++band) {
                bandPeak[band] = std::max(m_stemFilteredData[Left][stemIdx][band],
                        m_stemFilteredData[Right][stemIdx][band]);
            }
            const float bandSum = bandPeak[0] + bandPeak[1] + bandPeak[2];

            float rms = 0.0f;
            if (m_stemSamples > 0) {
                const double meanEnergy = (m_stemEnergy[Left][stemIdx] +
                                                  m_stemEnergy[Right][stemIdx]) /
                        (2.0 * m_stemSamples);
                rms = static_cast<float>(std::sqrt(meanEnergy));
            }

            columns->peak[stemIdx] = static_cast<unsigned char>(std::min(
                    255.0f, m_postScaleConversion * peak + 0.5f));
            columns->rms[stemIdx] = static_cast<unsigned char>(std::min(
                    255.0f, m_postScaleConversion * rms + 0.5f));

            unsigned char low = 0;
            unsigned char mid = 0;
            unsigned char high = 0;
            if (bandSum > 0.000001f) {
                low = static_cast<unsigned char>(
                        bandPeak[0] / bandSum * kStemBandMax + 0.5f);
                mid = static_cast<unsigned char>(
                        bandPeak[1] / bandSum * kStemBandMax + 0.5f);
                high = static_cast<unsigned char>(
                        bandPeak[2] / bandSum * kStemBandMax + 0.5f);
            }

            // Transient strength: a peak that stands far above the RMS of the
            // same stride is an attack (kick, snare, pluck), while sustained
            // material keeps the two close together.
            unsigned char attack = 0;
            if (rms > 0.000001f) {
                const float ratio = math_clamp((peak / rms - 1.5f) / 4.0f, 0.0f, 1.0f);
                attack = static_cast<unsigned char>(ratio * kStemBandMax + 0.5f);
            }

            columns->bands[stemIdx] = packStemBands(low, mid, high, attack);
        }
    }

    inline void averageStore(WaveformData* data) {
        if (m_averageDivisor) {
            for (int i = 0; i < ChannelCount; ++i) {
                WaveformData& datum = *(data + i);
                datum.filtered.all = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_averageOverallData[i] / m_averageDivisor + 0.5));
                datum.filtered.low = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_averageFilteredData[i][Low] /
                                        m_averageDivisor +
                                0.5));
                datum.filtered.mid = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_averageFilteredData[i][Mid] /
                                        m_averageDivisor +
                                0.5));
                datum.filtered.high = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_averageFilteredData[i][High] /
                                        m_averageDivisor +
                                0.5));
            }
        } else {
            // This is the case if The Overview Waveform has more samples than the detailed waveform
            for (int i = 0; i < ChannelCount; ++i) {
                WaveformData& datum = *(data + i);
                datum.filtered.all = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_overallData[i] + 0.5));
                datum.filtered.low = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_filteredData[i][Low] + 0.5));
                datum.filtered.mid = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_filteredData[i][Mid] + 0.5));
                datum.filtered.high = static_cast<unsigned char>(std::min(255.0,
                        m_postScaleConversion * m_filteredData[i][High] + 0.5));
            }
        }

        m_averageDivisor = 0;
        for (int i = 0; i < ChannelCount; ++i) {
            m_averageOverallData[i] = 0.0f;
            for (int f = 0; f < BandCount; ++f) {
                m_averageFilteredData[i][f] = 0.0f;
            }
        }
    }

    int m_position;
    int m_stemCount;
    double m_length;
    double m_averageLength;
    int m_averagePosition;
    int m_averageDivisor;

    float m_overallData[ChannelCount];
    float m_filteredData[ChannelCount][BandCount];
    float m_stemData[ChannelCount][mixxx::kMaxSupportedStems];
    // Per-stem low/mid/high band peaks and energy for RMS/attack detection.
    float m_stemFilteredData[ChannelCount][mixxx::kMaxSupportedStems][3];
    double m_stemEnergy[ChannelCount][mixxx::kMaxSupportedStems];
    int m_stemSamples;

    float m_averageOverallData[ChannelCount];
    float m_averageFilteredData[ChannelCount][BandCount];

    float m_postScaleConversion;
};

class AnalyzerWaveform : public Analyzer {
  public:
    AnalyzerWaveform(
            UserSettingsPointer pConfig,
            const QSqlDatabase& dbConnection);
    ~AnalyzerWaveform() override;

    bool initialize(const AnalyzerTrack& track,
            mixxx::audio::SampleRate sampleRate,
            mixxx::audio::ChannelCount channelCount,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* buffer, SINT count) override;
    void storeResults(TrackPointer tio) override;
    void cleanup() override;

  private:
    bool shouldAnalyze(TrackPointer tio) const;

    void storeCurrentStridePower();
    void resetCurrentStride();

    void createFilters(mixxx::audio::SampleRate sampleRate);
    void destroyFilters();
    void storeIfGreater(float* pDest, float source);

    mutable AnalysisDao m_analysisDao;

    WaveformPointer m_waveform;
    WaveformPointer m_waveformSummary;
    WaveformData* m_waveformData;
    WaveformData* m_waveformSummaryData;

    WaveformStride m_stride;

    int m_currentStride;
    int m_currentSummaryStride;
    mixxx::audio::ChannelCount m_channelCount;

    struct Filters {
        std::unique_ptr<EngineFilterIIRBase> low;
        std::unique_ptr<EngineFilterIIRBase> mid;
        std::unique_ptr<EngineFilterIIRBase> high;
    };

    Filters m_filters;
    // Per-stem band filters (stem tracks only). Stems are analysed in the same
    // pass as the mix, so the display can colour every lane by its own
    // spectrum without a second analysis pass.
    std::vector<Filters> m_stemFilters;

    struct Buffers {
        std::vector<float> low;
        std::vector<float> mid;
        std::vector<float> high;

        SINT size;

        Buffers()
                : low(),
                  mid(),
                  high(),
                  size(0) {
        }
    };

    Buffers m_buffers;
    // Scratch space for the interleaved stereo signal of a single stem.
    std::vector<std::vector<float>> m_stemInput;
    std::vector<Buffers> m_stemBuffers;

    PerformanceTimer m_timer;

#ifdef TEST_HEAT_MAP
    QImage* test_heatMap;
#endif
};
