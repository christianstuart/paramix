#pragma once

#include <QString>

namespace mixxx {

/// Sets the macOS process name, which is what the system menu bar displays for
/// a non-bundled developer build. The executable is still `paramix`; this only
/// changes the presentation name to `Paramix`.
void setMacProcessName(const QString& name);

} // namespace mixxx
