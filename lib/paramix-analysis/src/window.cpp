#include "window.h"

#include <cmath>

#include "float32.h"

namespace paramix::analysis {

WindowTable makeWindowTable() {
    WindowTable window{};
    const float k = f32(f32(6.2831855f) / f32(511.0f));
    for (int i = 0; i < kWindowLength; ++i) {
        const float c = f32(std::cos(static_cast<double>(f32(f32(i) * k))));
        window[static_cast<std::size_t>(i)] =
                f32(f32(f32(0.5f) - f32(f32(0.5f) * c)) * f32(0.5f));
    }
    return window;
}

} // namespace paramix::analysis
