# VoiceDash

VoiceDash is an experimental Android-only Geometry Dash / Geode mod that maps a sustained voiced sound (such as a vowel or hum) to the Jump input.

- Start vocalizing → press Jump.
- Confirmed silence → release Jump.
- Capture is opt-in and stops when voice control is disabled, a level exits, the game pauses, or the audio stream fails.
- Touch, keyboard, mouse, and voice Jump states are merged so releasing one source does not cancel another source that is still holding Jump.
- Audio is analysed locally. No speech-to-text, audio upload, or recording files.

## Status

Early implementation. A successful Android build and real-device tests are still required before calling this stable. The GitHub Actions workflow builds both Android architectures and runs detector unit tests.

## Target

- Geometry Dash Android 2.2081.
- Geode Android 32-bit and 64-bit, subject to SDK and installed-game compatibility.
- Native AAudio capture in PCM16 mono at the device-configured sample rate.
- Android 8.0 (API 26) or later, because the capture backend links against AAudio.

## Important Android microphone limitation

Android requires android.permission.RECORD_AUDIO to be declared by the host APK and granted at runtime. A .geode mod cannot independently merge a permission into the already-installed Geometry Dash/Geode Launcher manifest. VoiceDash fails closed if capture cannot initialize, but microphone access will not work if the host app does not declare and grant the permission. The mod does not bypass Android's permission model or provide its own host-app permission prompt.

See docs/ANDROID_MICROPHONE.md before testing.

## Build

Install the Geode SDK/CLI and Android NDK using the official Geode development documentation. For a local build:

~~~sh
geode sdk install-binaries -p android64
geode build -p android64

# If the installed game and Geode setup are 32-bit:
geode sdk install-binaries -p android32
geode build -p android32
~~~

GitHub Actions builds Android32 and Android64 and uploads a combined .geode artifact. CI success does not replace on-device microphone/input testing.

## Configuration

Open VoiceDash in Geode's mod settings:

- **Enable voice control** is off by default.
- **Voice activation threshold** controls the minimum audio level.
- **Voice release threshold** adds hysteresis to prevent rapid toggling.
- **Activation confirmation** filters very short sounds.
- **Release confirmation** controls how long silence must persist before Jump is released.

The current detector uses RMS amplitude, a slowly adapting background estimate, and activation/release hysteresis. It is intentionally lightweight for the real-time AAudio callback. It is not a full voice activity detector: loud environmental sounds may trigger it, and quieter or breathy vocalizations may not.

## Safety and privacy

Audio is processed in memory and not saved. Capture runs only while voice control is enabled and gameplay is active. Start calibration in an easy level and keep voice control disabled when you do not want the microphone active.

## Architecture

- **VoiceDetector:** bounded, allocation-free RMS analysis and hysteresis.
- **AndroidAudioCapture:** native AAudio input stream with a low-latency performance mode.
- **InputMixer:** merges physical Jump and voice Jump states, forwarding only effective transitions.
- **VoiceRuntime:** manages settings, capture lifecycle, pause/resume, and fail-safe release.
- **PlayLayer / PauseLayer hooks:** scope capture to active gameplay.

## License

MIT. See LICENSE.
