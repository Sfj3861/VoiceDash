# VoiceDash

VoiceDash is an experimental Android-only Geometry Dash / Geode mod that turns a sustained voiced sound (such as a vowel or hum) into the Jump input.

- Start a voiced sound → press Jump.
- Confirmed end of the sound → release Jump.
- Capture is opt-in and stops when voice control is disabled, the level exits, the game pauses, or capture fails.
- Touch, keyboard, mouse, and voice Jump states are combined so releasing one source does not cancel another source that is still holding Jump.
- Audio is analysed locally. No speech-to-text, audio upload, or recording files.

## Status

Early implementation. The code has not yet been confirmed by a successful Android build or real-device tests. Do not treat it as a stable release until GitHub Actions succeeds and input/permission behaviour is verified on-device.

## Target

- Geometry Dash Android 2.2081.
- Geode Android 32-bit and 64-bit, subject to SDK and installed-game compatibility.
- Android AudioRecord through JNI, PCM16 mono at 44.1 kHz, with 512-sample read requests.

## Important Android microphone limitation

Android requires android.permission.RECORD_AUDIO to be declared by the host APK and granted by the user at runtime. A .geode mod cannot independently merge a permission into the already-installed Geometry Dash/Geode Launcher manifest. VoiceDash fails closed if audio capture cannot initialize, but microphone access will not work if the host app does not declare and grant the permission. The current mod does not implement a standalone runtime permission prompt.

See docs/ANDROID_MICROPHONE.md before testing.

## Build

Install the Geode SDK/CLI and Android NDK using the official Geode development documentation. Then:

~~~sh
geode sdk install-binaries -p android64
geode build -p android64

# If the installed game and Geode setup are 32-bit:
geode sdk install-binaries -p android32
geode build -p android32
~~~

The GitHub Actions workflow builds Android32 and Android64 and uploads build artifacts. CI success is not a substitute for on-device microphone/input tests.

## Configuration

Open VoiceDash in Geode's mod settings:

- **Enable voice control** is off by default.
- **Voice activation threshold** controls the minimum audio level.
- **Voice release threshold** adds hysteresis to prevent rapid toggling.
- **Activation confirmation** filters very short sounds.
- **Release confirmation** controls how long silence must persist before Jump is released.

The detector combines RMS amplitude, a slowly adapting background estimate, autocorrelation-based periodicity, and attack/release hysteresis. It favours sustained voiced sounds; it is not a perfect voice/noise classifier. Music, tonal noises, and loud environmental sounds may still trigger it, while breathy or unvoiced sounds may not.

## Safety and privacy

Audio is processed in memory and not saved. Capture should only run while voice control is enabled and a level is active. Start calibration in an easy level and keep voice control disabled when you do not want the microphone active.

## Architecture

- **VoiceDetector:** RMS, adaptive noise estimate, periodicity check, and hysteresis.
- **AudioCapture:** Android AudioRecord capture on a dedicated worker thread.
- **Input merging:** combines physical Jump state and voice Jump state before forwarding transitions to the game.
- **PlayLayer hooks:** scope capture to level play, stop on pause/exit, and restart after resume.

## License

MIT. See LICENSE.
