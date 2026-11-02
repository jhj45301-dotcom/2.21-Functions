#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/modify/LevelAreaInnerLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GameStatsManager.hpp>
#include <Geode/binding/DialogLayer.hpp>
#include <Geode/binding/DialogObject.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/LevelPage.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

using namespace geode::prelude;

namespace {

CCMenuItemSpriteExtra* s_nativeGeodeButton = nullptr;

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

class PLCreditsPage : public geode::Popup {
protected:
    bool init() override {
        if (!Popup::init(440.f, 270.f, "GJ_square01.png")) {
            return false;
        }

        m_noElasticity = true;
        this->setTitle("CREDITS", "bigFont.fnt", 1.f, 24.f);

        auto panel = cocos2d::extension::CCScale9Sprite::create(
            "square02b_001.png"
        );
        panel->setContentSize({350.f, 155.f});
        panel->setColor({90, 45, 25});
        panel->setOpacity(150);
        panel->setPosition({220.f, 132.f});
        m_mainLayer->addChild(panel);

        auto gameManager = GameManager::sharedState();
        auto player = SimplePlayer::create(219);

        player->setColor(
            gameManager->colorForIdx(gameManager->getPlayerColor())
        );
        player->setSecondColor(
            gameManager->colorForIdx(gameManager->getPlayerColor2())
        );
        player->updateColors();
        player->setScale(1.55f);
        player->setPosition({220.f, 157.f});
        m_mainLayer->addChild(player, 2);

        auto nickname = CCLabelBMFont::create(
            "PLATON445301",
            "goldFont.fnt"
        );
        nickname->setScale(0.78f);
        nickname->setPosition({220.f, 100.f});
        m_mainLayer->addChild(nickname, 2);

        auto role = CCLabelBMFont::create(
            "CREATOR OF 2.21 FUNCTIONS",
            "bigFont.fnt"
        );
        role->setScale(0.38f);
        role->setColor({255, 235, 150});
        role->setPosition({220.f, 72.f});
        m_mainLayer->addChild(role, 2);

        auto studio = CCLabelBMFont::create(
            "PL STUDIO",
            "bigFont.fnt"
        );
        studio->setScale(0.32f);
        studio->setPosition({220.f, 48.f});
        m_mainLayer->addChild(studio, 2);

        return true;
    }

public:
    static PLCreditsPage* create() {
        auto ret = new PLCreditsPage();

        if (ret->init()) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }
};

class PLTowerDialogDelegate : public CCNode, public DialogDelegate {
public:
    static PLTowerDialogDelegate* create() {
        auto ret = new PLTowerDialogDelegate();

        if (ret->init()) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    void dialogClosed(DialogLayer*) override {
        auto level = GameLevelManager::sharedState()->getMainLevel(
            1,
            false
        );

        auto page = level ? LevelPage::create(level) : nullptr;

        if (page) {
            page->onTheTower(nullptr);
        }
    }
};

class PLDebugMapLayer : public CCLayerColor {
    CCNode* m_mapCanvas = nullptr;
    CCPoint m_lastTouch{};
    std::map<int, CCSprite*> m_visibleTiles;
    float m_assetScale = 1.f;

    static constexpr float kMapZoom = 0.70f;

    static constexpr int kColumns = 30;
    static constexpr int kRows = 30;

    static constexpr std::array<int, kColumns> kColumnWidths = {
        273, 272, 273, 273, 272, 273, 273, 272, 273, 273,
        272, 273, 273, 272, 273, 273, 272, 273, 273, 272,
        273, 273, 272, 273, 273, 272, 273, 273, 272, 273
    };

    static constexpr std::array<int, kRows> kRowHeights = {
        116, 117, 116, 116, 116, 117, 116, 116, 116, 117,
        116, 116, 116, 117, 116, 116, 117, 116, 116, 116,
        117, 116, 116, 116, 117, 116, 116, 116, 117, 116
    };

    static constexpr float kMapWidth = 8180.f;
    static constexpr float kMapHeight = 3488.f;

    float columnStart(int column) const {
        float result = 0.f;

        for (int i = 0; i < column; ++i) {
            result += kColumnWidths[i];
        }

        return result / m_assetScale * kMapZoom;
    }

