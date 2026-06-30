#include "core/Game.h"
#include "utils/Logger.h"
#include "utils/TextureGenerator.h"
#include "ecs/Component.h"
#include "gameplay/PlayerCombat.h"
#include "gameplay/CombatEffects.h"
#include "gameplay/SkillSystem.h"
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <chrono>
#include <algorithm>

namespace cu {

// ============================================================================
// 渲染圣物查看面板（R 键切换）
// ----------------------------------------------------------------------------
// 显示玩家当前已获得的圣物列表与构筑概览，让玩家随时审视自己的 Build。
// 布局：3x2 共 6 个槽位，已拥有的槽位显示图标色块+名称+描述，空槽显示"空缺"占位。
// ============================================================================
void Game::renderRelicPanel() {
    const sf::Font& font = resources_.GetDefaultFont();

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(1280.f, 720.f));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    window_.draw(overlay);

    // 主面板（居中）
    const float panelX = 280.f;
    const float panelY = 130.f;
    const float panelW = 720.f;
    const float panelH = 460.f;
    sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
    panel.setPosition(panelX, panelY);
    panel.setFillColor(sf::Color(20, 25, 40, 240));
    panel.setOutlineColor(sf::Color(180, 150, 80));
    panel.setOutlineThickness(2.f);
    window_.draw(panel);

    // 标题
    sf::Text title;
    title.setFont(font);
    title.setString(U8("圣物 (Build 构筑)"));
    title.setCharacterSize(28);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(sf::Color(255, 220, 100));
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition(panelX + (panelW - tb.width) * 0.5f, panelY + 16.f);
    window_.draw(title);

    // 副标题：当前数量
    const auto& owned = relicSystem_.GetOwnedRelics();
    int ownedCount = relicSystem_.GetOwnedCount();
    sf::Text countText;
    countText.setFont(font);
    countText.setString(U8("已获得: ") + std::to_string(ownedCount) + U8(" / 6"));
    countText.setCharacterSize(18);
    countText.setFillColor(sf::Color(200, 220, 240));
    sf::FloatRect cb = countText.getLocalBounds();
    countText.setPosition(panelX + (panelW - cb.width) * 0.5f, panelY + 56.f);
    window_.draw(countText);

    // 6 个圣物槽位 (3 列 x 2 行)
    const float slotW = 200.f;
    const float slotH = 140.f;
    const float gap = 20.f;
    const float totalW = 3 * slotW + 2 * gap;       // 640
    const float startX = panelX + (panelW - totalW) * 0.5f;  // 320
    const float startY = panelY + 100.f;

    for (int i = 0; i < kRelicMaxCount; ++i) {
        int col = i % 3;
        int row = i / 3;
        float x = startX + col * (slotW + gap);
        float y = startY + row * (slotH + gap);

        RelicType rt = owned[i];

        // 槽位背景
        sf::RectangleShape slotBg(sf::Vector2f(slotW, slotH));
        slotBg.setPosition(x, y);
        if (rt != RelicType::None) {
            const RelicData& rd = GetRelicData(rt);
            slotBg.setFillColor(sf::Color(30, 30, 40, 220));
            slotBg.setOutlineColor(sf::Color(rd.r, rd.g, rd.b));
            slotBg.setOutlineThickness(2.f);
        } else {
            slotBg.setFillColor(sf::Color(20, 20, 25, 180));
            slotBg.setOutlineColor(sf::Color(80, 80, 80, 150));
            slotBg.setOutlineThickness(1.f);
        }
        window_.draw(slotBg);

        if (rt != RelicType::None) {
            const RelicData& rd = GetRelicData(rt);

            // 图标色块（圣物主色调）
            sf::RectangleShape icon(sf::Vector2f(64.f, 64.f));
            icon.setPosition(x + (slotW - 64.f) * 0.5f, y + 12.f);
            icon.setFillColor(sf::Color(rd.r, rd.g, rd.b, 220));
            icon.setOutlineColor(sf::Color::White);
            icon.setOutlineThickness(1.f);
            window_.draw(icon);

            // 圣物名称
            sf::Text nameText;
            nameText.setFont(font);
            nameText.setString(utf8ToSfString(rd.name));
            nameText.setCharacterSize(16);
            nameText.setStyle(sf::Text::Bold);
            nameText.setFillColor(sf::Color(rd.r, rd.g, rd.b));
            sf::FloatRect nb = nameText.getLocalBounds();
            nameText.setPosition(x + (slotW - nb.width) * 0.5f, y + 84.f);
            window_.draw(nameText);

            // 圣物描述
            sf::Text descText;
            descText.setFont(font);
            descText.setString(utf8ToSfString(rd.desc));
            descText.setCharacterSize(13);
            descText.setFillColor(sf::Color(220, 220, 220));
            sf::FloatRect db = descText.getLocalBounds();
            descText.setPosition(x + (slotW - db.width) * 0.5f, y + 108.f);
            window_.draw(descText);

            // ---- 第三十一轮新增：圣物叙事文本（lore）----
            if (rd.lore && rd.lore[0] != '\0') {
                sf::Text loreText;
                loreText.setFont(font);
                loreText.setString(utf8ToSfString(rd.lore));
                loreText.setCharacterSize(10);
                loreText.setFillColor(sf::Color(160, 160, 180));
                loreText.setStyle(sf::Text::Italic);
                // 自动换行：每行最多 28 个字符
                std::string loreStr(rd.lore);
                std::string wrapped;
                int charCount = 0;
                for (size_t ci = 0; ci < loreStr.size();) {
                    unsigned char c = static_cast<unsigned char>(loreStr[ci]);
                    int charLen = 1;
                    if ((c & 0xE0) == 0xC0) charLen = 2;
                    else if ((c & 0xF0) == 0xE0) charLen = 3;
                    else if ((c & 0xF8) == 0xF0) charLen = 4;
                    wrapped += loreStr.substr(ci, charLen);
                    ci += charLen;
                    ++charCount;
                    if (charCount >= 28 && ci < loreStr.size()) {
                        wrapped += '\n';
                        charCount = 0;
                    }
                }
                loreText.setString(utf8ToSfString(wrapped));
                sf::FloatRect lb = loreText.getLocalBounds();
                loreText.setPosition(x + (slotW - lb.width) * 0.5f, y + 122.f);
                window_.draw(loreText);
            }
        } else {
            // 空缺占位
            sf::Text emptyText;
            emptyText.setFont(font);
            emptyText.setString(U8("— 空缺 —"));
            emptyText.setCharacterSize(14);
            emptyText.setFillColor(sf::Color(120, 120, 120));
            sf::FloatRect eb = emptyText.getLocalBounds();
            emptyText.setPosition(x + (slotW - eb.width) * 0.5f,
                                  y + (slotH - eb.height) * 0.5f - eb.top);
            window_.draw(emptyText);
        }
    }

