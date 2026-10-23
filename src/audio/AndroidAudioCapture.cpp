#include "AndroidAudioCapture.hpp"

#include <Geode/Geode.hpp>

#if defined(GEODE_IS_ANDROID)
#include <algorithm>
#endif

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
    if (m_stream != nullptr) {
        stop();
    }

    m_detector = &detector;
    m_detector->forceRelease();
    m_lastError.store(0, std::memory_order_relaxed);

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || builder == nullptr) {
        m_lastError.store(result, std::memory_order_relaxed);
        log::error("VoiceDash: AAudio_createStreamBuilder failed: {}",
                   AAudio_convertResultToText(result));
        return false;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 1);
    AAudioStreamBuilder_setSampleRate(builder, 48000);
    AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_RECOGNITION);
    AAudioStreamBuilder_setDataCallback(builder, &AndroidAudioCapture::dataCallback, this);
    AAudioStreamBuilder_setErrorCallback(builder, &AndroidAudioCapture::errorCallback, this);

    result = AAudioStreamBuilder_openStream(builder, &m_stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || m_stream == nullptr) {
        m_lastError.store(result, std::memory_order_relaxed);
        m_stream = nullptr;
        log::error(
            "VoiceDash: cannot open microphone stream ({}). Check Android RECORD_AUDIO "
            "manifest declaration and runtime permission.",
            AAudio_convertResultToText(result)
        );
        m_detector = nullptr;
        return false;
    }

    // Honour the device's actual configured rate instead of assuming all
    // audio routes run at the requested rate.
    const auto actualRate = AAudioStream_getSampleRate(m_stream);
    if (actualRate <= 0) {
        m_lastError.store(AAUDIO_ERROR_INVALID_RATE, std::memory_order_relaxed);
        AAudioStream_close(m_stream);
        m_stream = nullptr;
        m_detector = nullptr;
        return false;
    }

    // Publish running before starting: the error callback may fire during
    // requestStart. Never overwrite its false state after the callback runs.
    m_running.store(true, std::memory_order_release);
    result = AAudioStream_requestStart(m_stream);
    if (result != AAUDIO_OK) {
        m_running.store(false, std::memory_order_release);
        m_lastError.store(result, std::memory_order_relaxed);
        AAudioStream_close(m_stream);
        m_stream = nullptr;
        m_detector = nullptr;
        log::error("VoiceDash: starting microphone failed: {}",
                   AAudio_convertResultToText(result));
        return false;
    }

    if (m_running.load(std::memory_order_acquire)) {
        log::info("VoiceDash: microphone capture started at {} Hz", actualRate);
    }
    return true;
#else
    (void)detector;
    log::error("VoiceDash: microphone capture is only implemented for Android.");
    return false;
#endif
}

void AndroidAudioCapture::stop() noexcept {
#if defined(GEODE_IS_ANDROID)
    m_running.store(false, std::memory_order_release);
    if (m_stream != nullptr) {
        (void)AAudioStream_requestStop(m_stream);
        (void)AAudioStream_close(m_stream);
        m_stream = nullptr;
    }
#endif

    if (m_detector != nullptr) {
        m_detector->forceRelease();
        m_detector = nullptr;
    }
}

#if defined(GEODE_IS_ANDROID)
aaudio_data_callback_result_t AndroidAudioCapture::dataCallback(
    AAudioStream* stream,
    void* userData,
    void* audioData,
    std::int32_t numFrames) noexcept {
    auto* self = static_cast<AndroidAudioCapture*>(userData);
    if (self == nullptr || audioData == nullptr || numFrames <= 0) {
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    VoiceDetector* detector = self->m_detector;
    if (detector == nullptr) {
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    const auto sampleRate = AAudioStream_getSampleRate(stream);
    if (sampleRate > 0) {
        // Format is explicitly PCM_I16 and channel count is explicitly mono.
        detector->processSamples(
            static_cast<const std::int16_t*>(audioData),
            static_cast<std::size_t>(numFrames),
            static_cast<std::uint32_t>(sampleRate)
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

    // Do not log, allocate, show UI, or close the stream from the audio callback.
    // The game thread observes the stopped state and performs cleanup.
}
#endif

} // namespace voicedash
