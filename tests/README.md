# VoiceDash test plan

## Detector tests
- Silence at several noise levels must remain inactive.
- Sustained tone/noisy vocalization above the activation threshold must activate after the configured attack interval.
- A short impulse must not latch the detector indefinitely.
- Once active, brief dips above the release interval must not release the input.
- Sustained silence must release after the configured release interval.
- Reset must always clear the active state.

## Android integration tests
- Confirm RECORD_AUDIO permission is granted before starting capture.
- Verify capture failure does not crash the game and does not leave a held input.
- Start/stop repeatedly while entering and leaving levels.
- Test backgrounding, pause, death, restart, level exit, and audio focus changes.
- Verify touch/keyboard input still works while voice input is active.
- Measure activation/release latency and CPU use on each target device.

These are planned tests; they are not represented as passing until run against a built Android package.
