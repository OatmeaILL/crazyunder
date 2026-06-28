#include "gameplay/EnemySpawner.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "rendering/TextureAtlas.h"
#include "utils/Logger.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace cu {

// ============================================================================
// 敌人原型配置表（数据驱动）
// ============================================================================
static const EnemyPrototype kPrototypes[] = {
    // Melee: 红色圆形带尖刺，近战追击
    {EnemyType::Melee,   20.f,  80.f,  5.f,  24.f, 1.0f, 400.f,
     sf::Color(200, 50, 50),  1.0f, "enemy_melee"},
    // Ranged: 紫色长方形带法杖，远程射击
    {EnemyType::Ranged,  15.f,  50.f,  3.f, 300.f, 2.0f, 350.f,
     sf::Color(150, 50, 200),  1.0f, "enemy_ranged"},
    // Suicide: 橙色三角形带引信，自爆（高速冲撞，接触爆炸）
    // 速度/伤害/爆炸范围均强化，威胁更高
    {EnemyType::Suicide, 10.f, 350.f, 45.f, 40.f, 1.0f, 400.f,
     sf::Color(230, 130, 30),  1.0f, "enemy_suicide"},
    // Elite: 金色带光环，精英（带词缀），基础 HP 250 保证精英的威胁度
    {EnemyType::Elite,   250.f, 70.f, 15.f, 28.f, 1.2f, 400.f,
     sf::Color(220, 180, 50),  1.3f, "enemy_elite"},
    // Boss: 暗红色大圆形带角，Boss（体型 2x）
    {EnemyType::Boss,    1000.f,40.f, 30.f, 40.f, 1.5f, 500.f,
     sf::Color(120, 20, 20),   2.0f, "enemy_boss"},
    // ---- 新增怪物类型（增加多样性）----
    // StealthMelee: 青色，远距离完全隐身，近距离显形并攻击
    // 基础移速为普通近战（80）的 3 倍
    {EnemyType::StealthMelee, 25.f, 240.f, 8.f, 22.f, 1.2f, 400.f,
     sf::Color(100, 200, 200), 1.0f, "enemy_stealth"},
    // CountdownSuicide: 亮红色，靠近后激活倒计时自爆（伤害更高）
    {EnemyType::CountdownSuicide, 15.f, 200.f, 40.f, 20.f, 1.0f, 400.f,
     sf::Color(255, 80, 80),   1.0f, "enemy_countdown"},
    // Splitter: 绿色，死亡时分裂成 2 个小怪
    {EnemyType::Splitter, 30.f, 70.f, 6.f, 22.f, 1.0f, 400.f,
     sf::Color(80, 200, 80),   1.2f, "enemy_splitter"},
    // Shielded: 蓝灰色，带盾正面减伤 50%
    {EnemyType::Shielded, 40.f, 60.f, 10.f, 24.f, 1.5f, 400.f,
     sf::Color(80, 120, 180),  1.1f, "enemy_shielded"},
    // SniperRanged: 暗青色，超远距离高伤害狙击（靠近时快速撤退）
    {EnemyType::SniperRanged, 18.f, 70.f, 12.f, 500.f, 2.5f, 550.f,
     sf::Color(60, 180, 160),  1.0f, "enemy_sniper"},
    // Caster: 紫红色法袍，中距离引导地面 AoE（延迟爆炸，创造走位压力）
    // 基础 HP=25，移速慢，伤害中等，施法冷却 3.5s，检测范围 400px
    // 行为：保持 200-300px 距离，在玩家脚下召唤 1.5s 预警的 AoE 法阵
    {EnemyType::Caster, 25.f, 55.f, 8.f, 300.f, 3.5f, 400.f,
     sf::Color(180, 60, 160),   1.0f, "enemy_caster"},
};

static constexpr int kPrototypeCount = static_cast<int>(sizeof(kPrototypes) / sizeof(kPrototypes[0]));

EnemySpawner::EnemySpawner() = default;

