#include "waveform/waveform.h"

#include <QtDebug>
#include <algorithm>
#include <array>

#include "analyzer/constants.h"
#include "engine/engine.h"
#include "proto/waveform.pb.h"
#include "util/math.h"

using namespace mixxx::track;

// Return the smallest power of 2 which is greater than the desired size when
// squared.
int computeTextureStride(int size) {
    int stride = 256;
    while (stride * stride < size) {
        stride *= 2;
    }
    return stride;
}

Waveform::Waveform(const QByteArray& data)
        : m_id(-1),
          m_saveState(SaveState::NotSaved),
          m_dataSize(0),
          m_visualSampleRate(0),
          m_audioVisualRatio(0),
          m_textureStride(computeTextureStride(0)),
          m_completion(-1) {
    readByteArray(data);
}

Waveform::Waveform(
        int audioSampleRate,
        SINT frameLength,
        int desiredVisualSampleRate,
        int maxVisualSamples,
        int stemCount)
        : m_id(-1),
          m_saveState(SaveState::NotSaved),
          m_dataSize(0),
          m_visualSampleRate(0),
          m_audioVisualRatio(0),
          m_textureStride(1024),
          m_completion(-1),
          m_stemCount(stemCount) {
    int numberOfVisualSamples = 0;
    if (audioSampleRate > 0) {
        if (maxVisualSamples == -1) {
            // Waveform
            if (desiredVisualSampleRate < audioSampleRate) {
                m_visualSampleRate =
                        static_cast<double>(desiredVisualSampleRate);
            } else {
                m_visualSampleRate = static_cast<double>(audioSampleRate);
            }
        } else {
            // Waveform Summary (Overview)
            if (frameLength > maxVisualSamples / mixxx::kAnalysisChannels) {
                m_visualSampleRate = static_cast<double>(audioSampleRate) *
                        maxVisualSamples / mixxx::kAnalysisChannels / frameLength;
            } else {
                m_visualSampleRate = audioSampleRate;
            }
        }
        m_audioVisualRatio = (double)audioSampleRate / (double)m_visualSampleRate;
        numberOfVisualSamples =
                static_cast<int>(frameLength / m_audioVisualRatio *
                        mixxx::kAnalysisChannels) +
                1;
        numberOfVisualSamples += numberOfVisualSamples % 2;
    }
    assign(numberOfVisualSamples);
    setCompletion(0);
}

Waveform::~Waveform() {
}

