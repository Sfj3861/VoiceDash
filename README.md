# VoiceDash

VoiceDash is an experimental Android-only Geometry Dash / Geode mod that turns a continuous vocalization into the jump input.

- Start vocalizing → press Jump.
- Confirmed silence → release Jump.
- Microphone capture stops when gameplay ends or is paused.
- Touch, keyboard, mouse, and voice are merged so releasing one source does not cancel another source that is still holding Jump.
- Audio is analysed locally: no speech recognition and no network access.

## Status

Initial implementation. The detector, native capture path, input merger, and Android build workflow are being developed together. A successful GitHub Actions build and on-device tests are required before calling any release stable.

## Target

- Geometry Dash Android 2.2081.
- Geode Android 32-bit and 64-bit, where supported by the chosen Geode SDK and game installation.
- C++ audio capture through Android's AAudio NDK API (Android 8.0/API 26 or later).

## Important Android microphone limitation

A Geode mod cannot merge a new permission into the already-installed game's Android manifest. Android requires android.permission.RECORD_AUDIO to be declared by the host APK and granted by the user at runtime. VoiceDash detects capture failures and fails closed, but microphone access will not work if the Geode/Geometry Dash host app does not declare and grant that permission.

See docs/ANDROID_MICROPHONE.md before testing. For development, use a trusted Geode launcher/build that declares the permission and displays Android's permission prompt.

## Build

Install the Geode SDK/CLI and Android NDK using the official Geode development documentation. Then:

~~~sh
geode sdk install-binaries -p android64
geode build -p android64

# If your installed game and Geode setup are 32-bit:
geode sdk install-binaries -p android32
geode build -p android32
~~~

CI builds both Android targets. Download the build artifact from the Actions run; CI success is not a substitute for on-device microphone/input tests.

## Configuration

Open VoiceDash in Geode's mod settings:

- **Enable voice control** is off by default. Explicitly enable it before a level.
- **Activation threshold** controls how loud the vocalization must be.
- **Activation confirmation** filters very short noises.
- **Release confirmation** smooths tiny gaps in a vocalization.

The detector uses amplitude thresholds and hysteresis, not speech recognition. It can still mistake loud environmental sounds for a vocalization, so start with a quiet, calibrated environment.

## Safety and privacy

VoiceDash captures audio only while voice control is enabled and an active level is being played. It processes samples locally, stores no recordings, and stops capture on pause, level exit, stream error, or disable. A loud sound may activate Jump; keep an easy level loaded while calibrating.

## Architecture

- VoiceDetector: allocation-free, callback-safe energy gate with adaptive noise estimate and hysteresis.
- AndroidAudioCapture: low-latency AAudio input stream, PCM16 mono.
- InputMixer: merges voice and regular Jump states and forwards only effective transitions.
- VoiceRuntime: starts/stops capture and bridges audio state to the game's main thread.
- PlayLayer / PauseLayer hooks: scope microphone capture to active gameplay.

## License

MIT. See LICENSE.
