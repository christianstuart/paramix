#pragma once

#include <vector>

#include "refine.h"

namespace paramix::analysis {

struct TransientField {
    double position = 0.0;
    float weight = 0.0f;
    float bassWeight = 0.0f;
};

/// Finalize pass for the two float record fields:
/// weight = f32(f32(1 / maxFlux) * flux) and
/// bassWeight = max(0, sum_{k=-68..68, k!=0} sign(k) * bass[f2 + k]) with
/// f2 = trunc(position * 689.0625).
std::vector<TransientField> computeFields(
        const std::vector<RefinedPeak>& peaks, const std::vector<float>& bass);

} // namespace paramix::analysis
