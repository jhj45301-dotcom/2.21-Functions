#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/modify/LevelAreaInnerLayer.hpp>
#include <Geode/modify/GauntletNode.hpp>
#include <Geode/modify/GauntletLayer.hpp>
#include <Geode/modify/GauntletSprite.hpp>
#include <Geode/modify/GauntletSelectLayer.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/modify/GameStatsManager.hpp>
#include <Geode/modify/GJMapPack.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GameStatsManager.hpp>
#include <Geode/binding/DialogLayer.hpp>
#include <Geode/binding/DialogObject.hpp>
#include <Geode/binding/CurrencyRewardLayer.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GJRewardItem.hpp>
#include <Geode/binding/LevelPage.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <vector>

using namespace geode::prelude;

namespace {

int s_gauntletVisualOverride = 0;
CCMenuItemSpriteExtra* s_nativeGeodeButton = nullptr;
std::set<int> s_newGauntletLevelIDs;

bool needsLegacyGauntletCompatibility(int id) {
    return id == 61 || id == 62;
}

std::string customGauntletRewardKey(int id) {
    return fmt::format("gauntlet-{}-reward-claimed", id);
}

bool isCustomGauntletRewardClaimed(int id) {
    return Mod::get()->getSavedValue<bool>(customGauntletRewardKey(id), false);
}

void rememberNewGauntletLevels(GJMapPack* pack) {
    if (!pack || !needsLegacyGauntletCompatibility(pack->m_packID) || !pack->m_levels) return;

    for (auto object : CCArrayExt<CCObject*>(pack->m_levels)) {
        if (auto value = typeinfo_cast<CCString*>(object)) {
            auto levelID = value->intValue();
            if (levelID > 0) s_newGauntletLevelIDs.insert(levelID);
        }
    }
}

std::string graphicsSuffix() {
    auto scale = CC_CONTENT_SCALE_FACTOR();
    return scale >= 3.5f ? "-uhd" : (scale >= 1.5f ? "-hd" : "");
}

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
    auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(root->getChildByIDRecursive(id));
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

    // The official colour artwork is already hidden in GJ_GameSheet04.
    // The game normally greys it out because the menu item is disabled.
    auto artwork = CCSprite::createWithSpriteFrameName(spriteFrame);
    if (!artwork) return;