// ============================================================================
// 分维度层数缩放系数（线性增长，替代旧指数缩放）
// ----------------------------------------------------------------------------
// 设计原则：
//   - HP 线性增长 30%/层：击杀时间从 1s 缓慢增加到 4-5s
//   - 伤害线性增长 18%/层：避免后期一击秒杀（玩家 HP 100）
//   - 速度线性增长 4%/层，封顶 1.6：玩家速度 200，始终保持可躲避
//   - Boss 额外加权：HP ×1.8，伤害 ×1.3（在 SpawnEnemyAt 中应用）
//
// 数值表（Melee 近战，基础 HP=20 伤害=5 速度=80）：
//   层数  HP缩放  伤害缩放  速度缩放   HP   伤害  速度
//   1     1.0     1.0       1.0        20   5     80
//   3     1.6     1.36      1.08       32   6.8   86
//   5     2.2     1.72      1.16       44   8.6   93
//   10    3.7     2.62      1.36       74   13.1  109
//   15    5.2     3.52      1.56       104  17.6  125
//   20    6.7     4.42      1.60       134  22.1  128
// ============================================================================
float EnemySpawner::GetHpScaling() const noexcept {
    // HP 线性增长：1.0 + (level-1) × 0.30
    return 1.0f + static_cast<float>(dungeonLevel_ - 1) * 0.30f;
}

float EnemySpawner::GetDamageScaling() const noexcept {
    // 伤害线性增长：1.0 + (level-1) × 0.18
    return 1.0f + static_cast<float>(dungeonLevel_ - 1) * 0.18f;
}

float EnemySpawner::GetSpeedScaling() const noexcept {
    // 速度线性增长：1.0 + (level-1) × 0.04，封顶 1.6
    // 封顶保证第 16 层后速度不再增加，玩家始终可躲避
    float scale = 1.0f + static_cast<float>(dungeonLevel_ - 1) * 0.04f;
    return (scale > 1.6f) ? 1.6f : scale;
}

void EnemySpawner::Initialize(Registry& registry, const TextureAtlas& atlas,
                              const sf::Vector2f& playerPos) {
    registry_ = &registry;
    atlas_ = &atlas;
    (void)playerPos; // 玩家位置在 Update/StartWave 时使用

    // 预创建敌人实体池
    enemyPool_.clear();
    freeList_.clear();
    enemyPool_.reserve(kPoolCapacity);
    freeList_.reserve(kPoolCapacity);

    for (int i = 0; i < kPoolCapacity; ++i) {
        EntityId id = registry_->CreateEntity();

        // 挂载所有敌人所需组件（初始为非活跃状态）
        auto& transform = registry_->AddComponent<Transform>(id);
        transform.position = sf::Vector2f(0.f, 0.f);
        transform.scale = sf::Vector2f(1.f, 1.f);

        auto& sprite = registry_->AddComponent<Sprite>(id);
        sprite.color = sf::Color::White;
        sprite.origin = sf::Vector2f(16.f, 16.f);

        registry_->AddComponent<Velocity>(id);

        auto& collider = registry_->AddComponent<Collider>(id);
        collider.isCircle = true;
        collider.radius = 16.f;

        auto& health = registry_->AddComponent<Health>(id);
        health.current = 0.f;
        health.max = 20.f;

        auto& enemy = registry_->AddComponent<EnemyComponent>(id);
        enemy.active = false;

        registry_->AddComponent<Tag>(id).flags = TagFlag::Enemy;

        enemyPool_.push_back(id);
        freeList_.push_back(id);
    }

    LOG_INFO("敌人生成器已初始化: 池容量=%d, 可用=%d",
             kPoolCapacity, static_cast<int>(freeList_.size()));
}

EntityId EnemySpawner::acquireFromPool() {
    if (freeList_.empty()) {
        LOG_WARN("敌人对象池耗尽！考虑增大 kPoolCapacity");
        return kInvalidEntity;
    }
    EntityId id = freeList_.back();
    freeList_.pop_back();
    ++aliveCount_;
    return id;
}