    // 底部提示
    sf::Text hint;
    hint.setFont(font);
    hint.setString(U8("按 R 或 ESC 关闭"));
    hint.setCharacterSize(14);
    hint.setFillColor(sf::Color(160, 160, 160));
    sf::FloatRect hb = hint.getLocalBounds();
    hint.setPosition(panelX + (panelW - hb.width) * 0.5f, panelY + panelH - 30.f);
    window_.draw(hint);
}

// ============================================================================
// Phase 8: 渲染 UI
// ============================================================================
void Game::renderUI() {
    // HUD 仅在 Playing 状态渲染
    if (state_ == GameState::Playing) {
        hud_.Render(window_);

        // 第十七轮新增：地牢变异系统 UI
        //   Banner：进入新层时 5 秒淡入淡出提示（覆盖在 HUD 之上，模态菜单之下）
        //   HUD 指示器：左上角持久显示当前层激活的变异名
        renderFloorModifierHUD();
        renderFloorModifierBanner();

        if (upgradeChoiceActive_) {
            upgradeMenu_.Render(window_);
        }

        // 圣物选择菜单（Boss 击败后 3 选 1，第十五轮新增）
        if (relicChoiceActive_) {
            relicMenu_.Render(window_);
        }

        if (inventoryMenuVisible_) {
            inventoryMenu_.Render(window_);
        }

        // 圣物查看面板（R 键切换，第十五轮新增）
        if (relicPanelVisible_) {
            renderRelicPanel();
        }
    }
}

