#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>

using namespace geode::prelude;

namespace {

void hideMainMenuGeodeButton(MenuLayer* layer) {
    auto button = layer->getChildByIDRecursive("geode.loader/geode-button");
    if (!button) return;

    button->setVisible(false);
    button->setContentSize({0.f, 0.f});

    if (auto parent = button->getParent()) {
        parent->updateLayout();
    }
}

}

class $modify(PLMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        hideMainMenuGeodeButton(this);
        restoreMoreGamesButton();
        return true;
    }

    void restoreMoreGamesButton() {
        if (getChildByIDRecursive("more-games-button")) return;

        auto sprite = CCSprite::createWithSpriteFrameName(
            "GJ_moreGamesBtn_001.png"
        );
        if (!sprite) return;

        auto button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(PLMenuLayer::onOpenMoreGames)
        );
        button->setID("more-games-button");

        auto menu = CCMenu::create();
        auto win = CCDirector::sharedDirector()->getWinSize();
        menu->setID("plstudio-more-games-menu");
        menu->setPosition({win.width - 40.f, 40.f});
        menu->addChild(button);
        addChild(menu, 35);
    }

    void onOpenMoreGames(CCObject* sender) {
        MenuLayer::onMoreGames(sender);
    }
};

class $modify(PLOptionsLayer, OptionsLayer) {
    void customSetup() {
        OptionsLayer::customSetup();

        auto sprite = ButtonSprite::create(
            "GEODE",
            70,
            true,
            "goldFont.fnt",
            "GJ_button_01.png",
            28.f,
            0.55f
        );
        if (!sprite) return;

        auto button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(PLOptionsLayer::onOpenGeode)
        );
        button->setID("open-geode-button");

        auto menu = CCMenu::create();
        auto win = CCDirector::sharedDirector()->getWinSize();
        menu->setID("plstudio-geode-settings-menu");
        menu->setPosition({win.width - 38.f, 38.f});
        menu->addChild(button);
        addChild(menu, 200);
    }

    void onOpenGeode(CCObject*) {
        geode::openModsList();
    }
};