void EnemySpawner::releaseToPool(EntityId id) {
    EnemyComponent* enemy = registry_->GetComponent<EnemyComponent>(id);
    if (enemy) {
        enemy->active = false;
    }
    // 重置位置到远处（避免被渲染或碰撞检测到）
    Transform* transform = registry_->GetComponent<Transform>(id);
    if (transform) {
        transform->position = sf::Vector2f(99999.f, 99999.f);
    }
    // 重置速度
    Velocity* velocity = registry_->GetComponent<Velocity>(id);
    if (velocity) {
        velocity->linear = sf::Vector2f(0.f, 0.f);
    }
    freeList_.push_back(id);
    --aliveCount_;
}

const EnemyPrototype& EnemySpawner::getPrototype(EnemyType type) const {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kPrototypeCount) idx = 0;
    return kPrototypes[idx];
}

sf::Vector2f EnemySpawner::randomSpawnPosition(const sf::Vector2f& playerPos) const {
    // 在玩家周围 600-800px 的环形范围内随机生成
    float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 360.f;
    float angleRad = angle * 3.14159265358979f / 180.f;
    float minDist = 600.f;
    float maxDist = 800.f;
    float dist = minDist + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (maxDist - minDist);

    float x = playerPos.x + std::cos(angleRad) * dist;
    float y = playerPos.y + std::sin(angleRad) * dist;
    return sf::Vector2f(x, y);
}

void EnemySpawner::spawnEnemy(EnemyType type, const sf::Vector2f& playerPos) {
    sf::Vector2f spawnPos = randomSpawnPosition(playerPos);
    SpawnEnemyAt(type, spawnPos);
}

