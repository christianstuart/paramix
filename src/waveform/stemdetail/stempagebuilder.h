#pragma once

#include <QImage>
#include <QObject>
#include <memory>

#include "waveform/stemdetail/stemdetailtrack.h"
#include "waveform/stemdetail/stempagekey.h"

/// Build one page of the stem picture from an immutable detail track.
///
/// This is pure CPU work and safe to run on a worker thread. The returned image
/// is `QImage::Format_ARGB32_Premultiplied` with opaque colour pixels; lane
/// alpha, mute and order are applied by the renderer.
QImage buildStemPageImage(const StemDetailTrack& detail, const StemPageKey& key);

/// Worker that builds pages off the scene-graph thread.
class StemPageBuilder : public QObject {
    Q_OBJECT
  public:
    explicit StemPageBuilder(QObject* parent = nullptr);

  public slots:
    void build(const StemPageKey& key,
            std::shared_ptr<const StemDetailTrack> detail);

  signals:
    void built(const StemPageKey& key, const QImage& image);
};
