#include "AudioCapture.hpp"

#include <Geode/Geode.hpp>

#if defined(GEODE_IS_ANDROID)
#include <jni.h>
#include <platform/android/jni/JniHelper.h>
#endif

namespace voicedash {

AudioCapture::AudioCapture(VoiceDetector& detector) : m_detector(detector) {}
AudioCapture::~AudioCapture() { stop(); }

bool AudioCapture::start() {
    if (m_running.exchange(true, std::memory_order_acq_rel)) return true;
    m_error.store(false, std::memory_order_relaxed);
#if defined(GEODE_IS_ANDROID)
    m_thread = std::thread(&AudioCapture::worker, this);
    return true;
#else
    m_running.store(false, std::memory_order_release);
    m_error.store(true, std::memory_order_relaxed);
    log::warn("VoiceDash audio capture is Android-only.");
    return false;
#endif
}

void AudioCapture::stop() noexcept {
    m_running.store(false, std::memory_order_release);
    if (m_thread.joinable()) m_thread.join();
    m_detector.reset();
}

void AudioCapture::worker() noexcept {
#if defined(GEODE_IS_ANDROID)
    JavaVM* vm = cocos2d::JniHelper::getJavaVM();
    if (!vm) {
        m_error.store(true, std::memory_order_relaxed);
        m_running.store(false, std::memory_order_release);
        log::error("VoiceDash: Android JavaVM unavailable.");
        return;
    }

    JNIEnv* env = nullptr;
    bool attached = false;
    jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            m_error.store(true, std::memory_order_relaxed);
            m_running.store(false, std::memory_order_release);
            return;
        }
        attached = true;
    } else if (status != JNI_OK) {
        m_error.store(true, std::memory_order_relaxed);
        m_running.store(false, std::memory_order_release);
        return;
    }

    jobject recorder = nullptr;
    jshortArray buffer = nullptr;
    jclass audioRecordClass = env->FindClass("android/media/AudioRecord");
    if (!audioRecordClass || env->ExceptionCheck()) goto cleanup;

    {
        jmethodID minBufferMethod = env->GetStaticMethodID(
            audioRecordClass, "getMinBufferSize", "(III)I");
        jmethodID ctor = env->GetMethodID(audioRecordClass, "<init>", "(IIIII)V");
        if (!minBufferMethod || !ctor || env->ExceptionCheck()) goto cleanup;

        constexpr jint sampleRate = 44100;
        constexpr jint sourceMic = 1;       // MediaRecorder.AudioSource.MIC
        constexpr jint channelMono = 16;    // AudioFormat.CHANNEL_IN_MONO
        constexpr jint pcm16 = 2;           // AudioFormat.ENCODING_PCM_16BIT
        jint minBytes = env->CallStaticIntMethod(
            audioRecordClass, minBufferMethod, sampleRate, channelMono, pcm16);
        if (env->ExceptionCheck() || minBytes <= 0) goto cleanup;

        // Keep enough buffering for reliable reads; latency is measured on-device.
        jint bufferBytes = (minBytes * 2 < 8192) ? 8192 : minBytes * 2;
        recorder = env->NewObject(audioRecordClass, ctor, sourceMic, sampleRate,
                                  channelMono, pcm16, bufferBytes);
        if (env->ExceptionCheck() || !recorder) goto cleanup;

        jmethodID getState = env->GetMethodID(audioRecordClass, "getState", "()I");
        jmethodID startRecording = env->GetMethodID(audioRecordClass, "startRecording", "()V");
        jmethodID read = env->GetMethodID(audioRecordClass, "read", "([SII)I");
        jmethodID stopRecording = env->GetMethodID(audioRecordClass, "stop", "()V");
        jmethodID release = env->GetMethodID(audioRecordClass, "release", "()V");
        if (!getState || !startRecording || !read || !stopRecording || !release) goto cleanup;
        if (env->CallIntMethod(recorder, getState) != 1 || env->ExceptionCheck()) goto cleanup;

        buffer = env->NewShortArray(2048);
        if (!buffer) goto cleanup;
        env->CallVoidMethod(recorder, startRecording);
        if (env->ExceptionCheck()) goto cleanup;

        while (m_running.load(std::memory_order_acquire)) {
            jint count = env->CallIntMethod(recorder, read, buffer, 0, 2048);
            if (env->ExceptionCheck() || count < 0) {
                m_error.store(true, std::memory_order_relaxed);
                break;
            }
            if (count == 0) continue;

            jshort samples[2048];
            env->GetShortArrayRegion(buffer, 0, count, samples);
            if (env->ExceptionCheck()) {
                m_error.store(true, std::memory_order_relaxed);
                break;
            }
            m_detector.process(reinterpret_cast<int16_t const*>(samples),
                               static_cast<size_t>(count), sampleRate);
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
        env->CallVoidMethod(recorder, stopRecording);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->CallVoidMethod(recorder, release);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(buffer);
        buffer = nullptr;
        env->DeleteLocalRef(recorder);
        recorder = nullptr;
    }

cleanup:
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        m_error.store(true, std::memory_order_relaxed);
    }
    if (buffer) env->DeleteLocalRef(buffer);
    if (recorder) env->DeleteLocalRef(recorder);
    if (audioRecordClass) env->DeleteLocalRef(audioRecordClass);
    if (attached) vm->DetachCurrentThread();
    if (m_running.exchange(false, std::memory_order_acq_rel) && m_error.load()) {
        log::error("VoiceDash: audio capture failed. Check microphone permission and device compatibility.");
    }
#endif
}

} // namespace voicedash