EntityId EnemySpawner::SpawnEnemyAt(EnemyType type, sf::Vector2f position, bool champion) {
    EntityId id = acquireFromPool();
    if (id == kInvalidEntity) return kInvalidEntity;

    const EnemyPrototype& proto = getPrototype(type);
    // 分维度独立缩放（替代旧的单系数 GetLevelScaling）
    // HP/伤害/速度各自线性增长，避免后期"既肉又秒杀且不可躲避"
    const float hpScale  = GetHpScaling();
    const float dmgScale = GetDamageScaling();
    const float spdScale = GetSpeedScaling();
    // 第十七轮新增：地牢变异系统 multiplier（默认 1.0=无影响）
    // 与层数缩放、Champion 倍率累乘应用
    const float modHpMul  = modEnemyHpMul_;
    const float modDmgMul = modEnemyDmgMul_;
    const float modSpdMul = modEnemySpdMul_;
    // 攻速 multiplier > 1 表示更快攻击 → 冷却时间除以 multiplier
    const float modAtkSpdMul = (modEnemyAtkSpdMul_ > 0.01f) ? modEnemyAtkSpdMul_ : 1.f;

    // 精英强化版倍率（独立于层数缩放，叠加应用）
    // HP×3 伤害×1.5 速度×1.1 体型×1.5（介于普通1.0和Boss2.0之间）
    // 不对 Boss/Elite 类型应用（这两种本身已是特殊怪）
    const bool canChampion = (type != EnemyType::Boss && type != EnemyType::Elite);
    const bool isChamp = (champion && canChampion);
    const float champHpMul  = isChamp ? 3.0f : 1.0f;
    const float champDmgMul = isChamp ? 1.5f : 1.0f;
    const float champSpdMul = isChamp ? 1.1f : 1.0f;
    const float champScaleMul = isChamp ? 1.5f : 1.0f;

    // 重置 Transform
    Transform* transform = registry_->GetComponent<Transform>(id);
    if (transform) {
        transform->position = position;
        float finalScale = proto.scale * champScaleMul;
        transform->scale = sf::Vector2f(finalScale, finalScale);
    }

    // 重置 Sprite
    Sprite* sprite = registry_->GetComponent<Sprite>(id);
    if (sprite) {
        sprite->sourceRect = atlas_->GetPixelRect(proto.spriteName);
        // 精英怪用金色描边光环（颜色叠加），便于视觉识别
        if (isChamp) {
            sprite->color = sf::Color(255, 230, 150, 255);
        } else {
            sprite->color = sf::Color::White;
        }
        // Boss 体型 2x，origin 需对应调整
        float originBase = (type == EnemyType::Boss) ? 32.f : 16.f;
        sprite->origin = sf::Vector2f(originBase, originBase);
    }

    // 重置 Collider
    Collider* collider = registry_->GetComponent<Collider>(id);
    if (collider) {
        collider->isCircle = true;
        collider->radius = 16.f * proto.scale * champScaleMul;
    }

    // 重置 Health（应用 HP 缩放 + 精英倍率 + 变异 multiplier）
    Health* health = registry_->GetComponent<Health>(id);
    if (health) {
        float scaledHp = proto.hp * hpScale * champHpMul * modHpMul;
        // 防御性下限：变异 multiplier 不应让敌人 HP 降到 0
        if (scaledHp < 1.f) scaledHp = 1.f;
        health->current = scaledHp;
        health->max = scaledHp;
        health->invincibleTimer = 0.f;
    }

    // 重置 EnemyComponent（应用速度/伤害缩放 + 精英倍率 + 变异 multiplier）
    EnemyComponent* enemy = registry_->GetComponent<EnemyComponent>(id);
    if (enemy) {
        enemy->type = type;
        enemy->moveSpeed = proto.moveSpeed * spdScale * champSpdMul * modSpdMul;
        // 攻速 multiplier > 1 = 更快攻击 → 攻击冷却缩短（除以 multiplier）
        enemy->attackCooldown = proto.attackCooldown / modAtkSpdMul;
        enemy->attackRange = proto.attackRange;
        enemy->damage = proto.damage * dmgScale * champDmgMul * modDmgMul;
        enemy->detectionRange = proto.detectionRange;
        enemy->isElite = (type == EnemyType::Elite);
        enemy->isBoss = (type == EnemyType::Boss);
        enemy->active = true;
        enemy->isChampion = isChamp; // 精英强化版标记（用于头上血条渲染和掉落加权）

        // ---- 重置特殊机制字段 ----
        enemy->stealthTimer = 0.f;
        enemy->isStealth = false;
        enemy->selfDestructCountdown = 0.f;
        enemy->countdownActive = false;
        enemy->splitCount = (type == EnemyType::Splitter) ? 2 : 0; // 分裂怪死亡时分裂 2 个小怪
        enemy->hasShield = (type == EnemyType::Shielded);          // 带盾怪
        enemy->shieldAngle = 0.f;
        enemy->specialTimer = 0.f;
        // 重置 Boss 机制字段（Boss 设置初始延迟，避免立即攻击/召唤）
        enemy->rangedAttackTimer = (type == EnemyType::Boss) ? 1.f : 0.f;
        enemy->summonTimer = (type == EnemyType::Boss) ? 3.f : 0.f;
        enemy->isBossMinion = false;
        // ---- 重置施法者专属字段 ----
        enemy->castTimer = (type == EnemyType::Caster) ? 1.5f : 0.f; // 初始延迟，避免立即施法
        enemy->castActive = 0.f;
        enemy->castTargetPos = sf::Vector2f(0.f, 0.f);
        enemy->castWarningRadius = 80.f;
        enemy->castWarningLifetime = 0.f;
    }

    // ---- 第二十一轮新增：激活 EnemyAffix 精英词缀系统 ----
    // 设计意图：激活 Component.h 中自初版定义但从未使用的 EnemyAffix/EliteAffix 整套词缀框架。
    //   - 精英（EnemyType::Elite）和 Champion 升级版在生成时随机获得 2-3 个词缀组合
    //   - 4 种词缀：HpBoost / DamageBoost / SpeedBoost / Regenerating
    //   - 不重复抽样：C(4,2)=6 + C(4,3)=4 + C(4,4)=1 = 11 种组合，让每个精英产生玩法差异
    //   - 词缀倍率在 Champion 倍率和层修饰符之上累乘，与"极限闪避""元素状态"正交
    //   - Boss 不参与（Boss 已有 5 套独立机制）
    //   - Regenerating 词缀的回血逻辑在 EnemyAI.cpp 中处理（每秒回 1% maxHp）
    //   - 词缀光环粒子由 EnemyAI.cpp 中复用 auraTimer 字段实现
    const bool kCanHaveAffix = (type != EnemyType::Boss);
    const bool kShouldHaveAffix = kCanHaveAffix && (enemy->isElite || isChamp);
    if (kShouldHaveAffix) {
        // 随机抽 2-3 个词缀；Champion 5% 概率抽 4 个全开（"满词缀精英"特殊挑战）
        int affixCount = 2 + (std::rand() % 2); // 2 或 3
        if (isChamp && (std::rand() % 100 < 5)) {
            affixCount = 4; // 5% 概率全词缀（仅 Champion）
        }

        // 不重复抽样：4 选 N（Fisher-Yates 洗牌前 N 个）
        uint32_t mask = 0;
        int pool[4] = {0, 1, 2, 3}; // HpBoost / DamageBoost / SpeedBoost / Regenerating
        for (int i = 0; i < affixCount; ++i) {
            int j = i + (std::rand() % (4 - i));
            std::swap(pool[i], pool[j]);
            mask |= (1u << pool[i]);
        }

        // 挂载或重置 EnemyAffix 组件（复用对象池时可能已存在）
        EnemyAffix* affix = registry_->GetComponent<EnemyAffix>(id);
        if (!affix) {
            affix = &registry_->AddComponent<EnemyAffix>(id, EnemyAffix{});
        }
        affix->affixMask = mask;
        affix->regenTimer = 0.f;
        affix->auraTimer = 0.f;

        // 应用词缀倍率（在 Champion 倍率之后累乘）
        // HpBoost: HP ×1.5（与 Champion ×3 累乘 → ×4.5，但 Elite 基础 HP 已高，谨慎）
        // DamageBoost: 伤害 ×1.3
        // SpeedBoost: 速度 ×1.2
        // Regenerating: 不影响生成时属性，由 EnemyAI 处理回血
        if (HasEliteAffix(mask, EliteAffix::HpBoost) && health) {
            health->current *= 1.5f;
            health->max *= 1.5f;
        }
        if (HasEliteAffix(mask, EliteAffix::DamageBoost)) {
            enemy->damage *= 1.3f;
        }
        if (HasEliteAffix(mask, EliteAffix::SpeedBoost)) {
            enemy->moveSpeed *= 1.2f;
        }

        LOG_INFO("敌人 #%llu 获得精英词缀: mask=%u (count=%d, isChamp=%d)",
                 static_cast<unsigned long long>(id), mask, affixCount,
                 isChamp ? 1 : 0);
    } else {
        // 普通敌人：若复用了之前的精英实体（对象池复用），清空词缀避免残留
        EnemyAffix* affix = registry_->GetComponent<EnemyAffix>(id);
        if (affix) {
            affix->affixMask = 0;
            affix->regenTimer = 0.f;
            affix->auraTimer = 0.f;
        }
    }

    // 重置 Velocity
    Velocity* velocity = registry_->GetComponent<Velocity>(id);
    if (velocity) {
        velocity->linear = sf::Vector2f(0.f, 0.f);
    }

    // ---- 隐身怪初始完全隐身（进入显形范围后才会显示）----
    if (type == EnemyType::StealthMelee) {
        Sprite* sp = registry_->GetComponent<Sprite>(id);
        if (sp) {
            sp->color = sf::Color(100, 200, 200, 0); // 完全隐身
        }
        EnemyComponent* ec = registry_->GetComponent<EnemyComponent>(id);
        if (ec) {
            ec->isStealth = true;
        }
    }

    // ---- 施法者初始暗紫红色渲染 ----
    if (type == EnemyType::Caster) {
        Sprite* sp = registry_->GetComponent<Sprite>(id);
        if (sp) {
            sp->color = sf::Color(180, 60, 160, 255); // 暗紫红色
        }
    }

    return id;
}

