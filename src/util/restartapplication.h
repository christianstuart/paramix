#pragma once

namespace mixxx {

/// Relaunches the current executable with the same arguments, optionally
/// forcing the Paramix QML interface. Returns false if the new process could
/// not be started. The caller is responsible for quitting the current process.
bool restartApplication(bool useQml);

} // namespace mixxx
