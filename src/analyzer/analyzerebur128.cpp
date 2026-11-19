#include "analyzer/analyzerebur128.h"

#include <QtDebug>

#include "analyzer/analyzertrack.h"
#include "analyzer/constants.h"
#include "track/track.h"
#include "util/math.h"
#include "util/sample.h"
#include "util/timer.h"

namespace {
constexpr double kReplayGain2ReferenceLUFS = -18;
} // anonymous namespace

AnalyzerEbur128::AnalyzerEbur128(UserSettingsPointer pConfig)
        : m_rgSettings(pConfig),
          m_pState(nullptr) {
}

AnalyzerEbur128::~AnalyzerEbur128() {
    cleanup(); // ...to prevent memory leaks
}

bool AnalyzerEbur128::initialize(
        const AnalyzerTrack& track,
        mixxx::audio::SampleRate sampleRate,
        mixxx::audio::ChannelCount channelCount,
        SINT frameLength) {
    if (m_rgSettings.isAnalyzerDisabled(2, track.getTrack()) || frameLength <= 0) {
        qDebug() << "Skipping AnalyzerEbur128";
        return false;
    }
    if (channelCount > mixxx::audio::ChannelCount::stereo() &&
            channelCount != mixxx::audio::ChannelCount::stem()) {
        DEBUG_ASSERT(!"Unsupported channel count");
        return false;
    }
    DEBUG_ASSERT(m_pState == nullptr);
    m_sourceChannelCount = channelCount;
    // An 8-channel stem track is mixed down to stereo, matching what the deck
    // outputs, so the measured loudness matches the audible signal.
    const mixxx::audio::ChannelCount measuredChannels =
            channelCount == mixxx::audio::ChannelCount::stem()
            ? mixxx::audio::ChannelCount::stereo()
            : channelCount;
    m_pState = ebur128_init(
            measuredChannels,
            sampleRate,
            EBUR128_MODE_I);
    return m_pState != nullptr;
}

void AnalyzerEbur128::cleanup() {
    if (m_pState) {
        ebur128_destroy(&m_pState);
        // ebur128_destroy clears the pointer but let's not rely on that.
        m_pState = nullptr;
    }
}

bool AnalyzerEbur128::processSamples(const CSAMPLE* pIn, SINT count) {
    VERIFY_OR_DEBUG_ASSERT(m_pState) {
        return false;
    }
    ScopedTimer t(QStringLiteral("AnalyzerEbur128::processSamples()"));

    SINT frames = count / m_sourceChannelCount;
    const CSAMPLE* pMeasured = pIn;
    CSAMPLE* pMixedChannel = nullptr;
    if (m_sourceChannelCount == mixxx::audio::ChannelCount::stem()) {
        pMixedChannel = SampleUtil::alloc(frames * mixxx::audio::ChannelCount::stereo());
        VERIFY_OR_DEBUG_ASSERT(pMixedChannel) {
            return false;
        }
        SampleUtil::mixMultichannelToStereo(
                pMixedChannel, pIn, frames, m_sourceChannelCount);
        pMeasured = pMixedChannel;
    }

    int e = ebur128_add_frames_float(m_pState, pMeasured, frames);
    if (pMixedChannel) {
        SampleUtil::free(pMixedChannel);
    }
    VERIFY_OR_DEBUG_ASSERT(e == EBUR128_SUCCESS) {
        qWarning() << "AnalyzerEbur128::processSamples() failed with" << e;
        return false;
    }
    return true;
}

void AnalyzerEbur128::storeResults(TrackPointer pTrack) {
    VERIFY_OR_DEBUG_ASSERT(m_pState) {
        return;
    }
    double averageLufs;
    int e = ebur128_loudness_global(m_pState, &averageLufs);
    VERIFY_OR_DEBUG_ASSERT(e == EBUR128_SUCCESS) {
        qWarning() << "AnalyzerEbur128::storeResults() failed with" << e;
        return;
    }
    if (averageLufs == -HUGE_VAL ||
            averageLufs == HUGE_VAL ||
            // This catches 0 and abnormal values inf and -inf (that may have
            // slipped through in libebur128 for some reason.
            !util_isnormal(averageLufs)) {
        qWarning() << "AnalyzerEbur128::storeResults() averageLufs invalid:"
                   << averageLufs;
        return;
    }

    const double fReplayGain2 = kReplayGain2ReferenceLUFS - averageLufs;
    mixxx::ReplayGain replayGain(pTrack->getReplayGain());
    replayGain.setRatio(db2ratio(fReplayGain2));
    pTrack->setReplayGain(replayGain);
    qDebug() << "ReplayGain 2.0 (libebur128) result is" << fReplayGain2
             << "dB for" << pTrack->getFileInfo();
}