void EnemySpawner::SpawnEnemiesInArea(EnemyType type, sf::Vector2f center,
                                       int count, float radius, float championChance) {
    int championCount = 0;
    for (int i = 0; i < count; ++i) {
        // 在中心周围 radius 范围内随机位置
        float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
        float dist = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * radius;
        sf::Vector2f pos(
            center.x + std::cos(angle) * dist,
            center.y + std::sin(angle) * dist
        );
        // 按概率升级为精英强化版（Boss/Elite 类型不受影响，SpawnEnemyAt 内部会再判断）
        bool champ = (championChance > 0.f) &&
                     (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) < championChance);
        if (champ) ++championCount;
        SpawnEnemyAt(type, pos, champ);
    }
    LOG_INFO("在区域 (%.1f, %.1f) 生成 %d 个 %s 敌人 (其中 %d 个精英强化版)",
             center.x, center.y, count, EnemyTypeName(type), championCount);
}

WaveConfig EnemySpawner::generateWaveConfig(int waveNumber) const {
    WaveConfig config;
    // 波次递增公式：
    //   基础敌人 = 30 + wave * 20
    //   每 3 波出现精英，每 5 波出现 Boss
    //   远程与自爆从第 2 波开始出现
    //   新怪物从第 2 波开始逐步加入（增加多样性）
    int base = 30 + waveNumber * 20;

    config.meleeCount = base;
    config.rangedCount = (waveNumber >= 2) ? 5 + waveNumber * 3 : 0;
    config.suicideCount = (waveNumber >= 2) ? 3 + waveNumber * 2 : 0;
    config.eliteCount = (waveNumber >= 3 && waveNumber % 3 == 0) ? 2 + waveNumber / 3 : 0;
    config.bossCount = (waveNumber >= 5 && waveNumber % 5 == 0) ? 1 : 0;

    // ---- 新怪物数量（从第 2 波开始逐步加入）----
    // 隐身怪：第 2 波开始，数量随波次缓慢增长
    config.stealthCount = (waveNumber >= 2) ? 2 + waveNumber / 2 : 0;
    // 倒计时自爆：第 3 波开始（比普通自爆稍晚，伤害高）
    config.countdownSuicideCount = (waveNumber >= 3) ? 1 + waveNumber / 3 : 0;
    // 分裂怪：第 2 波开始
    config.splitterCount = (waveNumber >= 2) ? 2 + waveNumber / 2 : 0;
    // 带盾怪：第 4 波开始（较耐打，后期出现）
    config.shieldedCount = (waveNumber >= 4) ? 1 + waveNumber / 4 : 0;
    // 狙击远程：第 3 波开始（高伤害远程威胁，数量少）
    config.sniperCount = (waveNumber >= 3) ? 1 + waveNumber / 4 : 0;
    // 施法者：第 2 波开始（AoE 范围伤害，创造走位压力）
    config.casterCount = (waveNumber >= 2) ? 1 + waveNumber / 3 : 0;

    config.totalEnemies = config.meleeCount + config.rangedCount +
                          config.suicideCount + config.eliteCount + config.bossCount +
                          config.stealthCount + config.countdownSuicideCount +
                          config.splitterCount + config.shieldedCount +
                          config.sniperCount + config.casterCount;
    // 生成间隔随波次缩短（但最小 0.02s）
    config.spawnInterval = std::max(0.02f, 0.15f - waveNumber * 0.01f);

    return config;
}