// ============================================================================
// Playing 状态：渲染
// ============================================================================
void Game::renderPlaying(float /*alpha*/) {
    // 清屏
    window_.clear(sf::Color(20, 50, 30));

    const sf::Texture* atlasTexture = atlas_.GetTexture();

    // 1. 开始场景：设置摄像机
    renderer_.BeginScene(window_, camera_);

    // ---- Phase 6: 渲染地牢 TileMap ----
    if (dungeonInitialized_) {
        tileMap_.Render(renderer_, camera_);
    }

    // 2. 绘制所有拥有 Transform + Sprite 的实体
    registry_.ForEach<Transform, Sprite>([&](EntityId id) {
        Transform* t = registry_.GetComponent<Transform>(id);
        Sprite* s = registry_.GetComponent<Sprite>(id);
        if (t && s) {
            // 跳过非活跃敌人（已死亡但未回收）
            EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
            if (enemy && !enemy->active) return;

            // ---- 第十六轮新增：根据状态效果染色敌人 sprite ----
            // 让玩家直观看到哪些敌人被燃烧/冰冻/中毒/麻痹：
            //   Fire      燃烧：叠加橙红色 tint（255, 110, 50）
            //   Ice       冰冻：叠加青蓝色 tint（100, 180, 255）
            //   Poison    中毒：叠加毒绿色 tint（110, 200, 80）
            //   Lightning 麻痹：叠加亮黄色 tint（255, 230, 80）—— 第十九轮新增
            // 多状态时取优先级最高的（Lightning > Fire > Poison > Ice，按控制强度排序）
            //   Lightning 麻痹为硬控，视觉优先级最高，让玩家能识别"被控住的目标"
            // 实现：将 s->color 与 tint 做 50% 混合（保留原色识别度）
            sf::Color renderColor = s->color;
            if (enemy) {
                const StatusEffectComponent* statusComp =
                    registry_.GetComponent<StatusEffectComponent>(id);
                if (statusComp && !statusComp->effects.empty()) {
                    sf::Color tint = sf::Color::White;
                    bool hasTint = false;
                    int priority = 0; // 0=无, 1=Ice, 2=Poison, 3=Fire, 4=Lightning
                    for (const auto& eff : statusComp->effects) {
                        if (eff.type == ElementType::Lightning && priority < 4) {
                            tint = sf::Color(255, 230, 80);
                            priority = 4;
                            hasTint = true;
                        } else if (eff.type == ElementType::Fire && priority < 3) {
                            tint = sf::Color(255, 110, 50);
                            priority = 3;
                            hasTint = true;
                        } else if (eff.type == ElementType::Poison && priority < 2) {
                            tint = sf::Color(110, 200, 80);
                            priority = 2;
                            hasTint = true;
                        } else if (eff.type == ElementType::Ice && priority < 1) {
                            tint = sf::Color(100, 180, 255);
                            priority = 1;
                            hasTint = true;
                        }
                    }
                    if (hasTint) {
                        // 50% 混合：保留原色识别度，同时呈现状态色
                        renderColor.r = static_cast<sf::Uint8>((s->color.r + tint.r) / 2);
                        renderColor.g = static_cast<sf::Uint8>((s->color.g + tint.g) / 2);
                        renderColor.b = static_cast<sf::Uint8>((s->color.b + tint.b) / 2);
                        renderColor.a = s->color.a;
                    }
                }

                // ---- 第二十一轮新增：词缀精英紫色发光边缘 ----
                // 激活 EnemyAffix 词缀系统的视觉反馈：词缀敌人 sprite 与紫色 (180, 80, 255)
                // 做 30% 混合（轻度染色，比状态效果弱，避免覆盖元素状态色）。
                // 让玩家能视觉识别"词缀精英"并优先击杀，与紫色光环粒子呼应。
                // 不覆盖 Champion 金色描边（isChampion 的金色血条仍渲染在头顶）。
                const EnemyAffix* affix = registry_.GetComponent<EnemyAffix>(id);
                if (affix && affix->affixMask != 0u) {
                    constexpr float kMix = 0.3f; // 30% 紫色混合
                    renderColor.r = static_cast<sf::Uint8>(renderColor.r * (1.f - kMix) + 180.f * kMix);
                    renderColor.g = static_cast<sf::Uint8>(renderColor.g * (1.f - kMix) + 80.f  * kMix);
                    renderColor.b = static_cast<sf::Uint8>(renderColor.b * (1.f - kMix) + 255.f * kMix);
                }
            }

            renderer_.DrawSprite(atlasTexture, t->position,
                                 s->sourceRect, renderColor, t->scale);
        }
    });

    // 3. 绘制粒子
    particles_.Render(renderer_);

    // 3.5 绘制掉落物（战利品光圈）
    lootSystem_.Render(renderer_);

    // 3.6 绘制经验球（绿色发光圆点）
    expOrbSystem_.Render(renderer_);

    // 3.7 绘制金币（金色发光圆点）
    coinSystem_.Render(renderer_);

    // 3.8 绘制爱心（红色发光，Boss 召唤物掉落）
    heartSystem_.Render(renderer_);

    // 4. 结束场景
    renderer_.EndScene();

    // 4.5 渲染商人头顶文字（世界空间，此时 view 仍为摄像机视图）
    // 直接在商人世界位置上方绘制，摄像机自动处理坐标转换
    if (merchantSystem_.IsActive()) {
        sf::Vector2f merchantWorldPos = merchantSystem_.GetPosition();

        sf::Text merchantLabel;
        merchantLabel.setFont(resources_.GetDefaultFont());
        merchantLabel.setString(U8("神秘商人"));
        merchantLabel.setCharacterSize(14);
        merchantLabel.setFillColor(sf::Color(255, 220, 100));
        merchantLabel.setStyle(sf::Text::Bold);
        merchantLabel.setOutlineColor(sf::Color(0, 0, 0, 200));
        merchantLabel.setOutlineThickness(2.f);
        sf::FloatRect mb = merchantLabel.getLocalBounds();
        merchantLabel.setOrigin(mb.width * 0.5f, mb.height * 0.5f);
        merchantLabel.setPosition(merchantWorldPos.x, merchantWorldPos.y - 36.f);
        window_.draw(merchantLabel);

        // 玩家靠近时显示 "按 E 交易" 提示
        Transform* pT = registry_.GetComponent<Transform>(playerId_);
        if (pT && merchantSystem_.IsPlayerInRange(pT->position)) {
            sf::Text hint;
            hint.setFont(resources_.GetDefaultFont());
            hint.setString(U8("按 E 交易"));
            hint.setCharacterSize(12);
            hint.setFillColor(sf::Color(180, 255, 180));
            hint.setOutlineColor(sf::Color(0, 0, 0, 200));
            hint.setOutlineThickness(2.f);
            sf::FloatRect hb = hint.getLocalBounds();
            hint.setOrigin(hb.width * 0.5f, hb.height * 0.5f);
            hint.setPosition(merchantWorldPos.x, merchantWorldPos.y - 52.f);
            window_.draw(hint);
        }
    }

    // 4.6 渲染 Boss 冲撞地裂区域（世界空间，在摄像机视图下）
    renderFissureZones();

    // 4.7 渲染事件房交互提示（世界空间）
    renderEventHint();

    // 4.75 渲染精英强化怪头上小血条（世界空间，跟随摄像机）
    renderChampionHealthBars();

    // 5. 切换回屏幕空间绘制 UI（固定 1280x720 逻辑分辨率，由 SFML 自动缩放到窗口）
    window_.setView(sf::View(sf::FloatRect(0.f, 0.f, 1280.f, 720.f)));

    // 6. 渲染伤害飘字
    RenderDamageTexts(registry_, window_, camera_, resources_.GetDefaultFont());

    // 6.5 渲染门的血量条（屏幕空间）
    if (dungeonInitialized_) {
        renderDoorHealthBars();
    }

    // 6.6 渲染 BOSS 血条（屏幕顶部）
    renderBossHealthBar();

    // 6.65 渲染 BOSS 击败提示（屏幕中央上方）
    if (bossDefeatedHintTimer_ > 0.f) {
        sf::Text bossDefeatedText;
        bossDefeatedText.setFont(resources_.GetDefaultFont());
        bossDefeatedText.setString(U8("BOSS 已击败！通过通道前往下一层"));
        bossDefeatedText.setCharacterSize(24);
        bossDefeatedText.setFillColor(sf::Color(255, 220, 100));
        bossDefeatedText.setStyle(sf::Text::Bold);
        bossDefeatedText.setOutlineColor(sf::Color(0, 0, 0, 200));
        bossDefeatedText.setOutlineThickness(3.f);
        sf::FloatRect bdt = bossDefeatedText.getLocalBounds();
        bossDefeatedText.setOrigin(bdt.width * 0.5f, bdt.height * 0.5f);
        bossDefeatedText.setPosition(640.f, 120.f);
        // 闪烁效果（最后3秒闪烁）
        if (bossDefeatedHintTimer_ < 3.f) {
            float alpha = 128.f + 127.f * std::sin(bossDefeatedHintTimer_ * 10.f);
            bossDefeatedText.setFillColor(sf::Color(255, 220, 100, static_cast<uint8_t>(alpha)));
        }
        window_.draw(bossDefeatedText);
    }

    // 7. 操作提示（底部小字）
    hintText_.setCharacterSize(14);
    hintText_.setPosition(10.f, 695.f);
    hintText_.setString(U8("WASD:移动 | 左键:射击 | 右键:闪避 | 空格:AOE | E:交互/商人 | G:背包 | J:技能升级 | H:帮助 | F1:调试 | P/ESC:暂停"));
    window_.draw(hintText_);

    // 8. 调试信息（F1 切换）
    if (debugMode_) {
        std::string debug;
        debug += "FPS: " + std::to_string(time_.GetFPS()) + "\n";
        debug += "绘制调用: " + std::to_string(renderer_.GetDrawCallCount()) + "\n";
        debug += "顶点数: " + std::to_string(renderer_.GetVertexCount()) + "\n";
        debug += "实体数: " + std::to_string(registry_.GetEntityCount()) + "\n";
        debug += "粒子数: " + std::to_string(particles_.GetActiveCount()) + "\n";
        debug += "敌人数: " + std::to_string(enemySpawner_.GetAliveCount()) + "\n";
        debug += "波次: " + std::to_string(currentWaveNumber_) + "\n";
        debug += "流场耗时: " + std::to_string(lastFlowFieldTimeMs_) + " ms\n";
        debug += "AI更新: " + std::to_string(lastAIUpdateTimeMs_) + " ms\n";
        debug += "敌人池: " + std::to_string(enemySpawner_.GetFreeCount()) + "/" + std::to_string(enemySpawner_.GetPoolCapacity()) + "\n";
        debug += "子弹数: " + std::to_string(projectileSystem_.GetActiveCount()) + "\n";
        debug += "子弹池: " + std::to_string(projectileSystem_.GetFreeCount()) + "/" + std::to_string(projectileSystem_.GetPoolCapacity()) + "\n";
        debug += "子弹更新: " + std::to_string(lastProjectileTimeMs_) + " ms\n";
        debug += "战斗更新: " + std::to_string(lastCombatTimeMs_) + " ms\n";
        debug += "击杀数: " + std::to_string(totalKillCount_) + "\n";
        Transform* t = registry_.GetComponent<Transform>(playerId_);
        PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
        AnimationComponent* anim = registry_.GetComponent<AnimationComponent>(playerId_);
        if (t) {
            debug += "Player Pos: (" + std::to_string(t->position.x) + ", "
                   + std::to_string(t->position.y) + ")\n";
        }
        if (pc) {
            debug += "Facing: " + std::string(FacingDirectionName(pc->facing)) + "\n";
            debug += "Anim: " + std::string(PlayerAnimStateName(pc->animState)) + "\n";
            debug += "HP: " + std::to_string(static_cast<int>(pc->stats.currentHp))
                   + "/" + std::to_string(static_cast<int>(pc->stats.maxHp)) + "\n";
            debug += "Speed: " + std::to_string(static_cast<int>(pc->stats.moveSpeed)) + "\n";
            debug += "Atk CD: " + std::to_string(pc->attackCooldown) + "\n";
            debug += "Dodge CD: " + std::to_string(pc->dodgeCooldown) + "\n";
            debug += "AOE CD: " + std::to_string(pc->aoeCooldown) + "\n";
            debug += "Level: " + std::to_string(pc->stats.level) + "\n";
            debug += "EXP: " + std::to_string(static_cast<int>(pc->stats.exp)) + "/"
                   + std::to_string(static_cast<int>(pc->stats.expToNext)) + "\n";
        }
        if (anim) {
            debug += "Frame: " + std::to_string(anim->currentFrame) + "/"
                   + std::to_string(anim->frames.size()) + "\n";
        }
        sf::Vector2f mouseWorld = input_.GetMouseWorldPosition(camera_);
        debug += "Mouse World: (" + std::to_string(static_cast<int>(mouseWorld.x))
               + ", " + std::to_string(static_cast<int>(mouseWorld.y)) + ")\n";

        if (dungeonInitialized_) {
            debug += "--- Dungeon ---\n";
            debug += "Seed: " + std::to_string(dungeonSeed_) + "\n";
            debug += "Size: " + std::to_string(dungeon_.width) + "x" + std::to_string(dungeon_.height) + "\n";
            debug += "Rooms: " + std::to_string(dungeon_.rooms.size()) + "\n";
            debug += "Cleared: " + std::to_string(roomSystem_.GetClearedRoomCount())
                   + "/" + std::to_string(static_cast<int>(dungeon_.rooms.size())) + "\n";
            debug += "Visible Tiles: " + std::to_string(tileMap_.GetVisibleTileCount()) + "\n";
            int curRoom = roomSystem_.GetCurrentRoomIndex();
            if (curRoom >= 0 && curRoom < static_cast<int>(dungeon_.rooms.size())) {
                const Room& room = dungeon_.rooms[curRoom];
                debug += "Current Room: #" + std::to_string(curRoom)
                       + " (" + RoomTypeName(room.type) + ")\n";
            } else {
                debug += "Current Room: Corridor\n";
            }
            if (t) {
                sf::Vector2i playerTile = dungeon_.WorldToTile(t->position);
                debug += "Player Tile: (" + std::to_string(playerTile.x)
                       + ", " + std::to_string(playerTile.y) + ")\n";
                TileType pt = dungeon_.GetTile(playerTile.x, playerTile.y);
                debug += "Tile Type: " + std::string(TileTypeName(pt)) + "\n";
            }
        }
        debugText_.setString(utf8ToSfString(debug));
        window_.draw(debugText_);
    }

    // Phase 8: 渲染 HUD（血条/蓝条/经验条/技能图标/小地图/波次/FPS）
    hud_.Render(window_);

    // Phase 8: 渲染升级选择菜单
    if (upgradeChoiceActive_) {
        upgradeMenu_.Render(window_);
    }

    // Phase 8: 渲染背包菜单
    if (inventoryMenuVisible_) {
        inventoryMenu_.Render(window_);
    }

    // Phase 8: 渲染商人交易菜单
    if (merchantMenuVisible_) {
        merchantMenu_.Render(window_);
    }

    // 渲染任务面板（按 Q 切换）
    if (questMenuVisible_) {
        questMenu_.Render(window_);
    }

    // 渲染成就面板（按 Tab 切换）
    if (achievementMenuVisible_) {
        achievementMenu_.Render(window_);
    }

    // 调试面板（F5）
    if (debugPanelVisible_) {
        debugPanel_.Render(window_);
    }

    // 圣物查看面板（R 键切换，第十五轮新增）
    if (relicPanelVisible_) {
        renderRelicPanel();
    }

    // 9. 渲染按键教程覆盖层（首次进入游戏时显示）
    if (tutorialVisible_) {
        renderTutorial();
    }

    // ---- 第三十三轮新增：对话系统渲染（顶层，在最前面）----
    if (dialogueSystem_.IsActive()) {
        renderDialogueBox();
    }
}

