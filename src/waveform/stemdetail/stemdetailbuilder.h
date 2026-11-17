#pragma once

#include <QObject>
#include <atomic>
#include <memory>

#include "track/track_decl.h"
#include "waveform/stemdetail/stemdetailtrack.h"

/// Background worker that decodes a track and reduces its stem lanes.
///
/// The worker lives on its own QThread. build() is invoked through a queued
/// connection; cancel() is an atomic flag that the GUI thread may set at any
/// time, so a superseded build stops at the next decoded chunk.
class StemDetailBuilder final : public QObject {
    Q_OBJECT

  public:
    explicit StemDetailBuilder(QObject* parent = nullptr);

  public slots:
    void build(const StemDetailKey& key, const TrackPointer& pTrack);
    void cancel();

  signals:
    void built(const StemDetailKey& key, std::shared_ptr<const StemDetailTrack> detail);

  private:
    std::atomic<bool> m_cancel;
};