void EnemySpawner::StartWave(int waveNumber) {
    currentWave_ = waveNumber;
    WaveConfig config = generateWaveConfig(waveNumber);
    // 第十七轮新增：应用变异系统 spawnIntervalMul（<1=更频繁刷怪，>1=更慢）
    // 防御性下限 0.02s，避免刷怪过快导致对象池耗尽
    spawnInterval_ = std::max(0.02f, config.spawnInterval * modSpawnIntervalMul_);
    spawnTimer_ = 0.f;

    // 构建生成队列（按类型依次入队）
    spawnQueue_.clear();
    spawnQueue_.reserve(config.totalEnemies);

    for (int i = 0; i < config.meleeCount; ++i)              spawnQueue_.push_back(EnemyType::Melee);
    for (int i = 0; i < config.rangedCount; ++i)             spawnQueue_.push_back(EnemyType::Ranged);
    for (int i = 0; i < config.suicideCount; ++i)            spawnQueue_.push_back(EnemyType::Suicide);
    for (int i = 0; i < config.eliteCount; ++i)              spawnQueue_.push_back(EnemyType::Elite);
    for (int i = 0; i < config.bossCount; ++i)              spawnQueue_.push_back(EnemyType::Boss);
    for (int i = 0; i < config.stealthCount; ++i)           spawnQueue_.push_back(EnemyType::StealthMelee);
    for (int i = 0; i < config.countdownSuicideCount; ++i)  spawnQueue_.push_back(EnemyType::CountdownSuicide);
    for (int i = 0; i < config.splitterCount; ++i)          spawnQueue_.push_back(EnemyType::Splitter);
    for (int i = 0; i < config.shieldedCount; ++i)          spawnQueue_.push_back(EnemyType::Shielded);
    for (int i = 0; i < config.sniperCount; ++i)            spawnQueue_.push_back(EnemyType::SniperRanged);
    for (int i = 0; i < config.casterCount; ++i)           spawnQueue_.push_back(EnemyType::Caster);

    totalToSpawn_ = config.totalEnemies;
    spawnedCount_ = 0;

    LOG_INFO("波次 %d 开始: 总数=%d (近战=%d, 远程=%d, 自爆=%d, 精英=%d, Boss=%d, "
             "隐身=%d, 倒计时=%d, 分裂=%d, 带盾=%d, 狙击=%d, 施法者=%d), 间隔=%.3fs",
             waveNumber, config.totalEnemies, config.meleeCount, config.rangedCount,
             config.suicideCount, config.eliteCount, config.bossCount,
             config.stealthCount, config.countdownSuicideCount,
             config.splitterCount, config.shieldedCount, config.sniperCount,
             config.casterCount,
             config.spawnInterval);
}

