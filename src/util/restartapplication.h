#pragma once

namespace mixxx {

/// Relaunches the current executable with the same arguments. Returns false if
/// the new process could not be started. The caller is responsible for quitting
/// the current process.
bool restartApplication();

} // namespace mixxx
