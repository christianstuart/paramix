#pragma once

#include <QAtomicInt>
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QString>
#include <vector>

#include "analyzer/constants.h"
#include "audio/signalinfo.h"
#include "util/class.h"

enum BandIndex { AllBand = 0,
    Low = 1,
    Mid = 2,
    High = 3,
    BandCount = 4 };
enum ChannelIndex { Left = 0, Right = 1, ChannelCount = 2};

struct WaveformFilteredData {
    unsigned char low;
    unsigned char mid;
    unsigned char high;
    unsigned char all;
};

struct WaveformData {
    WaveformFilteredData filtered;
    unsigned char stems[mixxx::kMaxSupportedStems];
};

/// Per-stem display detail for one visual frame (or for a group of frames when
/// it comes from a lower resolution level of the detail pyramid).
///
/// 12 bytes per column: 4 stems x (peak, rms, packed bands). The packed band
/// byte encodes the relative low/mid/high energy and the attack strength in two
/// bits each, which is plenty for per-column colour and alpha modulation while
/// keeping the stored summary compact (see Waveform::toByteArray()).
struct WaveformStemColumn {
    /// Max of the absolute peaks of both channels, 0..255.
    unsigned char peak[mixxx::kMaxSupportedStems];
    /// RMS amplitude, 0..255.
    unsigned char rms[mixxx::kMaxSupportedStems];
    /// Bit-packed: bits 0-1 low, 2-3 mid, 4-5 high, 6-7 attack.
    unsigned char bands[mixxx::kMaxSupportedStems];
};

/// Number of bits used for each of the four packed band/attack fields.
constexpr int kStemBandBits = 2;
constexpr int kStemBandMax = (1 << kStemBandBits) - 1;

inline unsigned char stemBandField(unsigned char bands, int fieldIndex) {
    return static_cast<unsigned char>(
            (bands >> (fieldIndex * kStemBandBits)) & kStemBandMax);
}

inline unsigned char packStemBands(
        unsigned char low, unsigned char mid, unsigned char high, unsigned char attack) {
    return static_cast<unsigned char>((low & kStemBandMax) |
            ((mid & kStemBandMax) << kStemBandBits) |
            ((high & kStemBandMax) << (2 * kStemBandBits)) |
            ((attack & kStemBandMax) << (3 * kStemBandBits)));
}

class Waveform {
  public:
    enum class SaveState {
        NotSaved = 0,
        SavePending,
        Saved
    };

    explicit Waveform(const QByteArray& pData = QByteArray());
    Waveform(
            int audioSampleRate,
            SINT frameLength,
            int desiredVisualSampleRate,
            int maxVisualSamples,
            int stemCount);

    virtual ~Waveform();

    int getId() const {
        const auto locker = QMutexLocker(&m_mutex);
        return m_id;
    }

    void setId(int id) {
        const auto locker = QMutexLocker(&m_mutex);
        m_id = id;
    }

    QString getVersion() const {
        const auto locker = QMutexLocker(&m_mutex);
        return m_version;
    }

    void setVersion(const QString& version) {
        const auto locker = QMutexLocker(&m_mutex);
        m_version = version;
    }

    QString getDescription() const {
        const auto locker = QMutexLocker(&m_mutex);
        return m_description;
    }

    void setDescription(const QString& description) {
        const auto locker = QMutexLocker(&m_mutex);
        m_description = description;
    }

    QByteArray toByteArray() const;

    SaveState saveState() const {
        return m_saveState;
    }

    // AnalysisDAO needs to be able to change the state to savePending when finished
    // so we mark this as const and m_saveState mutable.
    void setSaveState(SaveState eState) const {
        m_saveState = eState;
    }

    // We do not lock the mutex since m_audioVisualRatio is not changed after
    // the constructor runs.
    double getAudioVisualRatio() const {
        return m_audioVisualRatio;
    }

    // Atomically lookup the completion of the waveform. Represents the number
    // of data elements that have been processed out of dataSize.
    int getCompletion() const {
        return m_completion.loadAcquire();
    }
    void setCompletion(int completion) {
        m_completion = completion;
    }

    // We do not lock the mutex since m_textureStride is not changed after
    // the constructor runs.
    inline int getTextureStride() const { return m_textureStride; }

    // We do not lock the mutex since m_data is not resized after the
    // constructor runs.
    inline int getTextureSize() const { return static_cast<int>(m_data.size()); }

    // Atomically get the number of data elements in this Waveform. We do not
    // lock the mutex since m_dataSize is not changed after the constructor
    // runs.
    inline int getDataSize() const { return m_dataSize; }

