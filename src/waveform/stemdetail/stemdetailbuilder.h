#pragma once

#include <QObject>
#include <atomic>
#include <memory>

#include "track/track_decl.h"
#include "waveform/stemdetail/stemdetailtrack.h"

/// Background worker that decodes a track and reduces its stem lanes.
///
/// The worker lives on its own QThread. build() is invoked through a queued
/// connection. cancel() bumps an atomic generation, so a build that was
/// superseded before its queued slot started (or while it is decoding) stops
/// at the next chunk instead of finishing work nobody will use.
class StemDetailBuilder final : public QObject {
    Q_OBJECT

  public:
    explicit StemDetailBuilder(QObject* parent = nullptr);

    quint64 generation() const {
        return m_generation.load(std::memory_order_relaxed);
    }

  public slots:
    void build(const StemDetailKey& key,
            const TrackPointer& pTrack,
            quint64 generation);
    void cancel();

  signals:
    void built(const StemDetailKey& key, std::shared_ptr<const StemDetailTrack> detail);

  private:
    std::atomic<quint64> m_generation{0};
};
