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
    if (layer != nullptr && s_voiceJump) {
        injectVoice(layer, false);
    }

    // A scene is ending, so clear cached source state to avoid carrying a
    // held button into the next level.
    s_injectingVoice = false;
    s_physicalJump = false;
    s_voiceJump = false;
    s_effectiveJump = false;
}

bool InputMixer::isInjectingVoice() noexcept {
    return s_injectingVoice;
}

} // namespace voicedash