    inline const WaveformData& get(int i) const { return m_data[i];}
    inline unsigned char getLow(int i) const { return m_data[i].filtered.low;}
    inline unsigned char getMid(int i) const { return m_data[i].filtered.mid;}
    inline unsigned char getHigh(int i) const { return m_data[i].filtered.high;}
    inline unsigned char getAll(int i) const { return m_data[i].filtered.all;}

    // We do not lock the mutex since m_data is not resized after the
    // constructor runs.
    WaveformData* data() { return &m_data[0];}

    // We do not lock the mutex since m_data is not resized after the
    // constructor runs.
    const WaveformData* data() const { return &m_data[0];}

    bool hasStem() const {
        return m_stemCount > 0;
    }

    /// Per-stem display detail. Level 0 has one WaveformStemColumn per visual
    /// frame; level i aggregates 2^i frames. The pyramid is built once, either
    /// while reading a stored summary or when the analyzer finishes, and is
    /// never recomputed while the renderer scrolls.
    bool hasStemColumns() const {
        return !m_stemColumnLevels.empty() && !m_stemColumnLevels.front().empty();
    }

    int getStemColumnLevelCount() const {
        return static_cast<int>(m_stemColumnLevels.size());
    }

    static int stemColumnStride(int level) {
        return 1 << level;
    }

    int getStemColumnCount(int level) const {
        if (level < 0 || level >= getStemColumnLevelCount()) {
            return 0;
        }
        return static_cast<int>(m_stemColumnLevels[level].size());
    }

    const WaveformStemColumn* getStemColumnData(int level) const {
        if (level < 0 || level >= getStemColumnLevelCount()) {
            return nullptr;
        }
        return m_stemColumnLevels[level].data();
    }

    /// Writable access to the base level, used by the analyzer.
    WaveformStemColumn* stemColumnDataForWrite() {
        if (m_stemColumnLevels.empty()) {
            return nullptr;
        }
        return m_stemColumnLevels.front().data();
    }

    /// Whole-track peak per stem (0..255) and the peak of the loudest stem.
    /// Used to derive one shared visual reference across lanes.
    int getStemPeak(int stemIdx) const {
        return m_stemPeak[stemIdx];
    }

    int getStemPeakReference() const {
        return m_stemPeakReference;
    }

    /// (Re)build aggregation levels and the whole-track peaks from level 0.
    void buildStemColumnLevels();

    void dump() const;

  private:
    void readByteArray(const QByteArray& data);
    void resize(int size);
    void assign(int size);
    void resetStemColumnLevels(int columns);

    inline WaveformData& at(int i) { return m_data[i];}
    inline unsigned char& low(int i) { return m_data[i].filtered.low;}
    inline unsigned char& mid(int i) { return m_data[i].filtered.mid;}
    inline unsigned char& high(int i) { return m_data[i].filtered.high;}
    inline unsigned char& all(int i) { return m_data[i].filtered.all;}
    double getVisualSampleRate() const { return m_visualSampleRate; }

    // If stored in the database, the ID of the waveform.
    int m_id;
    // mutable since AnalysisDAO needs to be able to set the waveform as saved.
    mutable SaveState m_saveState;
    QString m_version;
    QString m_description;

    // The size of the waveform data stored in m_data. Not allowed to change
    // after the constructor runs.
    int m_dataSize;
    // The vector storing the waveform data. It is potentially larger than
    // m_dataSize since it includes padding for uploading the entire waveform as
    // a texture in the GLSL renderer. The size is not allowed to change after
    // the constructor runs. We use a std::vector to avoid the cost of bounds
    // checking when accessing the vector.
    // TODO(XXX): In the future we should switch to QVector and use the raw data
    // pointer when performance matters.
    std::vector<WaveformData> m_data;
    // Not allowed to change after the constructor runs.
    double m_visualSampleRate;
    // Not allowed to change after the constructor runs.
    double m_audioVisualRatio;

    // We create an NxN texture out of m_data's buffer in the GLSL renderer. The
    // stride is N. Not allowed to change after the constructor runs.
    int m_textureStride;

    // For performance, completion is shared as a QAtomicInt and does not lock
    // the mutex. The completion of the waveform calculation.
    QAtomicInt m_completion;

    // The number of stem contained in waveform samples. 0 if not a stem waveform
    int m_stemCount;

    // Per-stem display detail, multi-resolution. Level 0 has one column per
    // visual frame, level i aggregates 2^i frames of level 0.
    std::vector<std::vector<WaveformStemColumn>> m_stemColumnLevels;
    // Whole-track peak amplitude per stem and the peak of the loudest stem.
    int m_stemPeak[mixxx::kMaxSupportedStems]{};
    int m_stemPeakReference{};

    mutable QMutex m_mutex;

    DISALLOW_COPY_AND_ASSIGN(Waveform);
};

typedef QSharedPointer<Waveform> WaveformPointer;
typedef QSharedPointer<const Waveform> ConstWaveformPointer;
