# Android microphone permission

## Why this matters

Android restricts microphone access. The host Android application must include:

~~~xml
<uses-permission android:name="android.permission.RECORD_AUDIO" />
~~~

The user must also grant the runtime permission. A Geode mod package is loaded into an already-installed host app; it cannot add a new entry to that APK's manifest by itself. Putting an AndroidManifest.xml file inside this mod repository does not merge it into the installed Geometry Dash or Geode Launcher manifest.

## Development checks

1. Verify that the exact host APK used to run Geometry Dash declares android.permission.RECORD_AUDIO.
2. Verify that Android has granted that permission to the host app.
3. Start a level only after enabling VoiceDash in Geode's mod settings.
4. If the stream does not open, inspect the Geode log. VoiceDash logs the AAudio error and does not synthesize a held input after capture failure.
5. After changing host-app permission/build configuration, fully restart the host app and try again.

The runtime permission prompt must be implemented by the host application (or by a launcher build designed to request it). VoiceDash deliberately does not claim to bypass Android's permission model. Do not install untrusted APKs or alter the permission model to test this mod.

## AAudio compatibility

The native AAudio API is available from Android 8.0 (API 26). The callback receives mono PCM16 samples and does only bounded RMS analysis. Older Android versions and unusual audio routes are not supported by this initial capture backend.

## Known detector limits

The first detector combines RMS amplitude, a slowly-adapting quiet-room noise estimate, and different activation/release thresholds. It does not identify a person's voice. A sufficiently loud environmental sound can trigger it. A future VAD backend may improve rejection of music, taps, and other non-vocal sounds, but must still preserve low latency.