// ============================================================================
// renderDoorHealthBars —— 渲染门的血量条（屏幕空间）
// ----------------------------------------------------------------------------
// 遍历可见区域的门，若门未满血则在其上方渲染血量条。
// 仅渲染 hp < maxHp 的门，避免满血门也显示血量条。
// ============================================================================
void Game::renderDoorHealthBars() {
    // 获取摄像机视图边界（世界坐标）
    const sf::View& view = camera_.GetView();
    sf::Vector2f viewCenter = view.getCenter();
    sf::Vector2f viewSize = view.getSize();
    sf::FloatRect viewBounds(
        viewCenter.x - viewSize.x * 0.5f,
        viewCenter.y - viewSize.y * 0.5f,
        viewSize.x,
        viewSize.y
    );

    // 转换为 tile 坐标范围
    sf::Vector2i minTile = dungeon_.WorldToTile(sf::Vector2f(viewBounds.left, viewBounds.top));
    sf::Vector2i maxTile = dungeon_.WorldToTile(
        sf::Vector2f(viewBounds.left + viewBounds.width, viewBounds.top + viewBounds.height)
    );

    // 裁剪到地牢范围
    minTile.x = std::max(0, minTile.x);
    minTile.y = std::max(0, minTile.y);
    maxTile.x = std::min(dungeon_.width - 1, maxTile.x);
    maxTile.y = std::min(dungeon_.height - 1, maxTile.y);

    // 遍历可见 tile，渲染门的血量条
    for (int ty = minTile.y; ty <= maxTile.y; ++ty) {
        for (int tx = minTile.x; tx <= maxTile.x; ++tx) {
            if (dungeon_.GetTile(tx, ty) != TileType::Door) continue;

            const DoorState* ds = dungeon_.GetDoorState(tx, ty);
            if (!ds) continue;
            // 仅未满血的门显示血量条
            if (ds->hp >= ds->maxHp) continue;

            // 门的世界坐标（中心）
            sf::Vector2f doorWorldPos = dungeon_.TileCenterToWorld(sf::Vector2i(tx, ty));
            // 转换为屏幕坐标
            sf::Vector2f screenPos = camera_.WorldToScreen(doorWorldPos);

            // 血量条尺寸
            float barWidth = 28.f;
            float barHeight = 4.f;
            float barX = screenPos.x - barWidth * 0.5f;
            float barY = screenPos.y - 24.f; // 门上方

            // 计算血量比例
            float hpRatio = ds->hp / ds->maxHp;
            if (hpRatio < 0.f) hpRatio = 0.f;
            if (hpRatio > 1.f) hpRatio = 1.f;

            // 背景（深色边框）
            sf::RectangleShape bg(sf::Vector2f(barWidth, barHeight));
            bg.setPosition(barX, barY);
            bg.setFillColor(sf::Color(40, 40, 40, 200));
            bg.setOutlineColor(sf::Color::Black);
            bg.setOutlineThickness(1.f);
            window_.draw(bg);

            // 前景（红色血量）
            float fgWidth = barWidth * hpRatio;
            if (fgWidth > 0.f) {
                sf::RectangleShape fg(sf::Vector2f(fgWidth, barHeight));
                fg.setPosition(barX, barY);
                // 血量低时变黄，极低时变红
                sf::Color fgColor;
                if (hpRatio > 0.5f) {
                    fgColor = sf::Color(220, 80, 80, 255);
                } else if (hpRatio > 0.25f) {
                    fgColor = sf::Color(220, 180, 50, 255);
                } else {
                    fgColor = sf::Color(220, 50, 50, 255);
                }
                fg.setFillColor(fgColor);
                window_.draw(fg);
            }
        }
    }
}

