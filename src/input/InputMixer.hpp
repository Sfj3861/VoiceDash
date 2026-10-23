#pragma once

class PlayLayer;

namespace voicedash {

struct RoutedButton final {
    bool forward = true;
    bool down = false;
};

/**
 * Merges voice Jump and physical Jump into one logical state.
 * Every method is called only from the game's main thread.
 */
class InputMixer final {
public:
    static RoutedButton route(bool down, int button, bool isPlayer1, bool inPlayLayer) noexcept;
    static void injectVoice(PlayLayer* layer, bool down) noexcept;
    static void reset(PlayLayer* layer) noexcept;
    static bool isInjectingVoice() noexcept;

private:
    static bool s_injectingVoice;
    static bool s_physicalJump;
    static bool s_voiceJump;
    static bool s_effectiveJump;
};

} // namespace voicedash
