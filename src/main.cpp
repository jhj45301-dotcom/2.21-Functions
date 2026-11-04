#include <Geode/Geode.hpp>
#include "MapFeature.hpp"
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/LevelAreaInnerLayer.hpp>
#include <Geode/binding/GameStatsManager.hpp>
#include <cstdlib>

using namespace geode::prelude;

namespace {

void restoreButtonSprite(CCNode* node) {
    if (!node) return;

    if (auto sprite = typeinfo_cast<CCSprite*>(node)) {
        sprite->setColor({255, 255, 255});
        sprite->setOpacity(255);
    }

    if (auto children = node->getChildren()) {
        for (auto child : CCArrayExt<CCNode*>(children)) {
            restoreButtonSprite(child);
        }
    }
}

void enableCreatorButton(CCNode* root, char const* id, bool makeBright) {
    auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(
        root->getChildByIDRecursive(id)
    );
    if (!button) return;

    button->setEnabled(true);
    button->setOpacity(255);

    if (makeBright) {
        button->setColor({255, 255, 255});
        restoreButtonSprite(button->getNormalImage());
        restoreButtonSprite(button->getSelectedImage());
        restoreButtonSprite(button->getDisabledImage());
    }
}

void applyColoredCreatorArtwork(
    CCNode* root,
    char const* buttonID,
    char const* spriteFrame
) {
    auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(
        root->getChildByIDRecursive(buttonID)
    );
    if (!button) return;

    auto artwork = CCSprite::createWithSpriteFrameName(spriteFrame);
    if (!artwork) return;

    CCSize targetSize{84.f, 84.f};

    if (auto parent = button->getParent()) {
        if (auto children = parent->getChildren()) {
            for (auto child : CCArrayExt<CCNode*>(children)) {
                auto neighbour = typeinfo_cast<CCMenuItemSpriteExtra*>(child);

                if (
                    !neighbour ||
                    neighbour == button ||
                    !neighbour->isEnabled()
                ) {
                    continue;
                }

                auto image = neighbour->getNormalImage();
                if (!image) continue;

                auto size = image->getContentSize();
                auto width = size.width * std::abs(image->getScaleX());
                auto height = size.height * std::abs(image->getScaleY());

                if (width > 50.f && height > 50.f) {
                    targetSize = CCSize(width, height);
                    break;
                }
            }
        }
    }

    auto artworkSize = artwork->getContentSize();

    auto scaleX = artworkSize.width > 0.f
        ? targetSize.width / artworkSize.width
        : 0.8f;

    auto scaleY = artworkSize.height > 0.f
        ? targetSize.height / artworkSize.height
        : 0.8f;

    button->setSprite(artwork);
    artwork->setScaleX(scaleX);
    artwork->setScaleY(scaleY);
    button->setEnabled(true);
    restoreButtonSprite(artwork);
}

}

class $modify(PLCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) {
            return false;
        }

        if (
            GameStatsManager::sharedState()->hasCompletedMainLevel(5004)
        ) {
            Mod::get()->setSavedValue("the-map-unlocked", true);
        }

        if (Mod::get()->getSettingValue<bool>("enable-the-map")) {
            enableCreatorButton(this, "map-button", true);

            applyColoredCreatorArtwork(
                this,
                "map-button",
                "GJ_mapBtn_001.png"
            );
        }

        if (Mod::get()->getSettingValue<bool>("enable-versus")) {
            enableCreatorButton(this, "versus-button", true);

            applyColoredCreatorArtwork(
                this,
                "versus-button",
                "GJ_versusBtn_001.png"
            );
        }

        if (Mod::get()->getSettingValue<bool>("enable-debug-map")) {
            auto debugSprite = ButtonSprite::create(
                "DEBUG MAP",
                92,
                true,
                "goldFont.fnt",
                "GJ_button_04.png",
                30.f,
                0.48f
            );

            auto debugButton = CCMenuItemSpriteExtra::create(
                debugSprite,
                this,
                menu_selector(PLCreatorLayer::onDebugMap)
            );

            debugButton->setPosition({-72.f, 0.f});

            auto dialogSprite = ButtonSprite::create(
                "DEBUG DIALOG",
                108,
                true,
                "goldFont.fnt",
                "GJ_button_04.png",
                30.f,
                0.42f
            );

            auto dialogButton = CCMenuItemSpriteExtra::create(
                dialogSprite,
                this,
                menu_selector(PLCreatorLayer::onDebugMapDialog)
            );

            dialogButton->setPosition({72.f, 0.f});

            auto debugMenu = CCMenu::create();
            auto win = CCDirector::sharedDirector()->getWinSize();

            debugMenu->setPosition({win.width / 2.f, 18.f});
            debugMenu->addChild(debugButton);
            debugMenu->addChild(dialogButton);
            debugMenu->setID("debug-map-menu");

            addChild(debugMenu, 20);
        }

        return true;
    }

    void onAdventureMap(CCObject* sender) {
        if (!Mod::get()->getSettingValue<bool>("enable-the-map")) {
            CreatorLayer::onAdventureMap(sender);
            return;
        }

        if (Mod::get()->getSavedValue<bool>("the-map-unlocked", false)) {
            CCDirector::sharedDirector()->replaceScene(
                CCTransitionFade::create(
                    0.35f,
                    createRestoredMapScene()
                )
            );
            return;
        }

        showLockedMapDialog();
    }

    void onMultiplayer(CCObject* sender) {
        if (!Mod::get()->getSettingValue<bool>("enable-versus")) {
            CreatorLayer::onMultiplayer(sender);
            return;
        }

        return;
    }

    void onDebugMap(CCObject*) {
        CCDirector::sharedDirector()->replaceScene(
            CCTransitionFade::create(
                0.35f,
                createRestoredMapScene()
            )
        );
    }

    void onDebugMapDialog(CCObject*) {
        showLockedMapDialog();
    }
};

class $modify(PLLevelAreaInnerLayer, LevelAreaInnerLayer) {
    void showFloor1CompleteDialog() {
        Mod::get()->setSavedValue("the-map-unlocked", true);
        LevelAreaInnerLayer::showFloor1CompleteDialog();
    }
};
