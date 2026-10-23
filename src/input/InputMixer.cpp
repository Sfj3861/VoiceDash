#include "InputMixer.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

namespace voicedash {

bool InputMixer::s_injectingVoice = false;
bool InputMixer::s_physicalJump = false;
bool InputMixer::s_voiceJump = false;
bool InputMixer::s_effectiveJump = false;

RoutedButton InputMixer::route(bool down, int button, bool isPlayer1, bool inPlayLayer) noexcept {
    const int jumpButton = static_cast<int>(PlayerButton::Jump);

    // GJBaseGameLayer::handleButton's third argument is isPlayer1.
    // Merge only player-one Jump during gameplay; preserve all other input.
    if (!inPlayLayer || !isPlayer1 || button != jumpButton) {
        return {true, down};
    }

    if (s_injectingVoice) {
        s_voiceJump = down;
    } else {
        s_physicalJump = down;
    }

    const bool effective = s_physicalJump || s_voiceJump;
    if (effective == s_effectiveJump) {
        return {false, effective};
    }

    s_effectiveJump = effective;
    return {true, effective};
}

void InputMixer::injectVoice(PlayLayer* layer, bool down) noexcept {
    if (layer == nullptr) return;

    s_injectingVoice = true;
    // true targets player one (not player two).
    layer->handleButton(down, static_cast<int>(PlayerButton::Jump), true);
    s_injectingVoice = false;
}

void InputMixer::reset(PlayLayer* layer) noexcept {
    // Clear physical ownership first, then route one final release through the
    // same mixer. This also handles a key/touch held while the pause scene opens.
    s_physicalJump = false;
    if (layer != nullptr && (s_voiceJump || s_effectiveJump)) {
        injectVoice(layer, false);
    }

    s_injectingVoice = false;
    s_physicalJump = false;
    s_voiceJump = false;
    s_effectiveJump = false;
}

bool InputMixer::isInjectingVoice() noexcept {
    return s_injectingVoice;
}

} // namespace voicedash