// ============================================================================
// renderChampionHealthBars —— 渲染精英强化怪头上小血条（世界空间）
// ----------------------------------------------------------------------------
// 遍历所有 isChampion 标记的活跃敌人，在其头顶绘制小血条
// 血条尺寸比 Boss 屏幕顶部血条小，跟随敌人移动
// 视觉规范：背景半透明黑色，前景金色（与 Boss 红色区分），高度 4px
// 第二十一轮扩展：词缀敌人（EnemyAffix::affixMask != 0）也渲染血条 + 词缀名
// ============================================================================
void Game::renderChampionHealthBars() {
    if (!dungeonInitialized_) return;

    registry_.ForEach<Transform, EnemyComponent>([&](EntityId id) {
        EnemyComponent* enemy = registry_.GetComponent<EnemyComponent>(id);
        if (!enemy || !enemy->active) return;

        // 第二十一轮：词缀敌人也显示血条（不仅是 Champion）
        const EnemyAffix* affix = registry_.GetComponent<EnemyAffix>(id);
        const bool hasAffix = (affix && affix->affixMask != 0u);
        if (!enemy->isChampion && !hasAffix) return;

        Transform* t = registry_.GetComponent<Transform>(id);
        Health* health = registry_.GetComponent<Health>(id);
        if (!t || !health) return;
        // 已死亡的不渲染（Health.current <= 0）
        if (health->current <= 0.f) return;

        // 计算血量比例
        float hpRatio = (health->max > 0.f) ? health->current / health->max : 0.f;
        if (hpRatio < 0.f) hpRatio = 0.f;
        if (hpRatio > 1.f) hpRatio = 1.f;

        // 血条尺寸（小血条，比 Boss 屏幕顶部血条小）
        // 宽度 36px，高度 4px，位于敌人头顶上方 22px
        constexpr float kBarWidth = 36.f;
        constexpr float kBarHeight = 4.f;
        constexpr float kBarOffsetY = 22.f;

        float barX = t->position.x - kBarWidth * 0.5f;
        float barY = t->position.y - kBarOffsetY;

        // 背景（半透明深色）
        sf::RectangleShape bg(sf::Vector2f(kBarWidth, kBarHeight));
        bg.setPosition(barX, barY);
        bg.setFillColor(sf::Color(20, 20, 20, 200));
        bg.setOutlineColor(sf::Color(0, 0, 0, 220));
        bg.setOutlineThickness(1.f);
        window_.draw(bg);

        // 前景血量：Champion 用金色，纯 Elite 词缀怪用紫色（区分两种精英）
        float fgWidth = kBarWidth * hpRatio;
        if (fgWidth > 0.f) {
            sf::RectangleShape fg(sf::Vector2f(fgWidth, kBarHeight));
            fg.setPosition(barX, barY);
            if (enemy->isChampion) {
                fg.setFillColor(sf::Color(255, 215, 80, 240));  // 金色
            } else {
                fg.setFillColor(sf::Color(180, 80, 255, 240));  // 紫色（词缀精英）
            }
            window_.draw(fg);
        }

        // 精英标识小三角形（左侧，标识"精英"）
        sf::ConvexShape mark;
        mark.setPointCount(3);
        mark.setPoint(0, sf::Vector2f(barX - 6.f, barY - 1.f));
        mark.setPoint(1, sf::Vector2f(barX - 1.f, barY + kBarHeight * 0.5f));
        mark.setPoint(2, sf::Vector2f(barX - 6.f, barY + kBarHeight + 1.f));
        mark.setFillColor(enemy->isChampion ? sf::Color(255, 215, 80, 240)
                                            : sf::Color(180, 80, 255, 240));
        window_.draw(mark);

        // ---- 第二十一轮新增：词缀敌人头顶显示词缀名（中文）----
        // 让玩家知道这个精英带了哪些词缀，决定是否优先击杀
        // 词缀名格式："厚血+狂暴"（多词缀用 + 连接）
        if (hasAffix) {
            std::string affixName;
            if (HasEliteAffix(affix->affixMask, EliteAffix::HpBoost))      affixName += "厚血";
            if (HasEliteAffix(affix->affixMask, EliteAffix::DamageBoost))  affixName += (affixName.empty() ? "" : "+") + std::string("狂暴");
            if (HasEliteAffix(affix->affixMask, EliteAffix::SpeedBoost))   affixName += (affixName.empty() ? "" : "+") + std::string("迅捷");
            if (HasEliteAffix(affix->affixMask, EliteAffix::Regenerating)) affixName += (affixName.empty() ? "" : "+") + std::string("回血");

            if (!affixName.empty()) {
                sf::Text affixLabel;
                affixLabel.setFont(resources_.GetDefaultFont());
                affixLabel.setString(utf8ToSfString(affixName));
                affixLabel.setCharacterSize(10);
                affixLabel.setFillColor(sf::Color(220, 180, 255));
                affixLabel.setOutlineColor(sf::Color(0, 0, 0, 220));
                affixLabel.setOutlineThickness(1.f);
                sf::FloatRect ab = affixLabel.getLocalBounds();
                affixLabel.setOrigin(ab.width * 0.5f, ab.height * 0.5f);
                affixLabel.setPosition(t->position.x, barY - 8.f);
                window_.draw(affixLabel);
            }
        }
    });
}

