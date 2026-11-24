#pragma once

// required for Qt-Macros
#include <qobjectdefs.h>

namespace mixxx {
Q_NAMESPACE

enum class OverviewType {
    Filtered,
    HSV,
    RGB,
    /// Stem-lane colour model: band balance sets the hue, loudness the
    /// brightness. Renderers without it draw RGB.
    Detail,
};
Q_ENUM_NS(OverviewType);

} // namespace mixxx
