#pragma once

namespace mixxx {

/// Relaunches the current executable with the same arguments. The obsolete
/// UI-selection flags are accepted and ignored by CmdlineArgs. Returns false if
/// the new process could not be started. The caller is responsible for quitting
/// the current process.
bool restartApplication();

} // namespace mixxx