// ============================================================================
// renderBossHealthBar —— 渲染 BOSS 血条（屏幕顶部中央）
// ----------------------------------------------------------------------------
// 当 BOSS 存活时，在屏幕顶部中央显示半透明血条。
// ============================================================================
void Game::renderBossHealthBar() {
    if (!bossActive_ || bossEntityId_ == kInvalidEntity) return;

    Health* bossHealth = registry_.GetComponent<Health>(bossEntityId_);
    if (!bossHealth) return;

    // BOSS 已死亡，清除标志
    if (bossHealth->current <= 0.f) {
        bossActive_ = false;
        bossEntityId_ = kInvalidEntity;
        return;
    }

    // 血条尺寸（屏幕顶部中央，使用固定 1280 逻辑宽度居中）
    float barWidth = 400.f;
    float barHeight = 20.f;
    float barX = (1280.f - barWidth) * 0.5f;
    float barY = 20.f;

    // 计算血量比例
    float hpRatio = bossHealth->current / bossHealth->max;
    if (hpRatio < 0.f) hpRatio = 0.f;
    if (hpRatio > 1.f) hpRatio = 1.f;

    // 背景（半透明深色）
    sf::RectangleShape bg(sf::Vector2f(barWidth, barHeight));
    bg.setPosition(barX, barY);
    bg.setFillColor(sf::Color(20, 20, 30, 180));
    bg.setOutlineColor(sf::Color(200, 50, 50, 200));
    bg.setOutlineThickness(2.f);
    window_.draw(bg);

    // 前景（红色血量）
    float fgWidth = barWidth * hpRatio;
    if (fgWidth > 0.f) {
        sf::RectangleShape fg(sf::Vector2f(fgWidth, barHeight));
        fg.setPosition(barX, barY);
        fg.setFillColor(sf::Color(180, 30, 30, 220));
        window_.draw(fg);
    }

    // BOSS 名称
    sf::Text bossName;
    bossName.setFont(resources_.GetDefaultFont());
    bossName.setString(U8("首领"));
    bossName.setCharacterSize(14);
    bossName.setFillColor(sf::Color::White);
    bossName.setPosition(barX + barWidth * 0.5f - 20.f, barY + 2.f);
    window_.draw(bossName);
}

// ============================================================================
// renderTutorial —— 渲染按键教程覆盖层
// ----------------------------------------------------------------------------
// 半透明遮罩 + 居中标题 + 按键说明网格
// 任意键/鼠标点击后关闭
// ============================================================================
void Game::renderTutorial() {
    const sf::Font& font = resources_.GetDefaultFont();
    // 使用固定 1280x720 逻辑分辨率（与当前 View 一致），不随窗口物理尺寸变化
    float screenW = 1280.f;
    float screenH = 720.f;

    // 半透明遮罩
    sf::RectangleShape overlay(sf::Vector2f(screenW, screenH));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    window_.draw(overlay);

    // 背景卡片
    float cardW = 720.f;
    float cardH = 500.f;
    float cardX = (screenW - cardW) * 0.5f;
    float cardY = (screenH - cardH) * 0.5f;
    sf::RectangleShape card(sf::Vector2f(cardW, cardH));
    card.setPosition(cardX, cardY);
    card.setFillColor(sf::Color(25, 25, 35, 230));
    card.setOutlineColor(sf::Color(120, 100, 60));
    card.setOutlineThickness(3.f);
    window_.draw(card);

    // 标题：首次显示"操作指南"，H 键重新打开时显示"帮助手册"
    sf::Text title;
    title.setFont(font);
    title.setString(tutorialShown_ ? U8("帮助手册") : U8("操作指南"));
    title.setCharacterSize(40);
    title.setFillColor(sf::Color(255, 220, 100));
    title.setStyle(sf::Text::Bold);
    title.setOutlineColor(sf::Color(80, 40, 0));
    title.setOutlineThickness(2.f);
    sf::FloatRect tb = title.getLocalBounds();
    title.setPosition((screenW - tb.width) * 0.5f, cardY + 30.f);
    window_.draw(title);

    // 按键说明：键位 + 说明
    struct TutorialItem {
        const char* key;
        const char* desc;
    };
    TutorialItem items[] = {
        { "W A S D",        "移动" },
        { "鼠标左键",       "普通攻击" },
        { "鼠标右键",       "闪避（带无敌帧）" },
        { "空格键",         "AOE 爆炸（清怪）" },
        { "E 键",           "交互 / 商人交易" },
        { "G 键",           "打开 / 关闭背包" },
        { "数字键 1-4",     "释放技能" },
        { "P / ESC",        "暂停游戏" },
        { "J 键",           "升级选择界面" },
        { "Q 键",           "切换任务面板" },
        { "Tab 键",         "切换成就面板" },
        { "R 键",           "切换圣物面板" },
        { "H 键",           "打开帮助手册" },
        { "Enter 键",       "开始下一波" },
    };
    constexpr int kItemCount = sizeof(items) / sizeof(items[0]);
    constexpr int kCol1Count = 7; // 左列 7 项，右列 6 项

    float rowH = 38.f;
    float col1X = cardX + 40.f;
    float col2X = cardX + 390.f;
    float startY = cardY + 100.f;
    float keyBgW = 130.f;

    // 左列：items[0..kCol1Count-1]
    for (int i = 0; i < kCol1Count; ++i) {
        float y = startY + static_cast<float>(i) * rowH;
        const auto& item = items[i];

        sf::RectangleShape keyBg(sf::Vector2f(keyBgW, 32.f));
        keyBg.setPosition(col1X, y);
        keyBg.setFillColor(sf::Color(60, 50, 30, 220));
        keyBg.setOutlineColor(sf::Color(255, 220, 100));
        keyBg.setOutlineThickness(1.f);
        window_.draw(keyBg);

        sf::Text keyText;
        keyText.setFont(font);
        keyText.setString(utf8ToSfString(item.key));
        keyText.setCharacterSize(15);
        keyText.setFillColor(sf::Color(255, 220, 100));
        keyText.setStyle(sf::Text::Bold);
        sf::FloatRect kb = keyText.getLocalBounds();
        keyText.setPosition(col1X + (keyBgW - kb.width) * 0.5f,
                            y + (32.f - kb.height) * 0.5f - 2.f);
        window_.draw(keyText);

        sf::Text descText;
        descText.setFont(font);
        descText.setString(utf8ToSfString(item.desc));
        descText.setCharacterSize(16);
        descText.setFillColor(sf::Color(220, 220, 220));
        descText.setPosition(col1X + keyBgW + 12.f, y + 5.f);
        window_.draw(descText);
    }

    // 右列：items[kCol1Count..kItemCount-1]
    for (int i = kCol1Count; i < kItemCount; ++i) {
        float y = startY + static_cast<float>(i - kCol1Count) * rowH;
        const auto& item = items[i];

        sf::RectangleShape keyBg(sf::Vector2f(keyBgW, 32.f));
        keyBg.setPosition(col2X, y);
        keyBg.setFillColor(sf::Color(60, 50, 30, 220));
        keyBg.setOutlineColor(sf::Color(255, 220, 100));
        keyBg.setOutlineThickness(1.f);
        window_.draw(keyBg);

        sf::Text keyText;
        keyText.setFont(font);
        keyText.setString(utf8ToSfString(item.key));
        keyText.setCharacterSize(15);
        keyText.setFillColor(sf::Color(255, 220, 100));
        keyText.setStyle(sf::Text::Bold);
        sf::FloatRect kb = keyText.getLocalBounds();
        keyText.setPosition(col2X + (keyBgW - kb.width) * 0.5f,
                            y + (32.f - kb.height) * 0.5f - 2.f);
        window_.draw(keyText);

        sf::Text descText;
        descText.setFont(font);
        descText.setString(utf8ToSfString(item.desc));
        descText.setCharacterSize(16);
        descText.setFillColor(sf::Color(220, 220, 220));
        descText.setPosition(col2X + keyBgW + 12.f, y + 5.f);
        window_.draw(descText);
    }

    // 底部提示
    sf::Text closeHint;
    closeHint.setFont(font);
    closeHint.setString(U8("按任意键或点击鼠标关闭  |  游戏中按 H 重新打开"));
    closeHint.setCharacterSize(16);
    closeHint.setFillColor(sf::Color(180, 255, 180));
    closeHint.setStyle(sf::Text::Italic);
    sf::FloatRect cb = closeHint.getLocalBounds();
    closeHint.setPosition((screenW - cb.width) * 0.5f, cardY + cardH - 50.f);
    window_.draw(closeHint);
}

