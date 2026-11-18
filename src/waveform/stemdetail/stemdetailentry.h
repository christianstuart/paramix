#pragma once

#include <cstdint>

/// One cell of Traktor's detail waveform image (spec §28).
///
/// The coverage default is 1.0 because the shipped reducer accumulates a
/// constant 1.0 per sample. The empty cell of the picture is the *zero* entry
/// and must be constructed explicitly: a {coverage=1, bands=0} cell maps to
/// black, not to the lane base colour.
struct StemDetailEntry {
    float coverage = 1.0f;
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
};

/// Reducer configuration, matching stemwave's DetailConfig.
struct StemDetailConfig {
    uint32_t sampleRate = 44100;
    int height = 21;
    int samplesPerColumn = 316;
    float magnification = 2.0f;
    int verticalInset = 1;
};
