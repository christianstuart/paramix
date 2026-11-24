#include "waveform/stemdetail/stemdetailtrack.h"

bool operator==(const StemDetailKey& lhs, const StemDetailKey& rhs) {
    return lhs.trackId == rhs.trackId &&
            lhs.sampleRate == rhs.sampleRate &&
            lhs.samplesPerColumn == rhs.samplesPerColumn &&
            lhs.height == rhs.height &&
            lhs.magnification == rhs.magnification &&
            lhs.mixdown == rhs.mixdown;
}

bool operator!=(const StemDetailKey& lhs, const StemDetailKey& rhs) {
    return !(lhs == rhs);
}