// ============================================================================
// 第十七轮新增：renderFloorModifierBanner
// ----------------------------------------------------------------------------
// 进入新层时显示 5 秒淡入淡出 Banner，提示当前层的变异效果
// 视觉：屏幕中上方 720x80 半透明背景板 + 主色调边框 + 标题 + 描述
// 动画：前 0.4s 淡入，中间 4.2s 稳定，最后 0.4s 淡出
// ============================================================================
void Game::renderFloorModifierBanner() {
    if (modifierBannerTimer_ <= 0.f) return;
    if (floorModifiers_.GetActiveCount() == 0) return;

    // 计算 alpha：前 0.4s 淡入，中间稳定，最后 0.4s 淡出
    const float fadeIn = 0.4f;
    const float fadeOut = 0.4f;
    float elapsed = kModifierBannerDuration - modifierBannerTimer_;
    float alpha = 1.f;
    if (elapsed < fadeIn) {
        alpha = elapsed / fadeIn; // 0 → 1
    } else if (modifierBannerTimer_ < fadeOut) {
        alpha = modifierBannerTimer_ / fadeOut; // 1 → 0
    }
    if (alpha < 0.f) alpha = 0.f;
    if (alpha > 1.f) alpha = 1.f;

    // 主色调：取第一个激活修饰符的颜色（多个时混合简化为首个）
    auto mods = floorModifiers_.GetActiveModifiers();
    sf::Color mainColor(255, 255, 255);
    for (auto t : mods) {
        if (t != FloorModifierType::None) {
            const auto& d = GetFloorModifierData(t);
            mainColor = sf::Color(d.r, d.g, d.b);
            break;
        }
    }

    // 备份当前 view，切换到默认 UI view
    sf::View prevView = window_.getView();
    window_.setView(window_.getDefaultView());

    // 半透明背景板：720x100 居中，y=180
    const float bgW = 720.f, bgH = 100.f;
    const float bgX = (1280.f - bgW) * 0.5f;
    const float bgY = 180.f;
    sf::RectangleShape bg(sf::Vector2f(bgW, bgH));
    bg.setPosition(bgX, bgY);
    bg.setFillColor(sf::Color(15, 15, 25, static_cast<sf::Uint8>(220 * alpha)));
    bg.setOutlineColor(sf::Color(mainColor.r, mainColor.g, mainColor.b,
                                  static_cast<sf::Uint8>(255 * alpha)));
    bg.setOutlineThickness(2.f);
    window_.draw(bg);

    // 顶部装饰条
    sf::RectangleShape topBar(sf::Vector2f(bgW - 8.f, 4.f));
    topBar.setPosition(bgX + 4.f, bgY + 4.f);
    topBar.setFillColor(sf::Color(mainColor.r, mainColor.g, mainColor.b,
                                   static_cast<sf::Uint8>(255 * alpha)));
    window_.draw(topBar);

    // 标题："本层变异"
    sf::Text title;
    title.setFont(resources_.GetDefaultFont());
    title.setCharacterSize(20);
    title.setFillColor(sf::Color(255, 220, 100, static_cast<sf::Uint8>(255 * alpha)));
    title.setString(U8("本层变异"));
    title.setPosition(bgX + 16.f, bgY + 12.f);
    window_.draw(title);

    // 修饰符名（主色调）
    sf::Text name;
    name.setFont(resources_.GetDefaultFont());
    name.setCharacterSize(26);
    name.setFillColor(sf::Color(mainColor.r, mainColor.g, mainColor.b,
                                 static_cast<sf::Uint8>(255 * alpha)));
    name.setStyle(sf::Text::Bold);
    name.setString(utf8ToSfString(floorModifiers_.GetActiveSummary()));
    // 计算文本宽度以居中
    float nameW = name.getLocalBounds().width;
    name.setPosition(bgX + (bgW - nameW) * 0.5f, bgY + 36.f);
    window_.draw(name);

    // 描述：拼接所有激活修饰符的描述
    std::string desc;
    bool first = true;
    for (auto t : mods) {
        if (t == FloorModifierType::None) continue;
        if (!first) desc += "  |  ";
        desc += GetFloorModifierData(t).description;
        first = false;
    }
    sf::Text descText;
    descText.setFont(resources_.GetDefaultFont());
    descText.setCharacterSize(14);
    descText.setFillColor(sf::Color(220, 220, 220, static_cast<sf::Uint8>(220 * alpha)));
    descText.setString(utf8ToSfString(desc));
    float descW = descText.getLocalBounds().width;
    descText.setPosition(bgX + (bgW - descW) * 0.5f, bgY + 72.f);
    window_.draw(descText);

    window_.setView(prevView);
}