QByteArray Waveform::toByteArray() const {
    io::Waveform waveform;
    waveform.set_visual_sample_rate(m_visualSampleRate);
    waveform.set_audio_visual_ratio(m_audioVisualRatio);

    io::Waveform::Signal* all = waveform.mutable_signal_all();
    io::Waveform::FilteredSignal* filtered = waveform.mutable_signal_filtered();

    QList<io::Waveform::Signal*> stems;
    for (int i = 0; i < m_stemCount; i++) {
        stems.append(waveform.add_signal_stems());
    }
    // TODO(rryan) get the actual cutoff values from analyzerwaveform.cpp so
    // that if they change we don't have to remember to update these.

    // Frequency cutoffs for butterworth filters:
    // filtered->set_low_cutoff_frequency(200);
    // filtered->set_mid_low_cutoff_frequency(200);
    // filtered->set_mid_high_cutoff_frequency(2000);
    // filtered->set_high_cutoff_frequency(2000);

    // Frequency cutoff for bessel_lowpass4
    filtered->set_low_cutoff_frequency(600);
    // Frequency cutoff for bessel_bandpass
    filtered->set_mid_low_cutoff_frequency(600);
    filtered->set_mid_high_cutoff_frequency(4000);
    // Frequency cutoff for bessel_highpass4
    filtered->set_high_cutoff_frequency(4000);

    io::Waveform::Signal* low = filtered->mutable_low();
    io::Waveform::Signal* mid = filtered->mutable_mid();
    io::Waveform::Signal* high = filtered->mutable_high();

    // TODO(vrince) set max/min for each signal
    all->set_units(io::Waveform::RMS);
    all->set_channels(mixxx::kEngineChannelOutputCount);
    low->set_units(io::Waveform::RMS);
    low->set_channels(mixxx::kEngineChannelOutputCount);
    mid->set_units(io::Waveform::RMS);
    mid->set_channels(mixxx::kEngineChannelOutputCount);
    high->set_units(io::Waveform::RMS);
    high->set_channels(mixxx::kEngineChannelOutputCount);

    int dataSize = getDataSize();
    for (int i = 0; i < dataSize; ++i) {
        const WaveformData& datum = m_data.at(i);
        all->add_value(datum.filtered.all);
        low->add_value(datum.filtered.low);
        mid->add_value(datum.filtered.mid);
        high->add_value(datum.filtered.high);
    }

    int stemIdx = 0;
    for (auto& stem : stems) {
        stem->set_units(io::Waveform::RMS);
        stem->set_channels(mixxx::kEngineChannelOutputCount);
        for (int i = 0; i < dataSize; ++i) {
            const WaveformData& datum = m_data.at(i);
            stem->add_value(datum.stems[stemIdx]);
        }
        stemIdx++;
    }

    // Per-stem band detail (additive, optional field). Older versions simply
    // ignore it, and summaries written by older versions do not have it.
    if (hasStemColumns()) {
        const WaveformStemColumn* pColumns = getStemColumnData(0);
        const int columnCount = getStemColumnCount(0);
        for (int stemIdx = 0; stemIdx < m_stemCount; ++stemIdx) {
            io::Waveform::StemDetail* pDetail = waveform.add_signal_stem_details();
            for (int i = 0; i < columnCount; ++i) {
                pDetail->add_rms(pColumns[i].rms[stemIdx]);
                pDetail->add_bands(pColumns[i].bands[stemIdx]);
            }
        }
    }

    qDebug() << "Writing waveform from byte array:"
             << "dataSize" << dataSize
             << "stemCount" << m_stemCount
             << "allSignalSize" << all->value_size()
             << "visualSampleRate" << waveform.visual_sample_rate()
             << "audioVisualRatio" << waveform.audio_visual_ratio();

    std::string output;
    waveform.SerializeToString(&output);
    return QByteArray(output.data(), static_cast<int>(output.length()));
}

