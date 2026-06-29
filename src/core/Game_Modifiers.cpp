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
// 第十七轮新增：applyFloorModifiersToSubsystems
// ----------------------------------------------------------------------------
// 将当前 floorModifiers_ 的乘法系数推送到 EnemySpawner/LootSystem/MerchantSystem
// 在 setupPlayingScene 与 applySaveData 中调用，确保所有子系统与本层 modifier 同步
// 默认 modifier 均为 1.0（无影响），仅在 RollForLevel / Deserialize 后才有非 1 值
// ============================================================================
void Game::applyFloorModifiersToSubsystems() {
    enemySpawner_.SetModifierEnemyHpMul(floorModifiers_.GetEnemyHpMul());
    enemySpawner_.SetModifierEnemyDamageMul(floorModifiers_.GetEnemyDamageMul());
    enemySpawner_.SetModifierEnemyMoveSpeedMul(floorModifiers_.GetEnemyMoveSpeedMul());
    enemySpawner_.SetModifierEnemyAttackSpeedMul(floorModifiers_.GetEnemyAttackSpeedMul());
    enemySpawner_.SetModifierSpawnIntervalMul(floorModifiers_.GetSpawnIntervalMul());

    lootSystem_.SetModifierItemDropChanceMul(floorModifiers_.GetItemDropChanceMul());

    MerchantSystem::SetModifierPriceMul(floorModifiers_.GetMerchantPriceMul());
}

// ============================================================================
// applyCurse —— 施加诅咒效果（诅咒房进入时）
// ============================================================================
void Game::applyCurse(int roomIndex) {
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (!pc) return;
    pc->cursed = true;
    pc->cursedRoomIndex = roomIndex;
    recomputePlayerStats();
    // 视觉提示
    Transform* pT = registry_.GetComponent<Transform>(playerId_);
    if (pT) {
        SpawnFloatText(registry_, pT->position, "被诅咒！移速与攻速降低",
                       sf::Color(180, 80, 220), 22, 2.0f);
    }
    LOG_INFO("玩家被诅咒（房间 %d）", roomIndex);
}

// ============================================================================
// removeCurse —— 解除诅咒效果（诅咒房清理时）
// ============================================================================
void Game::removeCurse() {
    PlayerComponent* pc = registry_.GetComponent<PlayerComponent>(playerId_);
    if (!pc || !pc->cursed) return;
    pc->cursed = false;
    pc->cursedRoomIndex = -1;
    recomputePlayerStats();
    LOG_INFO("玩家诅咒已解除");
}

} // namespace cu