// ============================================================================
// 第十七轮新增：renderFloorModifierHUD
// ----------------------------------------------------------------------------
// HUD 持久指示器：屏幕左上角（FPS 下方）显示当前激活的变异名
// 始终显示（即使 Banner 已消失），便于玩家随时知晓本层规则
// ============================================================================
void Game::renderFloorModifierHUD() {
    if (floorModifiers_.GetActiveCount() == 0) return;

    sf::View prevView = window_.getView();
    window_.setView(window_.getDefaultView());

    // 标签 "变异：" + 修饰符名（主色调拼接）
    auto mods = floorModifiers_.GetActiveModifiers();
    sf::Text label;
    label.setFont(resources_.GetDefaultFont());
    label.setCharacterSize(14);
    label.setFillColor(sf::Color(180, 180, 200));
    label.setString(U8("变异："));
    label.setPosition(8.f, 96.f); // FPS 文本下方（FPS 通常在 y=80）
    window_.draw(label);

    float offsetX = label.getLocalBounds().width + 12.f;
    float posY = 96.f;
    bool first = true;
    for (auto t : mods) {
        if (t == FloorModifierType::None) continue;
        const auto& d = GetFloorModifierData(t);
        sf::Text modName;
        modName.setFont(resources_.GetDefaultFont());
        modName.setCharacterSize(14);
        modName.setFillColor(sf::Color(d.r, d.g, d.b));
        modName.setStyle(sf::Text::Bold);
        modName.setString(utf8ToSfString(d.name));
        if (!first) {
            offsetX += 10.f; // 多个修饰符间距
        }
        modName.setPosition(offsetX, posY);
        window_.draw(modName);
        offsetX += modName.getLocalBounds().width + 6.f;
        first = false;
    }

    window_.setView(prevView);
}

// ============================================================================
// renderEventHint —— 渲染事件房交互提示（房间上方文字）
// ============================================================================
void Game::renderEventHint() {
    if (activeEventRoomIdx_ < 0) return;
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (!pc || !pc->eventPromptActive) return;

    // 在事件房中心上方显示提示
    if (activeEventRoomIdx_ >= static_cast<int>(dungeon_.rooms.size())) return;
    const Room& room = dungeon_.rooms[activeEventRoomIdx_];
    if (room.eventTriggered) return;

    sf::Vector2f roomCenter = dungeon_.TileCenterToWorld(room.center);

    // ---- 渲染事件房 NPC 贴图（世界空间）----
    // 宝箱怪事件不渲染 NPC（假宝箱由 TileType::Chest 渲染）
    const char* spriteKey = nullptr;
    switch (activeEventType_) {
        case EventType::Beggar:     spriteKey = "event_beggar"; break;
        case EventType::Mage:       spriteKey = "event_mage";   break;
        case EventType::Altar:      spriteKey = "event_altar";  break;
        case EventType::ChestMimic: spriteKey = nullptr;        break; // 复用宝箱 tile
        case EventType::Forge:      spriteKey = "event_altar";  break; // 锻造台复用祭坛贴图
        default: return;
    }
    if (spriteKey) {
        sf::IntRect rect = atlas_.GetPixelRect(spriteKey);
        const sf::Texture* tex = atlas_.GetTexture();
        if (rect.width > 0 && rect.height > 0 && tex) {
            sf::Sprite npcSprite;
            npcSprite.setTexture(*tex);
            npcSprite.setTextureRect(rect);
            npcSprite.setOrigin(rect.width * 0.5f, rect.height * 0.5f);
            // NPC 放在房间中心，放大 1.5 倍与玩家一致
            npcSprite.setPosition(roomCenter.x, roomCenter.y);
            npcSprite.setScale(1.5f, 1.5f);
            window_.draw(npcSprite);
        }
    }

    // ---- 渲染交互提示文字（NPC 上方）----
    std::string hint;
    switch (activeEventType_) {
        case EventType::Beggar:     hint = "按 E 与乞丐对话"; break;
        case EventType::Mage:       hint = "按 E 与神秘法师交谈"; break;
        case EventType::ChestMimic: hint = ""; break; // 宝箱怪不显示提示（避免剧透）
        case EventType::Altar:      hint = "按 E 使用祭坛"; break;
        case EventType::Forge:      hint = "按 E 使用锻造台"; break;
        default: return;
    }
    if (hint.empty()) return; // 宝箱怪事件不显示提示

    eventHintText_.setString(utf8ToSfString(hint));
    // 提示文字放在 NPC 上方
    eventHintText_.setPosition(roomCenter.x - 100.f, roomCenter.y - 60.f);
    window_.draw(eventHintText_);
}

// ============================================================================
// renderFissureZones —— 渲染地裂区域视觉（暗红色半透明圆 + 边缘亮色）
// ============================================================================
void Game::renderFissureZones() {
    if (fissureZones_.empty()) return;

    // 切换到世界坐标 View（由 renderPlaying 调用，已是世界 View）
    for (const auto& fz : fissureZones_) {
        // 半透明暗红色圆形（地裂区域）
        sf::CircleShape zone(fz.radius);
        zone.setOrigin(fz.radius, fz.radius);
        zone.setPosition(fz.position);
        // 寿命越短越透明
        float alpha = std::min(1.f, fz.lifetime / 5.f) * 0.5f;
        sf::Uint8 a = static_cast<sf::Uint8>(alpha * 255);
        zone.setFillColor(sf::Color(120, 30, 20, a));
        zone.setOutlineColor(sf::Color(255, 100, 50, a));
        zone.setOutlineThickness(2.f);
        window_.draw(zone);

        // 中心暗棕色圆（裂缝核心）
        sf::CircleShape core(fz.radius * 0.4f);
        core.setOrigin(fz.radius * 0.4f, fz.radius * 0.4f);
        core.setPosition(fz.position);
        core.setFillColor(sf::Color(60, 20, 10, a));
        window_.draw(core);
    }
}

} // namespace cu