void Waveform::readByteArray(const QByteArray& data) {
    if (data.isNull()) {
        return;
    }

    io::Waveform waveform;

    if (!waveform.ParseFromArray(data.constData(), data.size())) {
        qDebug() << "ERROR: Could not parse Waveform from QByteArray of size "
                 << data.size();
        return;
    }

    if (!waveform.has_visual_sample_rate() ||
            !waveform.has_audio_visual_ratio() ||
            !waveform.has_signal_all() ||
            !waveform.has_signal_filtered() ||
            !waveform.signal_filtered().has_low() ||
            !waveform.signal_filtered().has_mid() ||
            !waveform.signal_filtered().has_high()) {
        qDebug() << "ERROR: Waveform proto is missing key data. Skipping.";
        return;
    }

    const io::Waveform::Signal& all = waveform.signal_all();
    const io::Waveform::Signal& low = waveform.signal_filtered().low();
    const io::Waveform::Signal& mid = waveform.signal_filtered().mid();
    const io::Waveform::Signal& high = waveform.signal_filtered().high();

    qDebug() << "Reading waveform from byte array:"
             << "allSignalSize" << all.value_size()
             << "visualSampleRate" << waveform.visual_sample_rate()
             << "audioVisualRatio" << waveform.audio_visual_ratio()
             << "stemSignalSize" << waveform.signal_stems_size();

    resize(all.value_size());

    int dataSize = getDataSize();
    if (all.value_size() != dataSize) {
        qDebug() << "ERROR: Couldn't resize Waveform to" << all.value_size()
                 << "while reading.";
        resize(0);
        m_saveState = SaveState::NotSaved;
        return;
    }

    m_visualSampleRate = waveform.visual_sample_rate();
    m_audioVisualRatio = waveform.audio_visual_ratio();
    if (low.value_size() != dataSize ||
            mid.value_size() != dataSize ||
            high.value_size() != dataSize) {
        qDebug() << "WARNING: Filtered data size does not match all-signal size.";
    }

    // TODO(XXX) If non-RMS, convert but since we only save RMS today we can add
    // this later.
    bool low_valid = low.units() == io::Waveform::RMS;
    bool mid_valid = mid.units() == io::Waveform::RMS;
    bool high_valid = high.units() == io::Waveform::RMS;
    for (int i = 0; i < dataSize; ++i) {
        m_data[i].filtered.all = static_cast<unsigned char>(all.value(i));
        bool use_low = low_valid && i < low.value_size();
        bool use_mid = mid_valid && i < mid.value_size();
        bool use_high = high_valid && i < high.value_size();
        m_data[i].filtered.low = use_low ? static_cast<unsigned char>(low.value(i)) : 0;
        m_data[i].filtered.mid = use_mid ? static_cast<unsigned char>(mid.value(i)) : 0;
        m_data[i].filtered.high = use_high ? static_cast<unsigned char>(high.value(i)) : 0;
    }
    m_stemCount = waveform.signal_stems_size();
    for (int stemIdx = 0; stemIdx < m_stemCount; ++stemIdx) {
        const io::Waveform::Signal& stem = waveform.signal_stems(stemIdx);
        if (stem.units() == io::Waveform::RMS && stem.value_size() > 0) {
            for (int i = 0; i < std::min(dataSize, stem.value_size()); ++i) {
                m_data[i].stems[stemIdx] = static_cast<unsigned char>(stem.value(i));
            }
        }
        if (stem.units() != io::Waveform::RMS || dataSize > stem.value_size()) {
            for (int i = stem.value_size(); i < dataSize; ++i) {
                m_data[i].stems[stemIdx] = 0;
            }
        }
    }

    // Per-stem band detail was added after the first waveform format, so its
    // absence only means "this summary predates it": the stem lanes are then
    // rendered without per-band colour until the track is re-analysed.
    const bool hasStemDetails = waveform.signal_stem_details_size() == m_stemCount &&
            m_stemCount > 0 &&
            waveform.signal_stem_details(0).rms_size() >= dataSize / 2;
    resetStemColumnLevels(hasStemDetails ? dataSize / 2 : 0);
    if (hasStemDetails) {
        WaveformStemColumn* pColumns = stemColumnDataForWrite();
        const int columnCount = getStemColumnCount(0);
        for (int stemIdx = 0; stemIdx < m_stemCount; ++stemIdx) {
            const io::Waveform::StemDetail& detail = waveform.signal_stem_details(stemIdx);
            const int rmsSize = std::min(columnCount, detail.rms_size());
            const int bandsSize = std::min(columnCount, detail.bands_size());
            for (int i = 0; i < columnCount; ++i) {
                pColumns[i].rms[stemIdx] = i < rmsSize
                        ? static_cast<unsigned char>(detail.rms(i))
                        : 0;
                pColumns[i].bands[stemIdx] = i < bandsSize
                        ? static_cast<unsigned char>(detail.bands(i))
                        : 0;
            }
        }
        // The peak bytes are stored with the stem signals above; fold them into
        // the level-0 columns so the renderer has one place to look.
        for (int i = 0; i < columnCount; ++i) {
            for (int stemIdx = 0; stemIdx < m_stemCount; ++stemIdx) {
                pColumns[i].peak[stemIdx] = math_max(m_data[i * 2].stems[stemIdx],
                        m_data[i * 2 + 1].stems[stemIdx]);
            }
        }
        buildStemColumnLevels();
    }

    m_completion = dataSize;
    m_saveState = SaveState::Saved;
}

void Waveform::resize(int size) {
    m_dataSize = size;
    m_textureStride = computeTextureStride(size);
    m_data.resize(m_textureStride * m_textureStride);
    resetStemColumnLevels(0);
}

void Waveform::assign(int size) {
    m_dataSize = size;
    m_textureStride = computeTextureStride(size);
    m_data.assign(m_textureStride * m_textureStride, {});
    m_saveState = SaveState::SavePending;
    // Allocate the base level so the analyzer can fill it in place.
    resetStemColumnLevels(m_stemCount > 0 ? size / 2 : 0);
}

