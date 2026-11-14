#include "stems/stemresampler.h"

#include <algorithm>

#ifdef __STEM_SEPARATION__
#include <demucs/resample.hpp>
#endif

namespace mixxx::stems {

bool resamplePlanar(const float* pInput,
        int channels,
        SINT frames,
        int inputRate,
        int outputRate,
        std::vector<float>* pOutput,
        SINT* pOutputFrames) {
    if (pInput == nullptr || channels <= 0 || frames <= 0 || inputRate <= 0 ||
            outputRate <= 0 || pOutput == nullptr || pOutputFrames == nullptr) {
        return false;
    }
    if (inputRate == outputRate) {
        pOutput->assign(pInput, pInput + static_cast<std::size_t>(frames) * channels);
        *pOutputFrames = frames;
        return true;
    }
#ifdef __STEM_SEPARATION__
    demucs::audio::SincResampler resampler(
            static_cast<double>(outputRate) / static_cast<double>(inputRate));
    const SINT outputFrames = static_cast<SINT>(resampler.output_frames(frames));
    if (outputFrames <= 0) {
        return false;
    }
    pOutput->assign(static_cast<std::size_t>(outputFrames) * channels, 0.0f);
    std::vector<float> channel(static_cast<std::size_t>(frames));
    for (int channelIndex = 0; channelIndex < channels; ++channelIndex) {
        std::copy_n(pInput + static_cast<std::size_t>(channelIndex) * frames,
                frames,
                channel.data());
        resampler.process(channel.data(),
                frames,
                pOutput->data() +
                        static_cast<std::size_t>(channelIndex) * outputFrames);
    }
    *pOutputFrames = outputFrames;
    return true;
#else
    return false;
#endif
}

} // namespace mixxx::stems