    // Measure a real neighbouring active card instead of hard-coding a
    // resolution-dependent size. This also works with texture packs.
    CCSize targetSize{84.f, 84.f};
    if (auto parent = button->getParent()) {
        if (auto children = parent->getChildren()) {
            for (auto child : CCArrayExt<CCNode*>(children)) {
                auto neighbour = typeinfo_cast<CCMenuItemSpriteExtra*>(child);
                if (!neighbour || neighbour == button || !neighbour->isEnabled()) {
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
    auto scaleX = artworkSize.width > 0.f ? targetSize.width / artworkSize.width : 0.8f;
    auto scaleY = artworkSize.height > 0.f ? targetSize.height / artworkSize.height : 0.8f;
    button->setSprite(artwork);
    artwork->setScaleX(scaleX);
    artwork->setScaleY(scaleY);
    button->setEnabled(true);
    restoreButtonSprite(artwork);
}

} // namespace

class PLCreditsPage : public geode::Popup {
protected:
    bool init() override {
        if (!Popup::init(440.f, 270.f, "GJ_square01.png")) return false;

        m_noElasticity = true;
        this->setTitle("CREDITS", "bigFont.fnt", 1.f, 24.f);

        auto panel = cocos2d::extension::CCScale9Sprite::create("square02b_001.png");
        panel->setContentSize({350.f, 155.f});
        panel->setColor({90, 45, 25});
        panel->setOpacity(150);
        panel->setPosition({220.f, 132.f});
        m_mainLayer->addChild(panel);

        auto gameManager = GameManager::sharedState();
        // Fixed author icon from the creator profile. It must not change when
        // another local cube is selected in the garage.
        auto player = SimplePlayer::create(219);
        player->setColor(gameManager->colorForIdx(gameManager->getPlayerColor()));
        player->setSecondColor(gameManager->colorForIdx(gameManager->getPlayerColor2()));
        player->updateColors();
        player->setScale(1.55f);
        player->setPosition({220.f, 157.f});
        m_mainLayer->addChild(player, 2);

        auto nickname = CCLabelBMFont::create("PLATON445301", "goldFont.fnt");
        nickname->setScale(0.78f);
        nickname->setPosition({220.f, 100.f});
        m_mainLayer->addChild(nickname, 2);

        auto role = CCLabelBMFont::create("CREATOR OF 2.21 FUNCTIONS", "bigFont.fnt");
        role->setScale(0.38f);
        role->setColor({255, 235, 150});
        role->setPosition({220.f, 72.f});
        m_mainLayer->addChild(role, 2);

        auto studio = CCLabelBMFont::create("PL STUDIO", "bigFont.fnt");
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
        // On Windows the Tower scene itself is not exported, but this is the
        // native handler Geometry Dash uses to enter it from an official
        // level page. A temporary page gives the handler its expected state.
        auto level = GameLevelManager::sharedState()->getMainLevel(1, false);
        auto page = level ? LevelPage::create(level) : nullptr;
        if (page) page->onTheTower(nullptr);
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
    // The source was cut at fractional boundaries, so the pieces alternate
    // between 272/273 px and 116/117 px. Keep their original sizes: scaling
    // every tile to an average size causes seams and makes the restored map
    // look warped.
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
        for (int i = 0; i < column; ++i) result += kColumnWidths[i];
        return result / m_assetScale * kMapZoom;
    }

    float rowTop(int row) const {
        float result = kMapHeight;
        for (int i = 0; i < row; ++i) result -= kRowHeights[i];
        return result / m_assetScale * kMapZoom;
    }

    float mapWidth() const { return kMapWidth / m_assetScale * kMapZoom; }
    float mapHeight() const { return kMapHeight / m_assetScale * kMapZoom; }

    std::filesystem::path tilePath(int index) const {
        // Geode flattens wildcard resource folders into this mod's namespaced
        // resource directory when it creates the .geode package.
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
                auto tileWidth = static_cast<float>(kColumnWidths[column]) * kMapZoom;
                auto tileHeight = static_cast<float>(kRowHeights[row]) * kMapZoom;
                auto x = columnStart(column) + tileWidth / 2.f;
                auto y = rowTop(row) - tileHeight / 2.f;
                if (
                    x + tileWidth / 2.f < left || x - tileWidth / 2.f > right ||
                    y + tileHeight / 2.f < bottom || y - tileHeight / 2.f > top
                ) continue;

                auto index = row * kColumns + column + 1;
                needed.insert(index);
                if (m_visibleTiles.contains(index)) continue;

                auto path = tilePath(index);
                auto pathString = geode::utils::string::pathToString(path);
                if (auto tile = CCSprite::create(pathString.c_str())) {
                    // Keep Normal graphics as the visual reference. HD/UHD
                    // textures can report a different logical size, so fit
                    // every piece to the same Normal-sized map rectangle.
                    auto content = tile->getContentSize();
                    if (content.width > 0.f) tile->setScaleX(tileWidth / content.width);
                    if (content.height > 0.f) tile->setScaleY(tileHeight / content.height);
                    tile->setPosition({x, y});
                    tile->setAnchorPoint({0.5f, 0.5f});
                    m_mapCanvas->addChild(tile, 1);
                    m_visibleTiles[index] = tile;
                }
            }
        }

        std::vector<int> stale;
        for (auto const& [index, tile] : m_visibleTiles) {
            if (!needed.contains(index)) stale.push_back(index);
        }
        for (auto index : stale) {
            auto path = tilePath(index);
            if (auto tile = m_visibleTiles[index]) tile->removeFromParentAndCleanup(true);
            auto pathString = geode::utils::string::pathToString(path);
            CCTextureCache::sharedTextureCache()->removeTextureForKey(pathString.c_str());
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
        if (auto layer = PLDebugMapLayer::create()) scene->addChild(layer);
        return scene;
    }

    bool init() override {
        if (!CCLayerColor::initWithColor({58, 58, 58, 255})) return false;

        auto win = CCDirector::sharedDirector()->getWinSize();
        // Coordinates are always expressed in Normal-graphics units. The
        // individual tile sprites are fitted to these units above.
        m_assetScale = 1.f;

        m_mapCanvas = CCNode::create();
        // Start on the snowy route beside the small ice monsters.
        auto playerX = 1020.f / m_assetScale * kMapZoom;
        auto playerY = 1760.f / m_assetScale * kMapZoom;
        auto startX = std::max(win.width - mapWidth(), std::min(0.f, win.width / 2.f - playerX));
        auto startY = std::max(win.height - mapHeight(), std::min(0.f, win.height / 2.f - playerY));
        m_mapCanvas->setPosition({startX, startY});
        addChild(m_mapCanvas, 1);

        auto gameManager = GameManager::sharedState();
        auto shadowPath = Mod::get()->getResourcesDir() / "PLMapShadow.png";
        auto shadowPathString = geode::utils::string::pathToString(shadowPath);
        auto shadow = CCSprite::create(shadowPathString.c_str());
        if (shadow) {
            auto size = shadow->getContentSize();
            auto baseShadowX = size.width > 0.f ? 27.f / size.width : 1.f;
            auto baseShadowY = size.height > 0.f ? 8.f / size.height : 1.f;
            shadow->setScaleX(baseShadowX);
            shadow->setScaleY(baseShadowY);
            shadow->setPosition({playerX, playerY - 12.f});
            shadow->setID("map-player-shadow");
            m_mapCanvas->addChild(shadow, 19);
            shadow->runAction(CCRepeatForever::create(CCSequence::create(
                CCScaleTo::create(0.08f, baseShadowX, baseShadowY),
                CCEaseSineOut::create(CCScaleTo::create(0.26f, baseShadowX * 0.62f, baseShadowY * 0.72f)),
                CCEaseSineIn::create(CCScaleTo::create(0.26f, baseShadowX, baseShadowY)),
                CCScaleTo::create(0.08f, baseShadowX, baseShadowY),
                CCDelayTime::create(0.20f),
                nullptr
            )));
        }

        auto player = SimplePlayer::create(gameManager->getPlayerFrame());
        player->setColor(gameManager->colorForIdx(gameManager->getPlayerColor()));
        player->setSecondColor(gameManager->colorForIdx(gameManager->getPlayerColor2()));
        if (gameManager->getPlayerGlow()) {
            player->setGlowOutline(
                gameManager->colorForIdx(gameManager->getPlayerGlowColor())
            );
        }
        player->updateColors();
        auto playerSize = player->getContentSize();
        auto playerExtent = std::max(playerSize.width, playerSize.height);
        auto basePlayerScale = playerExtent > 0.f ? 36.f / playerExtent : 0.58f;
        player->setScale(basePlayerScale);
        player->setPosition({playerX, playerY});
        player->setID("map-player");
        m_mapCanvas->addChild(player, 20);
        player->runAction(CCRepeatForever::create(CCSequence::create(
            CCScaleTo::create(0.08f, basePlayerScale * 1.09f, basePlayerScale * 0.87f),
            CCSpawn::create(
                CCEaseSineOut::create(CCMoveBy::create(0.26f, {0.f, 10.f})),
                CCScaleTo::create(0.26f, basePlayerScale * 0.91f, basePlayerScale * 1.11f),
                nullptr
            ),
            CCSpawn::create(
                CCEaseSineIn::create(CCMoveBy::create(0.26f, {0.f, -10.f})),
                CCScaleTo::create(0.26f, basePlayerScale * 1.09f, basePlayerScale * 0.87f),
                nullptr
            ),
            CCScaleTo::create(0.08f, basePlayerScale, basePlayerScale),
            CCDelayTime::create(0.20f),
            nullptr
        )));
        refreshVisibleTiles();

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        addChild(menu, 10);

        auto arrow = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
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
            geode::utils::string::pathToString(music), true, 0.5f, 0
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
        position.x = std::max(win.width - mapWidth(), std::min(0.f, position.x));
        position.y = std::max(win.height - mapHeight(), std::min(0.f, position.y));
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
            CCTransitionFade::create(0.35f, CreatorLayer::scene())
        );
    }
};

CCScene* createRestoredMapScene() {
    // The Android beta calls MapSelectLayer::scene directly, but that symbol
    // is not exported by the Windows 2.2081 executable. Keep the portable map
    // shell here while its interactive objects are recreated for Windows.
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

CCScene* createVersusBlackScreen() {
    auto scene = CCScene::create();
    auto black = CCLayerColor::create({0, 0, 0, 255});
    scene->addChild(black);
    return scene;
}

class $modify(PLCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;

        // Also recover the unlock for players who completed the final Tower
        // level before installing this mod.
        if (GameStatsManager::sharedState()->hasCompletedMainLevel(5004)) {
            Mod::get()->setSavedValue("the-map-unlocked", true);
        }

        if (Mod::get()->getSettingValue<bool>("enable-the-map")) {
            // The Map is available and must look fully lit.
            enableCreatorButton(this, "map-button", true);
            applyColoredCreatorArtwork(
                this, "map-button", "GJ_mapBtn_001.png"
            );
        }

        if (Mod::get()->getSettingValue<bool>("enable-versus")) {
            // Versus is deliberately exposed even though its official mode
            // has not shipped yet. Restore the full-colour native artwork.
            enableCreatorButton(this, "versus-button", true);
            applyColoredCreatorArtwork(
                this, "versus-button", "GJ_versusBtn_001.png"
            );
        }

        if (Mod::get()->getSettingValue<bool>("enable-debug-map")) {
            auto debugSprite = ButtonSprite::create(
                "DEBUG MAP", 92, true, "goldFont.fnt",
                "GJ_button_04.png", 30.f, 0.48f
            );
            auto debugButton = CCMenuItemSpriteExtra::create(
                debugSprite,
                this,
                menu_selector(PLCreatorLayer::onDebugMap)
            );
            debugButton->setPosition({-72.f, 0.f});

            auto dialogSprite = ButtonSprite::create(
                "DEBUG DIALOG", 108, true, "goldFont.fnt",
                "GJ_button_04.png", 30.f, 0.42f
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
                CCTransitionFade::create(0.35f, createRestoredMapScene())
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
        // The official mode has no usable scene in 2.2081. Keep its restored
        // button visible, but make an accidental tap completely harmless.
        return;
    }

    void onDebugMap(CCObject*) {
        CCDirector::sharedDirector()->replaceScene(
            CCTransitionFade::create(0.35f, createRestoredMapScene())
        );
    }

    void onDebugMapDialog(CCObject*) {
        showLockedMapDialog();
    }
};

class $modify(PLLevelAreaInnerLayer, LevelAreaInnerLayer) {
    void showFloor1CompleteDialog() {
        // This is called by the game after the last room of the current Tower
        // floor is completed. Persist the unlock so The Map stays available
        // after restarting Geometry Dash.
        Mod::get()->setSavedValue("the-map-unlocked", true);
        LevelAreaInnerLayer::showFloor1CompleteDialog();
    }
};

class $modify(PLGJMapPack, GJMapPack) {
    int completedMaps() {
        if (!needsLegacyGauntletCompatibility(m_packID)) {
            return GJMapPack::completedMaps();
        }

        rememberNewGauntletLevels(this);
        int completed = 0;
        if (!m_levels) return completed;

        auto manager = GameLevelManager::sharedState();
        auto stats = GameStatsManager::sharedState();
        for (auto object : CCArrayExt<CCObject*>(m_levels)) {
            auto value = typeinfo_cast<CCString*>(object);
            if (!value) continue;
            auto levelID = value->intValue();
            if (levelID <= 0) continue;

            bool done = stats->hasCompletedGauntletLevel(levelID);
            if (auto level = manager->getSavedLevel(levelID)) {
                done = done || static_cast<int>(level->m_normalPercent) >= 100;
            }
            if (done) ++completed;
        }
        return completed;
    }
};

class $modify(PLGameStatsManager, GameStatsManager) {
    bool hasCompletedGauntletLevel(int levelID) {
        if (GameStatsManager::hasCompletedGauntletLevel(levelID)) return true;
        if (!s_newGauntletLevelIDs.contains(levelID)) return false;

        if (auto level = GameLevelManager::sharedState()->getSavedLevel(levelID)) {
            return static_cast<int>(level->m_normalPercent) >= 100;
        }
        return false;
    }

    gd::string getGauntletRewardKey(int id) {
        if (s_gauntletVisualOverride && id != s_gauntletVisualOverride) {
            id = s_gauntletVisualOverride;
        }
        return GameStatsManager::getGauntletRewardKey(id);
    }

};

class $modify(PLGauntletLayer, GauntletLayer) {
    void setupGauntlet(CCArray* levels) {
        auto id = static_cast<int>(m_gauntletType);
        if (!needsLegacyGauntletCompatibility(id)) {
            GauntletLayer::setupGauntlet(levels);
            return;
        }

        auto originalType = m_gauntletType;
        s_gauntletVisualOverride = id;
        m_gauntletType = id == 61 ? GauntletType::Death : GauntletType::Space;
        GauntletLayer::setupGauntlet(levels);
        m_gauntletType = originalType;
        s_gauntletVisualOverride = 0;
    }
};

class $modify(PLGauntletSprite, GauntletSprite) {
    void toggleLockedSprite(bool locked) {
        GauntletSprite::toggleLockedSprite(locked);

        auto id = s_gauntletVisualOverride;
        if (!needsLegacyGauntletCompatibility(id)) return;
        if (getChildByID("plstudio-inner-gauntlet-island")) return;

        auto path = Mod::get()->getResourcesDir() / fmt::format(
            "PL{}{}.png",
            id == 61 ? "Duality" : "Paradox",
            graphicsSuffix()
        );
        auto pathString = geode::utils::string::pathToString(path);
        auto island = CCSprite::create(pathString.c_str());
        if (!island) return;

        auto size = island->getContentSize();
        auto extent = std::max(size.width, size.height);
        island->setScale(extent > 0.f ? 62.f / extent : 1.f);
        island->setPosition(getContentSize() / 2.f);
        island->setID("plstudio-inner-gauntlet-island");
        if (locked) {
            island->setColor({105, 105, 105});
        }
        addChild(island, -2);
    }
};

class $modify(PLGauntletNode, GauntletNode) {
    struct Fields {
        int pendingRewardID = 0;
    };

    void onClaimReward() {
        if (!m_gauntlet || !needsLegacyGauntletCompatibility(m_gauntlet->m_packID)) {
            GauntletNode::onClaimReward();
            return;
        }

        auto id = m_gauntlet->m_packID;
        if (GameStatsManager::sharedState()->isGauntletChestUnlocked(id)) {
            CCDirector::sharedDirector()->replaceScene(
                CCTransitionFade::create(
                    0.35f,
                    GauntletLayer::scene(static_cast<GauntletType>(id))
                )
            );
            return;
        }
        auto total = m_gauntlet->totalMaps();
        if (total <= 0 || m_gauntlet->completedMaps() < total) return;

        auto stats = GameStatsManager::sharedState();
        auto shard = id == 61
            ? SpecialRewardItem::MetalShard
            : SpecialRewardItem::SoulShard;
        auto shardStat = GJRewardItem::rewardItemToStat(shard);
        auto shardMaxed = stats->getStat(shardStat.c_str()) >= 100;
        auto orbs = shardMaxed ? 300 : 0;
        auto diamonds = id == 61 ? 40 : 80;
        auto unlockType = id == 61 ? UnlockType::Cube : UnlockType::Bird;
        auto unlockID = id == 61 ? 362 : 134;

        auto reward = GJRewardItem::createSpecial(
            GJRewardType::Large,
            orbs,
            diamonds,
            shard,
            shardMaxed ? 0 : 3,
            SpecialRewardItem::BonusKey,
            1,
            static_cast<int>(unlockType),
            unlockID
        );
        if (!reward) return;

        // Feed the exact 2.209 reward into Geometry Dash's own special-chest
        // table, then let the original gauntlet handler perform its native
        // chest-opening animation, reward registration, sounds and timing.
        // This deliberately avoids a custom CurrencyRewardLayer, whose items
        // looked like they were fired out of a bomb in the Android test.
        auto rewardKey = stats->getGauntletRewardKey(id);
        stats->m_allSpecialChests->setObject(reward, rewardKey);

        if (m_fields->pendingRewardID != 0) return;
        m_fields->pendingRewardID = id;

        // Stay on the gauntlet list. Animate only the chest; do not navigate
        // into GauntletLayer while the reward is being claimed.
        if (m_rewardNode) {
            auto base = m_rewardNode->getScale();
            m_rewardNode->runAction(CCSequence::create(
                CCEaseSineOut::create(CCScaleTo::create(0.16f, base * 0.90f)),
                CCEaseBackOut::create(CCScaleTo::create(0.34f, base * 1.18f)),
                CCScaleTo::create(0.14f, base),
                CCDelayTime::create(0.28f),
                nullptr
            ));
        }
        showUnlockAnimation();
        scheduleOnce(schedule_selector(PLGauntletNode::finishNativeGauntletReward), 0.94f);
    }

    void finishNativeGauntletReward(float) {
        auto id = m_fields->pendingRewardID;
        m_fields->pendingRewardID = 0;
        if (!needsLegacyGauntletCompatibility(id)) return;

        auto stats = GameStatsManager::sharedState();
        auto reward = stats->unlockGauntletChest(id);
        if (!reward) return;

        // The original card callback only performs the final chest -> island
        // swap. At this point the native chest state is already permanent.
        GauntletNode::onClaimReward();

        auto shard = id == 61
            ? SpecialRewardItem::MetalShard
            : SpecialRewardItem::SoulShard;
        auto shardStat = GJRewardItem::rewardItemToStat(shard);
        auto shardMaxed = stats->getStat(shardStat.c_str()) >= 100;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto currency = CurrencyRewardLayer::create(
            shardMaxed ? 300 : 0,
            0,
            0,
            id == 61 ? 40 : 80,
            CurrencySpriteType::DemonKey,
            1,
            id == 61 ? CurrencySpriteType::MetalShard : CurrencySpriteType::SoulShard,
            shardMaxed ? 0 : 3,
            win / 2.f,
            CurrencyRewardType::Treasure,
            0.f,
            1.f
        );
        if (currency) {
            CCDirector::sharedDirector()->getRunningScene()->addChild(currency, 1000);
        }
    }

    void generateNode() {
        if (
            m_gauntlet &&
            (m_gauntlet->m_packID == 61 || m_gauntlet->m_packID == 62)
        ) {
            auto originalID = m_gauntlet->m_packID;
            s_gauntletVisualOverride = originalID;

            // 2.2081 has no colour definitions for these two newer types.
            // Their official 2.209 cards reuse the dark Death and navy Space
            // palettes; only the name and island artwork are new.
            m_gauntlet->m_packID = originalID == 61
                ? static_cast<int>(GauntletType::Death)
                : static_cast<int>(GauntletType::Space);
            GauntletNode::generateNode();

            m_gauntlet->m_packID = originalID;
            s_gauntletVisualOverride = 0;

            // generateNode had to borrow an older palette, but progress must
            // remain attached to the real server gauntlet. Correct the two
            // native containers after restoring the genuine ID: five
            // completed server levels replace the island with the standard
            // reward chest, exactly like an ordinary gauntlet.
            rememberNewGauntletLevels(m_gauntlet);
            auto completed = m_gauntlet->completedMaps();
            auto total = m_gauntlet->totalMaps();
            auto rewardReady = total > 0 && completed >= total;
            // Ignore legacy mod flags left by the old custom reward screen.
            // Only Geometry Dash's own special-chest state is authoritative.
            auto rewardClaimed =
                GameStatsManager::sharedState()->isGauntletChestUnlocked(originalID);
            auto showChest = rewardReady && !rewardClaimed;
            if (m_gauntletInfoNode) m_gauntletInfoNode->setVisible(!showChest);
            if (m_rewardNode) m_rewardNode->setVisible(showChest);

            if (originalID == 61) {
                // Duality borrows the dark Death card background in 2.2081,
                // but its official title and progress text are white.
                std::function<void(CCNode*)> fixDualityText = [&](CCNode* node) {
                    if (auto label = typeinfo_cast<CCLabelBMFont*>(node)) {
                        auto color = label->getColor();
                        if (color.r > 200 && color.g < 110 && color.b < 110) {
                            label->setColor({255, 255, 255});
                        }
                    }
                    if (auto children = node->getChildren()) {
                        for (auto child : CCArrayExt<CCNode*>(children)) {
                            fixDualityText(child);
                        }
                    }
                };
                fixDualityText(this);
            }

            auto iconPath = Mod::get()->getResourcesDir() / fmt::format(
                "PL{}{}.png",
                originalID == 61 ? "Duality" : "Paradox",
                graphicsSuffix()
            );
            auto iconPathString = geode::utils::string::pathToString(iconPath);
            if (auto icon = CCSprite::create(iconPathString.c_str())) {
                auto size = icon->getContentSize();
                auto extent = std::max(size.width, size.height);
                auto scale = extent > 0.f ? 64.f / extent : 1.f;
                icon->setScale(scale);
                icon->setPosition({0.f, 28.f});
                // Once all five levels are complete, the native reward chest
                // owns this space. Do not leave the decorative island over it.
                icon->setVisible(!showChest);
                icon->setID("plstudio-gauntlet-icon");
                addChild(icon, 20);
            }
            return;
        }
        GauntletNode::generateNode();
    }

    static gd::string nameForType(GauntletType type) {
        auto id = s_gauntletVisualOverride
            ? s_gauntletVisualOverride
            : static_cast<int>(type);
        if (id == 61) return "Duality";
        if (id == 62) return "Paradox";

        auto name = GauntletNode::nameForType(type);
        // The server may publish a gauntlet type newer than this executable.
        // Keep the server entry visible instead of dropping or crashing it.
        return name.empty() ? gd::string("Unknown") : name;
    }

    static gd::string frameForType(GauntletType type) {
        auto id = s_gauntletVisualOverride
            ? s_gauntletVisualOverride
            : static_cast<int>(type);
        if (id == 61) return "island_duality_001.png";
        if (id == 62) return "island_paradox_001.png";

        auto frame = GauntletNode::frameForType(type);
        if (!frame.empty()) return frame;

        // Unknown gauntlets reuse the official Fire card artwork while their
        // levels, order and rewards remain exactly as supplied by the server.
        return GauntletNode::frameForType(GauntletType::Fire);
    }
};

void reloadNewGauntletFrames() {
    auto scale = CC_CONTENT_SCALE_FACTOR();
    std::string suffix = scale >= 3.5f ? "-uhd" : (scale >= 1.5f ? "-hd" : "");
    auto sheet = Mod::get()->getResourcesDir() /
        fmt::format("PLGauntletSheet{}.plist", suffix);
    auto texture = Mod::get()->getResourcesDir() /
        fmt::format("PLGauntletSheet{}.png", suffix);
    auto cache = CCSpriteFrameCache::sharedSpriteFrameCache();
    cache->removeSpriteFrameByName("island_duality_001.png");
    cache->removeSpriteFrameByName("island_paradox_001.png");
    auto sheetString = geode::utils::string::pathToString(sheet);
    auto textureString = geode::utils::string::pathToString(texture);
    cache->addSpriteFramesWithFile(sheetString.c_str(), textureString.c_str());
}

class $modify(PLGauntletSelectLayer, GauntletSelectLayer) {
    bool init(int unused) {
        // The game may purge custom textures when graphics quality changes.
        // Register the matching base/HD/UHD frames immediately before the
        // native gauntlet cards are constructed.
        reloadNewGauntletFrames();
        return GauntletSelectLayer::init(unused);
    }

    void loadLevelsFinished(CCArray* gauntlets, char const* key, int type) {
        reloadNewGauntletFrames();
        GauntletSelectLayer::loadLevelsFinished(gauntlets, key, type);
    }
};

class $modify(PLMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        // Keep the main menu visually identical to the game. Geode remains
        // accessible from the native More Options window below.
        if (auto geodeButton = typeinfo_cast<CCMenuItemSpriteExtra*>(
            getChildByIDRecursive("geode.loader/geode-button")
        )) {
            auto parent = geodeButton->getParent();
            geodeButton->retain();
            geodeButton->removeFromParentAndCleanup(false);
            s_nativeGeodeButton = geodeButton;
            if (parent) parent->updateLayout();
        }

        // The Android build does not always create More Games. Restore the
        // native button only when it is genuinely absent.
        if (!getChildByIDRecursive("more-games-button")) {
            if (auto sprite = CCSprite::createWithSpriteFrameName("GJ_moreGamesBtn_001.png")) {
                auto button = CCMenuItemSpriteExtra::create(
                    sprite, this, menu_selector(PLMenuLayer::onOpenMoreGames)
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

        // Geometry Dash 2.2081 predates gauntlets 61 and 62, while the user's
        // official 2.209 files already contain their artwork. The mod ships a
        // read-only copy of that atlas and registers only the two missing
        // frames, leaving both game installations untouched.
        auto scale = CC_CONTENT_SCALE_FACTOR();
        std::string suffix = scale >= 3.5f ? "-uhd" : (scale >= 1.5f ? "-hd" : "");
        static std::string s_loadedGauntletSuffix = "__none__";
        if (s_loadedGauntletSuffix != suffix) {
            auto cache = CCSpriteFrameCache::sharedSpriteFrameCache();
            if (s_loadedGauntletSuffix != "__none__") {
                auto oldSheet = Mod::get()->getResourcesDir() /
                    fmt::format("PLGauntletSheet{}.plist", s_loadedGauntletSuffix);
                auto oldSheetString = geode::utils::string::pathToString(oldSheet);
                cache->removeSpriteFramesFromFile(oldSheetString.c_str());
            }
            auto sheet = Mod::get()->getResourcesDir() /
                fmt::format("PLGauntletSheet{}.plist", suffix);
            auto texture = Mod::get()->getResourcesDir() /
                fmt::format("PLGauntletSheet{}.png", suffix);
            auto sheetString = geode::utils::string::pathToString(sheet);
            auto textureString = geode::utils::string::pathToString(texture);
            cache->addSpriteFramesWithFile(sheetString.c_str(), textureString.c_str());
            s_loadedGauntletSuffix = suffix;
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
        if (s_nativeGeodeButton && !s_nativeGeodeButton->getParent()) {
            s_nativeGeodeButton->setPosition({0.f, 0.f});
            menu->addChild(s_nativeGeodeButton);
        }
        addChild(menu, 200);
    }

    void layerHidden() {
        if (s_nativeGeodeButton && s_nativeGeodeButton->getParent()) {
            s_nativeGeodeButton->removeFromParentAndCleanup(false);
        }
        OptionsLayer::layerHidden();
    }
};