void Waveform::resetStemColumnLevels(int columns) {
    m_stemColumnLevels.clear();
    for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
        m_stemPeak[stemIdx] = 0;
    }
    m_stemPeakReference = 0;
    if (columns > 0) {
        m_stemColumnLevels.emplace_back(columns);
    }
}

void Waveform::buildStemColumnLevels() {
    if (m_stemColumnLevels.empty()) {
        return;
    }

    // Move the base level out before adding levels, so that growing the level
    // list can never invalidate the columns we aggregate from.
    std::vector<std::vector<WaveformStemColumn>> levels;
    levels.reserve(16);
    levels.push_back(std::move(m_stemColumnLevels.front()));
    m_stemColumnLevels.clear();

    const std::size_t baseSize = levels[0].size();

    // Per-stem reference level: the 95th percentile of the column peaks, not
    // the absolute maximum. A single outlier (one huge hit, a click) would
    // otherwise scale the whole lane down and leave typical loud sections
    // looking thin. A 256-bin histogram makes the percentile one cheap pass.
    for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
        m_stemPeak[stemIdx] = 0;
    }
    std::array<std::array<int, 256>, mixxx::kMaxSupportedStems> peakHistogram{};
    for (const WaveformStemColumn& column : levels[0]) {
        for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
            ++peakHistogram[stemIdx][column.peak[stemIdx]];
        }
    }
    const int totalColumns = static_cast<int>(levels[0].size());
    if (totalColumns > 0) {
        const int wanted = (totalColumns * 95 + 99) / 100;
        for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
            int seen = 0;
            for (int bucket = 255; bucket >= 0; --bucket) {
                seen += peakHistogram[stemIdx][bucket];
                if (seen >= wanted) {
                    m_stemPeak[stemIdx] = bucket;
                    break;
                }
            }
        }
    }
    m_stemPeakReference = 0;
    for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
        m_stemPeakReference = math_max(m_stemPeakReference, m_stemPeak[stemIdx]);
    }

    // Aggregation levels: stride 2, 4, 8, ... until a level has a single column.
    // Peaks and RMS use max (transients and loudness must survive), the band
    // and attack fields use the mean of the covered columns.
    std::size_t stride = 2;
    while (stride <= baseSize) {
        const std::size_t count = (baseSize + stride - 1) / stride;
        std::vector<WaveformStemColumn> level(count, WaveformStemColumn{});
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t begin = i * stride;
            const std::size_t end = std::min(baseSize, begin + stride);
            int bandSum[mixxx::kMaxSupportedStems][4]{};
            for (std::size_t j = begin; j < end; ++j) {
                for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
                    level[i].peak[stemIdx] = math_max(level[i].peak[stemIdx],
                            levels[0][j].peak[stemIdx]);
                    level[i].rms[stemIdx] = math_max(level[i].rms[stemIdx],
                            levels[0][j].rms[stemIdx]);
                    for (int field = 0; field < 4; ++field) {
                        bandSum[stemIdx][field] += stemBandField(
                                levels[0][j].bands[stemIdx], field);
                    }
                }
            }
            const int divisor = static_cast<int>(end - begin);
            for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
                level[i].bands[stemIdx] = packStemBands(
                        static_cast<unsigned char>(bandSum[stemIdx][0] / divisor),
                        static_cast<unsigned char>(bandSum[stemIdx][1] / divisor),
                        static_cast<unsigned char>(bandSum[stemIdx][2] / divisor),
                        static_cast<unsigned char>(bandSum[stemIdx][3] / divisor));
            }
        }
        levels.push_back(std::move(level));
        stride *= 2;
    }

    m_stemColumnLevels = std::move(levels);
}

void Waveform::dump() const {
    qDebug() << "Waveform" << this
             << "size(" + QString::number(getDataSize()) + ")"
             << "stems(" + QString::number(m_stemCount) + ")"
             << "textureStride(" + QString::number(m_textureStride) + ")"
             << "completion(" + QString::number(getCompletion()) + ")"
             << "visualSampleRate(" + QString::number(m_visualSampleRate) + ")"
             << "audioVisualRatio(" + QString::number(m_audioVisualRatio) + ")";
}
