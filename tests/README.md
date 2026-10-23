# VoiceDash test plan

## Automated detector tests

The GitHub Actions workflow compiles and runs tests/voice_detector_tests.cpp. It checks:
- Silence remains inactive.
- A short vocal-like burst does not pass activation confirmation.
- Sustained audio above threshold activates Jump.
- A brief gap does not release the input prematurely.
- Continued sound cancels the pending release.
- Sustained silence releases the input.
- forceRelease clears the active state.

These tests validate detector state transitions only. They do not prove that the AAudio backend works on a real phone.

## Android integration checklist

- Verify the host APK declares android.permission.RECORD_AUDIO and the user has granted it.
- Check microphone denial, a busy microphone, audio route changes, backgrounding, pause/resume, level exit, death/restart, and repeated start/stop.
- Verify physical Jump and voice Jump do not cancel one another.
- Measure activation/release latency (p50/p95), false activations, false releases, CPU, and memory.
- Confirm stream errors release any voice-held input and are cleaned up on the game thread.

Do not mark a release stable until both Android builds succeed and these device tests have been completed.
