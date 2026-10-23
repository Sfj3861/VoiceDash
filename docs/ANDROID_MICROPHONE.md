# Android microphone permission

## Why this matters

Android microphone access requires both:
- android.permission.RECORD_AUDIO declared by the host APK.
- Runtime permission granted by the user.

A Geode .geode mod is loaded into an already-installed host app and cannot add a permission to that APK's manifest by itself. VoiceDash checks for capture failures and leaves voice input inactive when AAudio cannot open, but it does not implement a runtime permission prompt. If the host APK does not declare RECORD_AUDIO, enabling the mod setting is not enough.

## Development checks

1. Verify the exact host APK used to run Geometry Dash declares android.permission.RECORD_AUDIO.
2. Verify Android has granted that permission to the host app.
3. Enable VoiceDash in Geode settings before entering a level.
4. If capture does not start, inspect the Geode log for the AAudio error code.
5. Test on a physical Android device and handle permission denial or revocation gracefully.

The permission prompt must be implemented by the host app or a trusted launcher build designed to request it. VoiceDash does not bypass Android's permission model. Do not install untrusted APKs to test the mod.

## Capture backend

The current backend uses native AAudio, PCM 16-bit mono, with the device-configured sample rate and low-latency performance mode. It processes samples in the AAudio data callback without allocation, logging, UI operations, or locks. Audio is processed in memory and is not saved or uploaded.

AAudio is available from Android 8.0 (API 26), so the Android builds target API 26 or later.

## Detector limitations

The detector combines RMS amplitude, a slowly adapting background estimate, and activation/release hysteresis. This is deliberately lightweight for the audio callback. It is not a full voice activity detector: loud environmental sounds may trigger it, and quieter or breathy vocalizations may not.

## Validation checklist

- Verify the host manifest and runtime permission.
- Test permission denial/revocation, microphone busy, backgrounding, audio-route changes, pause/resume, death/restart, level exit, and repeated start/stop.
- Verify physical and voice Jump inputs do not cancel each other.
- Measure activation/release latency (p50/p95), false activations, false releases, CPU, and memory.
- Do not label a release stable until both Android targets build and real-device tests pass.
