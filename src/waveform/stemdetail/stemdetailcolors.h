#pragma once

#include <QRgb>

#include "waveform/stemdetail/stemdetailentry.h"

/// Colour parameters of the shipping detail control (spec §34).
///
/// gain   WaveformControl+0x240
/// base   control+0x228..+0x234 (live #181818)
/// high   high-band scale, 1.0 for colour mode 3 (0x10352c8b4)
struct StemDetailColorParams {
    float gain = 3.5f;
    QRgb base = qRgb(24, 24, 24);
    float highScale = 1.0f;
};

/// coverage -> brightness LUT. The live LUT is pow(n/255, 1/3.5) for
/// n = 1..255 with LUT[0] = 0.
float stemDetailLut(int level);
float stemDetailBrightness(float coverage, float gain);

/// One entry -> one opaque 0xffRRGGBB pixel.
QRgb stemDetailEntryColor(const StemDetailEntry& entry,
        const StemDetailColorParams& params = {});