    float rowTop(int row) const {
        float result = kMapHeight;

        for (int i = 0; i < row; ++i) {
            result -= kRowHeights[i];
        }

        return result / m_assetScale * kMapZoom;
    }

    float mapWidth() const {
        return kMapWidth / m_assetScale * kMapZoom;
    }

    float mapHeight() const {
        return kMapHeight / m_assetScale * kMapZoom;
    }

    std::filesystem::path tilePath(int index) const {
        return Mod::get()->getResourcesDir() /
            fmt::format("image_part_{:03}.png", index);
    }

    void refreshVisibleTiles() {
        if (!m_mapCanvas) return;

        auto win = CCDirector::sharedDirector()->getWinSize();
        auto pos = m_mapCanvas->getPosition();
        auto preloadMargin = 280.f * kMapZoom;

        auto left = -pos.x - preloadMargin;
        auto right = -pos.x + win.width + preloadMargin;
        auto bottom = -pos.y - preloadMargin;
        auto top = -pos.y + win.height + preloadMargin;

        std::set<int> needed;

        for (int row = 0; row < kRows; ++row) {
            for (int column = 0; column < kColumns; ++column) {
                auto tileWidth =
                    static_cast<float>(kColumnWidths[column]) * kMapZoom;

                auto tileHeight =
                    static_cast<float>(kRowHeights[row]) * kMapZoom;

                auto x = columnStart(column) + tileWidth / 2.f;
                auto y = rowTop(row) - tileHeight / 2.f;

                if (
                    x + tileWidth / 2.f < left ||
                    x - tileWidth / 2.f > right ||
                    y + tileHeight / 2.f < bottom ||
                    y - tileHeight / 2.f > top
                ) {
                    continue;
                }

                auto index = row * kColumns + column + 1;
                needed.insert(index);

                if (m_visibleTiles.contains(index)) {
                    continue;
                }

                auto path = tilePath(index);
                auto pathString = geode::utils::string::pathToString(path);

                if (auto tile = CCSprite::create(pathString.c_str())) {
                    auto content = tile->getContentSize();

                    if (content.width > 0.f) {
                        tile->setScaleX(tileWidth / content.width);
                    }

                    if (content.height > 0.f) {
                        tile->setScaleY(tileHeight / content.height);
                    }

                    tile->setPosition({x, y});
                    tile->setAnchorPoint({0.5f, 0.5f});
                    m_mapCanvas->addChild(tile, 1);
                    m_visibleTiles[index] = tile;
                }
            }
        }

        std::vector<int> stale;

        for (auto const& [index, tile] : m_visibleTiles) {
            if (!needed.contains(index)) {
                stale.push_back(index);
            }
        }

        for (auto index : stale) {
            auto path = tilePath(index);

            if (auto tile = m_visibleTiles[index]) {
                tile->removeFromParentAndCleanup(true);
            }

            auto pathString = geode::utils::string::pathToString(path);

            CCTextureCache::sharedTextureCache()->removeTextureForKey(
                pathString.c_str()
            );

            m_visibleTiles.erase(index);
        }
    }

public:
    static PLDebugMapLayer* create() {
        auto ret = new PLDebugMapLayer();

        if (ret->init()) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    static CCScene* scene() {
        auto scene = CCScene::create();

        if (auto layer = PLDebugMapLayer::create()) {
            scene->addChild(layer);
        }

        return scene;
    }

    bool init() override {
        if (!CCLayerColor::initWithColor({58, 58, 58, 255})) {
            return false;
        }

        auto win = CCDirector::sharedDirector()->getWinSize();

        m_assetScale = 1.f;
        m_mapCanvas = CCNode::create();

        auto playerX = 1020.f / m_assetScale * kMapZoom;
        auto playerY = 1760.f / m_assetScale * kMapZoom;

        auto startX = std::max(
            win.width - mapWidth(),
            std::min(0.f, win.width / 2.f - playerX)
        );

        auto startY = std::max(
            win.height - mapHeight(),
            std::min(0.f, win.height / 2.f - playerY)
        );

        m_mapCanvas->setPosition({startX, startY});
        addChild(m_mapCanvas, 1);

        auto gameManager = GameManager::sharedState();
        auto shadowPath = Mod::get()->getResourcesDir() / "PLMapShadow.png";
        auto shadowPathString =
            geode::utils::string::pathToString(shadowPath);

        auto shadow = CCSprite::create(shadowPathString.c_str());

        if (shadow) {
            auto size = shadow->getContentSize();

            auto baseShadowX = size.width > 0.f
                ? 27.f / size.width
                : 1.f;

            auto baseShadowY = size.height > 0.f
                ? 8.f / size.height
                : 1.f;

            shadow->setScaleX(baseShadowX);
            shadow->setScaleY(baseShadowY);
            shadow->setPosition({playerX, playerY - 12.f});
            shadow->setID("map-player-shadow");
            m_mapCanvas->addChild(shadow, 19);

            shadow->runAction(CCRepeatForever::create(CCSequence::create(
                CCScaleTo::create(
                    0.08f,
                    baseShadowX,
                    baseShadowY
                ),
                CCEaseSineOut::create(
                    CCScaleTo::create(
                        0.26f,
                        baseShadowX * 0.62f,
                        baseShadowY * 0.72f
                    )
                ),
                CCEaseSineIn::create(
                    CCScaleTo::create(
                        0.26f,
                        baseShadowX,
                        baseShadowY
                    )
                ),
                CCScaleTo::create(
                    0.08f,
                    baseShadowX,
                    baseShadowY
                ),
                CCDelayTime::create(0.20f),
                nullptr
            )));
        }

        auto player = SimplePlayer::create(
            gameManager->getPlayerFrame()
        );

        player->setColor(
            gameManager->colorForIdx(gameManager->getPlayerColor())
        );

        player->setSecondColor(
            gameManager->colorForIdx(gameManager->getPlayerColor2())
        );

        if (gameManager->getPlayerGlow()) {
            player->setGlowOutline(
                gameManager->colorForIdx(
                    gameManager->getPlayerGlowColor()
                )
            );
        }

        player->updateColors();

        auto playerSize = player->getContentSize();
        auto playerExtent = std::max(
            playerSize.width,
            playerSize.height
        );

        auto basePlayerScale = playerExtent > 0.f
            ? 36.f / playerExtent
            : 0.58f;

        player->setScale(basePlayerScale);
        player->setPosition({playerX, playerY});
        player->setID("map-player");
        m_mapCanvas->addChild(player, 20);

        player->runAction(CCRepeatForever::create(CCSequence::create(
            CCScaleTo::create(
                0.08f,
                basePlayerScale * 1.09f,
                basePlayerScale * 0.87f
            ),
            CCSpawn::create(
                CCEaseSineOut::create(
                    CCMoveBy::create(0.26f, {0.f, 10.f})
                ),
                CCScaleTo::create(
                    0.26f,
                    basePlayerScale * 0.91f,
                    basePlayerScale * 1.11f
                ),
                nullptr
            ),
            CCSpawn::create(
                CCEaseSineIn::create(
                    CCMoveBy::create(0.26f, {0.f, -10.f})
                ),
                CCScaleTo::create(
                    0.26f,
                    basePlayerScale * 1.09f,
                    basePlayerScale * 0.87f
                ),
                nullptr
            ),
            CCScaleTo::create(
                0.08f,
                basePlayerScale,
                basePlayerScale
            ),
            CCDelayTime::create(0.20f),
            nullptr
        )));

        refreshVisibleTiles();

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        addChild(menu, 10);

        auto arrow = CCSprite::createWithSpriteFrameName(
            "GJ_arrow_01_001.png"
        );

        auto back = CCMenuItemSpriteExtra::create(
            arrow,
            this,
            menu_selector(PLDebugMapLayer::onBack)
        );

        back->setPosition({25.f, win.height - 25.f});
        menu->addChild(back);

        setKeypadEnabled(true);
        setTouchMode(kCCTouchesOneByOne);
        setTouchEnabled(true);

        auto music = Mod::get()->getResourcesDir() / "adventureLoop.mp3";

        FMODAudioEngine::sharedEngine()->playMusic(
            geode::utils::string::pathToString(music),
            true,
            0.5f,
            0
        );

        return true;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        m_lastTouch = touch->getLocation();
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (!m_mapCanvas) return;

        auto current = touch->getLocation();
        auto delta = ccpSub(current, m_lastTouch);
        auto position = ccpAdd(m_mapCanvas->getPosition(), delta);

        auto win = CCDirector::sharedDirector()->getWinSize();

        position.x = std::max(
            win.width - mapWidth(),
            std::min(0.f, position.x)
        );

        position.y = std::max(
            win.height - mapHeight(),
            std::min(0.f, position.y)
        );

        m_mapCanvas->setPosition(position);
        refreshVisibleTiles();
        m_lastTouch = current;
    }

    void keyBackClicked() override {
        onBack(nullptr);
    }

    void onBack(CCObject*) {
        FMODAudioEngine::sharedEngine()->stopMusic(0);
        GameManager::sharedState()->playMenuMusic();

        CCDirector::sharedDirector()->replaceScene(
            CCTransitionFade::create(
                0.35f,
                CreatorLayer::scene()
            )
        );
    }
};

CCScene* createRestoredMapScene() {
    return PLDebugMapLayer::scene();
}

void showLockedMapDialog() {
    auto dialogs = CCArray::create();

    dialogs->addObject(DialogObject::create(
        "The Keymaster",
        "Where do you <cl>think</c> you are going?",
        2,
        1.f,
        false,
        {255, 255, 255}
    ));

    dialogs->addObject(DialogObject::create(
        "The Keymaster",
        "One does not simply <cg>WALK<d010>.<d010>.<d010>.</c>\n<d050>into <co>The Map</c>.",
        2,
        1.f,
        false,
        {255, 255, 255}
    ));

    dialogs->addObject(DialogObject::create(
        "The Keymaster",
        "<cr>FIRST!</c><d025> You must complete<d050>.<d050>.<d050>.\n<d050><i050><s260><cy>THE</c></s></i> <i050><s260><cy>TOWER</c>!</s></i>",
        2,
        1.f,
        false,
        {255, 255, 255}
    ));

    auto layer = DialogLayer::createWithObjects(dialogs, 2);
    auto delegate = PLTowerDialogDelegate::create();

    if (!layer || !delegate) return;

    layer->m_delegate = delegate;
    layer->addChild(delegate);
    layer->animateIn(DialogAnimationType::FromLeft);
    layer->addToMainScene();
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

class $modify(PLMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        if (auto geodeButton = typeinfo_cast<CCMenuItemSpriteExtra*>(
            getChildByIDRecursive("geode.loader/geode-button")
        )) {
            auto parent = geodeButton->getParent();

            geodeButton->retain();
            geodeButton->removeFromParentAndCleanup(false);
            s_nativeGeodeButton = geodeButton;

            if (parent) {
                parent->updateLayout();
            }
        }

        if (!getChildByIDRecursive("more-games-button")) {
            if (
                auto sprite = CCSprite::createWithSpriteFrameName(
                    "GJ_moreGamesBtn_001.png"
                )
            ) {
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
        }

        return true;
    }

    void onOpenMoreGames(CCObject* sender) {
        MenuLayer::onMoreGames(sender);
    }
};

class $modify(PLOptionsLayer, OptionsLayer) {
    void customSetup() {
        OptionsLayer::customSetup();

        auto win = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();

        menu->setID("plstudio-geode-settings-menu");
        menu->setPosition({win.width - 38.f, 38.f});

        if (
            s_nativeGeodeButton &&
            !s_nativeGeodeButton->getParent()
        ) {
            s_nativeGeodeButton->setPosition({0.f, 0.f});
            menu->addChild(s_nativeGeodeButton);
        }

        addChild(menu, 200);
    }

    void layerHidden() {
        if (
            s_nativeGeodeButton &&
            s_nativeGeodeButton->getParent()
        ) {
            s_nativeGeodeButton->removeFromParentAndCleanup(false);
        }

        OptionsLayer::layerHidden();
    }
};