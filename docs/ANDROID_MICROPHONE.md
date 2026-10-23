# Android microphone permission

## Why this matters

Android microphone access requires both:
- android.permission.RECORD_AUDIO declared by the host APK.
- Runtime permission granted by the user.

A Geode .geode mod is loaded into an already-installed host app and cannot add a permission to that APK's manifest by itself. The current VoiceDash code detects capture failures and stops safely, but it does not yet implement a runtime permission prompt. If the host APK does not declare RECORD_AUDIO, enabling the mod setting is not enough.

## Development checks

1. Verify the exact host APK used to run Geometry Dash declares android.permission.RECORD_AUDIO.
2. Verify Android has granted that permission to the host app.
3. Enable VoiceDash in Geode settings before entering a level.
4. If capture does not start, inspect the Geode log for AudioRecord/JNI errors.
5. Test on a physical Android device and handle permission denial or revocation gracefully.

The permission prompt must be implemented by the host app or a trusted launcher build designed to request it. VoiceDash does not bypass Android's permission model. Do not install untrusted APKs to test the mod.

## Capture backend

The current code uses Android Java AudioRecord through JNI: MIC source, PCM 16-bit mono, 44.1 kHz, with 512-sample read requests on a dedicated worker thread. Samples are processed in memory and are not saved or uploaded. A capture failure leaves voice input inactive.

The native worker must remain independent from the game thread. It must not call Geometry Dash input methods directly; it only publishes detector state for the gameplay thread to consume.

## Detector limitations

The detector combines RMS amplitude, an adaptive background estimate, autocorrelation-based periodicity, and different activation/release thresholds. This favours sustained voiced sounds such as vowels or humming. It is not a perfect voice/noise classifier: tonal environmental sounds can trigger it, and breathy or unvoiced vocalizations may not activate it.

## Validation checklist

- Verify the host manifest and runtime permission.
- Test permission denial/revocation, microphone busy, backgrounding, audio-route changes, pause/resume, death/restart, level exit, and repeated start/stop.
- Verify physical and voice Jump inputs do not cancel each other.
- Measure activation/release latency (p50/p95), false activations, false releases, CPU, and memory.
- Do not label a release stable until both Android targets build and real-device tests pass.