void EnemySpawner::Update(float dt, const sf::Vector2f& playerPos) {
    // 1. 处理生成队列
    if (!spawnQueue_.empty()) {
        spawnTimer_ -= dt;
        while (spawnTimer_ <= 0.f && !spawnQueue_.empty()) {
            EnemyType type = spawnQueue_.back();
            spawnQueue_.pop_back();
            spawnEnemy(type, playerPos);
            ++spawnedCount_;
            spawnTimer_ += spawnInterval_;
        }
    }

    // 2. 回收死亡敌人
    recycleDeadEnemies();
}

void EnemySpawner::recycleDeadEnemies() {
    // 回收 active=false 的敌人（由 Game.cpp 死亡检测设置）
    // 不使用 IsAlive 检查，因为实体未被销毁，仅标记为非活跃
    for (EntityId id : enemyPool_) {
        // 先检查是否已在 freeList 中
        bool alreadyFree = false;
        for (EntityId freeId : freeList_) {
            if (freeId == id) {
                alreadyFree = true;
                break;
            }
        }
        if (alreadyFree) continue;

        // 检查敌人是否非活跃（已死亡）
        EnemyComponent* enemy = registry_->GetComponent<EnemyComponent>(id);
        if (enemy && !enemy->active) {
            freeList_.push_back(id);
            --aliveCount_;
        }
    }
}

bool EnemySpawner::IsWaveComplete() const noexcept {
    // 波次完成 = 队列已清空 且 无活跃敌人
    return spawnQueue_.empty() && aliveCount_ == 0;
}

void EnemySpawner::ClearAllEnemies() {
    for (EntityId id : enemyPool_) {
        EnemyComponent* enemy = registry_->GetComponent<EnemyComponent>(id);
        if (enemy && enemy->active) {
            releaseToPool(id);
        }
    }
    spawnQueue_.clear();
    LOG_INFO("所有敌人已清除，回收 %d 个到对象池", static_cast<int>(freeList_.size()));
}

} // namespace cu
