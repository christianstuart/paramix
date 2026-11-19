#include "fields.h"

#include <algorithm>
#include <cmath>

#include "float32.h"

namespace paramix::analysis {

std::vector<TransientField> computeFields(
        const std::vector<RefinedPeak>& peaks, const std::vector<float>& bass) {
    std::vector<TransientField> fields;
    fields.reserve(peaks.size());

    float maxFlux = 0.0f;
    for (const RefinedPeak& peak : peaks) {
        if (peak.flux > maxFlux) {
            maxFlux = peak.flux;
        }
    }
    const float reciprocal = maxFlux > 0.0f ? f32(f32(1.0f) / maxFlux) : 0.0f;

    const int bassSize = static_cast<int>(bass.size());
    constexpr double kBassScale = 689.0625; // 22050 / 32
    for (const RefinedPeak& peak : peaks) {
        TransientField field;
        field.position = peak.position;
        field.weight = maxFlux > 0.0f ? f32(reciprocal * peak.flux) : 0.0f;

        const int centre = static_cast<int>(std::trunc(peak.position * kBassScale));
        double sum = 0.0;
        for (int k = -68; k <= 68; ++k) {
            if (k == 0) {
                continue;
            }
            const int index = centre + k;
            if (index < 0 || index >= bassSize) {
                continue;
            }
            sum += (k > 0 ? 1.0 : -1.0) * static_cast<double>(
                                                      bass[static_cast<std::size_t>(index)]);
        }
        field.bassWeight = sum > 0.0 ? f32(sum) : 0.0f;
        fields.push_back(field);
    }

    return fields;
}

} // namespace paramix::analysis
