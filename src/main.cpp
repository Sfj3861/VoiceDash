#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include "input/InputMixer.hpp"
#include "runtime/VoiceRuntime.hpp"

#include <algorithm>
#include <string>

using namespace geode::prelude;

namespace {

void showVoiceCalibrator() {
    const double activation = Mod::get()->getSettingValue<double>("activation-threshold");
    const double release = Mod::get()->getSettingValue<double>("release-threshold");

    createQuickPopup(
        "VoiceDash Calibrator",
        fmt::format(
            "Adjust the microphone sensitivity. Make a sustained vowel/hum while playing, "
            "then pause to tune it.\n\nCurrent activation threshold: {:.3f}\n"
            "Current release threshold: {:.3f}\n\n"
            "More sensitive helps quiet voices but may react to background noise.",
            activation, release
        ),
        "Less sensitive",
        "More sensitive",
        [](auto, bool moreSensitive) {
            double on = Mod::get()->getSettingValue<double>("activation-threshold");
            double off = Mod::get()->getSettingValue<double>("release-threshold");

            if (moreSensitive) {
                on = std::max(0.005, on * 0.80);
                off = std::max(0.002, off * 0.80);
            } else {
                on = std::min(0.20, on * 1.20);
                off = std::min(0.15, off * 1.20);
            }

            // Keep the release threshold below activation so the detector has
            // hysteresis and does not chatter around one threshold.
            off = std::min(off, on * 0.80);
            Mod::get()->setSettingValue<double>("activation-threshold", on);
            Mod::get()->setSettingValue<double>("release-threshold", off);
            showVoiceCalibrator();
        }
    );
}

} // namespace

class $modify(VoiceDashBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        // GJBaseGameLayer is also used by the editor. Only merge voice input
        // into player-one Jump while the current object is a PlayLayer.
        const bool inPlayLayer = typeinfo_cast<PlayLayer*>(this) != nullptr;
        const auto routed = voicedash::InputMixer::route(
            down, button, isPlayer1, inPlayLayer
        );

        if (routed.forward) {
            GJBaseGameLayer::handleButton(routed.down, button, isPlayer1);
        }
    }
};

class $modify(VoiceDashPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        voicedash::InputMixer::reset(this);
        voicedash::VoiceRuntime::get().onLevelStart(this);
        return true;
    }

    void update(float dt) {
        // Apply the latest audio state before the game's frame update so a
        // detected vocalization can affect this frame rather than one frame
        // after physics has already advanced.
        voicedash::VoiceRuntime::get().onLevelUpdate(this);
        PlayLayer::update(dt);
    }

    void onQuit() {
        voicedash::VoiceRuntime::get().onLevelEnd(this);
        PlayLayer::onQuit();
    }
};

class $modify(VoiceDashPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto* label = ButtonSprite::create("Voice Calibrator");
        auto* item = CCMenuItemSpriteExtra::create(
            label, this, menu_selector(VoiceDashPauseLayer::onVoiceCalibrator)
        );
        auto* menu = CCMenu::create();
        menu->addChild(item);
        menu->setPosition({ CCDirector::sharedDirector()->getWinSize().width / 2.0f,
                            CCDirector::sharedDirector()->getWinSize().height - 38.0f });
        menu->setID("voicedash-calibrator-menu");
        this->addChild(menu, 100);

        // Compact square close button on the right side of the pause menu.
        // Reuse Geometry Dash's native close icon and resume the level when tapped.
        auto* closeSprite = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        if (closeSprite) {
            closeSprite->setScale(0.85f);
            auto* closeItem = CCMenuItemSpriteExtra::create(
                closeSprite, this, menu_selector(VoiceDashPauseLayer::onVoiceDashClose)
            );
            auto* closeMenu = CCMenu::create();
            closeMenu->addChild(closeItem);
            const auto winSize = CCDirector::sharedDirector()->getWinSize();
            closeMenu->setPosition({ winSize.width - 24.0f, winSize.height * 0.5f });
            closeMenu->setID("voicedash-close-menu");
            this->addChild(closeMenu, 101);
        }
    }

    void onVoiceCalibrator(CCObject*) {
        showVoiceCalibrator();
    }

    void onVoiceDashClose(CCObject*) {
        // Same effect as the game's Resume button: dismiss the pause layer.
        this->onResume(nullptr);
    }

    void onEnter() {
        if (auto* layer = PlayLayer::get()) {
            voicedash::VoiceRuntime::get().onPause(layer);
            voicedash::InputMixer::reset(layer);
        }
        PauseLayer::onEnter();
    }

    void onExit() {
        PauseLayer::onExit();
        if (auto* layer = PlayLayer::get()) {
            voicedash::VoiceRuntime::get().onResume(layer);
        }
    }
};

$on_mod(Loaded) {
    log::info(
        "VoiceDash loaded. Voice control is opt-in; the Android host app must declare "
        "and grant RECORD_AUDIO."
    );
}
