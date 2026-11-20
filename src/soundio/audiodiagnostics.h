#pragma once

#include <QtGlobal>
#include <atomic>

namespace mixxx::audio::diagnostics {

/// Underflow code of the most recent device callback underrun.
///
/// SoundManager::underflowHappened() runs on the real-time callback thread and
/// only stores the code here. The ~1 Hz dropout report reads it from the GUI
/// thread. A relaxed atomic keeps the callback free of locks, allocation and
/// logging, which is why the code is not written to a ControlObject.
inline std::atomic<int> s_lastUnderflowCode{0};

inline void recordUnderflow(int code) {
    s_lastUnderflowCode.store(code, std::memory_order_relaxed);
}

inline int lastUnderflowCode() {
    return s_lastUnderflowCode.load(std::memory_order_relaxed);
}

} // namespace mixxx::audio::diagnostics
