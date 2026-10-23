#include "AndroidAudioCapture.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace voicedash {

AndroidAudioCapture::~AndroidAudioCapture() {
    stop();
}

bool AndroidAudioCapture::start(VoiceDetector& detector) noexcept {
    if (isRunning()) {
        return true;
    }

#if defined(GEODE_IS_ANDROID)
    // An earlier async error may have stopped playback without closing the
    // stream. Reap it before creating a replacement stream.
    if (m_stream != nullptr) {
        stop();
    }

    detector.forceRelease();
    m_detector.store(&detector, std::memory_order_release);
    m_lastError.store(0, std::memory_order_relaxed);

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || builder == nullptr) {
        if (builder != nullptr) {
            AAudioStreamBuilder_delete(builder);
        }
        m_lastError.store(result, std::memory_order_relaxed);
        m_detector.store(nullptr, std::memory_order_release);
        log::error("VoiceDash: AAudio_createStreamBuilder failed: {}",
                   AAudio_convertResultToText(result));
        return false;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 1);
    AAudioStreamBuilder_setSampleRate(builder, AAUDIO_UNSPECIFIED);
    AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_RECOGNITION);
    AAudioStreamBuilder_setDataCallback(builder, &AndroidAudioCapture::dataCallback, this);
    AAudioStreamBuilder_setErrorCallback(builder, &AndroidAudioCapture::errorCallback, this);

    result = AAudioStreamBuilder_openStream(builder, &m_stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || m_stream == nullptr) {
        m_lastError.store(result, std::memory_order_relaxed);
        if (m_stream != nullptr) {
            AAudioStream_close(m_stream);
        }
        m_stream = nullptr;
        m_detector.store(nullptr, std::memory_order_release);
        log::error(
            "VoiceDash: cannot open microphone stream ({}). Check Android RECORD_AUDIO "
            "manifest declaration and runtime permission.",
            AAudio_convertResultToText(result)
        );
        return false;
    }

    const auto actualRate = AAudioStream_getSampleRate(m_stream);
    const auto actualFormat = AAudioStream_getFormat(m_stream);
    const auto actualChannels = AAudioStream_getChannelCount(m_stream);
    if (actualRate <= 0 || actualFormat != AAUDIO_FORMAT_PCM_I16 || actualChannels != 1) {
        m_lastError.store(AAUDIO_ERROR_INVALID_FORMAT, std::memory_order_relaxed);
        AAudioStream_close(m_stream);
        m_stream = nullptr;
        m_sampleRate.store(0, std::memory_order_relaxed);
        m_detector.store(nullptr, std::memory_order_release);
        log::error(
            "VoiceDash: unsupported actual audio format (rate={}, format={}, channels={}); "
            "requires PCM16 mono.",
            actualRate, static_cast<int>(actualFormat), actualChannels
        );
        return false;
    }

    m_sampleRate.store(static_cast<std::uint32_t>(actualRate), std::memory_order_relaxed);

    // Publish running before starting, because an error callback may occur
    // immediately after requestStart. Do not overwrite a callback's failure.
    m_running.store(true, std::memory_order_release);
    result = AAudioStream_requestStart(m_stream);
    if (result != AAUDIO_OK) {
        m_running.store(false, std::memory_order_release);
        m_lastError.store(result, std::memory_order_relaxed);
        AAudioStream_close(m_stream);
        m_stream = nullptr;
        m_sampleRate.store(0, std::memory_order_relaxed);
        m_detector.store(nullptr, std::memory_order_release);
        log::error("VoiceDash: starting microphone failed: {}",
                   AAudio_convertResultToText(result));
        return false;
    }

    if (!m_running.load(std::memory_order_acquire)) {
        // AAudio may invoke the error callback before requestStart returns.
        // Reap the partially started stream here so the runtime does not leak
        // it when start() reports failure.
        log::error("VoiceDash: microphone stream failed immediately after start ({}).",
                   m_lastError.load(std::memory_order_relaxed));
        stop();
        return false;
    }

    log::info("VoiceDash: microphone capture started at {} Hz", actualRate);
    return true;
#else
    (void)detector;
    m_lastError.store(-1, std::memory_order_relaxed);
    log::error("VoiceDash: microphone capture is only implemented for Android.");
    return false;
#endif
}

void AndroidAudioCapture::stop() noexcept {
#if defined(GEODE_IS_ANDROID)
    m_running.store(false, std::memory_order_release);
    if (m_stream != nullptr) {
        (void)AAudioStream_requestStop(m_stream);
        // AAudioStream_close waits for its callbacks to finish. Clear the
        // detector pointer only after the callback lifetime has ended.
        (void)AAudioStream_close(m_stream);
        m_stream = nullptr;
    }
    m_sampleRate.store(0, std::memory_order_relaxed);
#endif

    if (auto* detector = m_detector.exchange(nullptr, std::memory_order_acq_rel)) {
        detector->forceRelease();
    }
}

#if defined(GEODE_IS_ANDROID)
aaudio_data_callback_result_t AndroidAudioCapture::dataCallback(
    AAudioStream*,
    void* userData,
    void* audioData,
    std::int32_t numFrames) noexcept {
    auto* self = static_cast<AndroidAudioCapture*>(userData);
    if (self == nullptr || audioData == nullptr || numFrames <= 0) {
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    auto* detector = self->m_detector.load(std::memory_order_acquire);
    if (detector == nullptr) {
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    const auto sampleRate = self->m_sampleRate.load(std::memory_order_relaxed);
    if (sampleRate > 0) {
        detector->processSamples(
            static_cast<const std::int16_t*>(audioData),
            static_cast<std::size_t>(numFrames),
            sampleRate
        );
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AndroidAudioCapture::errorCallback(
    AAudioStream*,
    void* userData,
    aaudio_result_t error) noexcept {
    auto* self = static_cast<AndroidAudioCapture*>(userData);
    if (self == nullptr) {
        return;
    }

    self->m_lastError.store(error, std::memory_order_relaxed);
    self->m_running.store(false, std::memory_order_release);

    // No logging, allocation, UI, locks, or stream closure from this callback.
    // The game thread observes the stopped state and performs cleanup.
}
#endif

} // namespace voicedash
