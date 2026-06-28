#include "gameplay/SkillSystem.h"
#include "gameplay/Player.h"
#include "gameplay/EnemyAI.h"
#include "gameplay/CombatSystem.h"
#include "gameplay/CombatEffects.h"
#include "gameplay/DungeonGenerator.h"
#include "core/AudioManager.h"
#include "rendering/Camera.h"
#include "rendering/ParticleSystem.h"
#include "ecs/Registry.h"
#include "ecs/Component.h"
#include "utils/UniformGrid.h"
#include "utils/Logger.h"
#include <cmath>
#include <cstdlib>

namespace cu {

// ============================================================================
// 技能静态数据表
// ============================================================================
static const SkillData kSkillDataTable[] = {
    // type,            name,        desc,                                cooldown, duration, manaCost
    { SkillType::GroundSlam,  "震地波",   "范围伤害+击退+破甲",               7.f,  0.f,  20.f  },
    { SkillType::LeechStrike, "吸血打击", "持续5s内攻击吸血30%",              11.f, 5.f,  15.f  },
    { SkillType::Berserk,     "狂暴",     "+50%攻击+30%移速 受伤+20% 持续6s", 20.f, 6.f,  25.f  },
    { SkillType::GravityWell, "引力井",   "拉扯周围敌人4秒",                  15.f, 4.f,  18.f  },
    { SkillType::SpikeGround, "地刺",     "地面伤害区域5秒 减速+持续伤害",     12.f, 5.f,  20.f  },
};

const SkillData& GetSkillData(SkillType type) {
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(SkillType::Count)) {
        return kSkillDataTable[idx];
    }
    static SkillData empty{SkillType::Count, "", "", 0.f, 0.f, 0.f};
    return empty;
}

const char* GetSkillName(SkillType type) {
    return GetSkillData(type).name;
}

// ============================================================================
// ExecuteSkill —— 释放技能
// ============================================================================
bool ExecuteSkill(Registry& registry, EntityId player,
                  SkillType skill, ParticleSystem& particles,
                  Camera& camera, UniformGrid& grid,
                  CombatSystem& combat, const Dungeon* dungeon) {
    if (skill == SkillType::Count) return false;

    Transform* transform = registry.GetComponent<Transform>(player);
    PlayerComponent* pc = registry.GetComponent<PlayerComponent>(player);
    if (!transform || !pc) return false;

    const SkillData& data = GetSkillData(skill);
    const int skillLevel = GetSkillLevel(*pc, skill); // 技能等级 1-3（影响效果强度）

    // ---- 法力值检查：法力不足时技能释放失败 ----
    if (data.manaCost > 0.f && pc->stats.currentMp < data.manaCost) {
        return false;
    }
    // 扣减法力值
    if (data.manaCost > 0.f) {
        pc->stats.currentMp -= data.manaCost;
        if (pc->stats.currentMp < 0.f) pc->stats.currentMp = 0.f;
    }

    switch (skill) {
    // ---- 震地波：范围伤害+击退+破甲（华丽化：5 层粒子叠加）----
    case SkillType::GroundSlam: {
        // 等级缩放：半径 150+25*(lv-1)，伤害倍率 2.0+0.5*(lv-1)，击退力 400+50*(lv-1)
        const float kRadius = 150.f + 25.f * (skillLevel - 1);
        const float kDamageMul = 2.f + 0.5f * (skillLevel - 1);
        const float kKnockbackForce = 400.f + 50.f * (skillLevel - 1);

        // 层1：中央白色闪光（极快、短命、高亮，制造"砸地瞬间"的爆白感）
        EmitConfig flash;
        flash.radial = true;
        flash.speedMin = 400.f;
        flash.speedMax = 600.f;
        flash.colorMin = sf::Color(255, 250, 220, 255);
        flash.colorMax = sf::Color(255, 255, 255, 255);
        flash.sizeMin = 6.f;
        flash.sizeMax = 12.f;
        flash.lifeMin = 0.12f;
        flash.lifeMax = 0.2f;
        particles.Emit(transform->position, 18, flash);

        // 层2：金色能量环（明亮、中速、向外扩散，赋予技能"能量感"）
        EmitConfig energyRing;
        energyRing.radial = true;
        energyRing.speedMin = 280.f;
        energyRing.speedMax = 420.f;
        energyRing.colorMin = sf::Color(255, 180, 60, 255);
        energyRing.colorMax = sf::Color(255, 230, 120, 255);
        energyRing.sizeMin = 4.f;
        energyRing.sizeMax = 7.f;
        energyRing.lifeMin = 0.3f;
        energyRing.lifeMax = 0.5f;
        particles.Emit(transform->position, 26, energyRing);

        // 层3：暗棕色冲击波环（水平向外扩散的岩石碎片，保留原特色）
        EmitConfig shockwave;
        shockwave.radial = true;
        shockwave.speedMin = 300.f;
        shockwave.speedMax = 450.f;
        shockwave.colorMin = sf::Color(80, 60, 40);
        shockwave.colorMax = sf::Color(140, 100, 60);
        shockwave.sizeMin = 5.f;
        shockwave.sizeMax = 9.f;
        shockwave.lifeMin = 0.3f;
        shockwave.lifeMax = 0.5f;
        particles.Emit(transform->position, 30, shockwave);

        // 层4：向上飞溅的岩石碎块（更高更密，模拟地面被砸碎溅起）
        EmitConfig rocks;
        rocks.radial = false;
        rocks.velocityMin = sf::Vector2f(-150.f, -240.f);
        rocks.velocityMax = sf::Vector2f(150.f, -120.f);
        rocks.colorMin = sf::Color(60, 45, 30);
        rocks.colorMax = sf::Color(110, 80, 55);
        rocks.sizeMin = 3.f;
        rocks.sizeMax = 7.f;
        rocks.lifeMin = 0.5f;
        rocks.lifeMax = 0.85f;
        particles.Emit(transform->position, 24, rocks);

        // 层5：地面尘土云（慢速、贴近地面、更大更持久）
        EmitConfig dust;
        dust.radial = true;
        dust.speedMin = 20.f;
        dust.speedMax = 70.f;
        dust.colorMin = sf::Color(100, 90, 80, 180);
        dust.colorMax = sf::Color(170, 160, 140, 210);
        dust.sizeMin = 10.f;
        dust.sizeMax = 18.f;
        dust.lifeMin = 0.7f;
        dust.lifeMax = 1.2f;
        particles.Emit(transform->position, 16, dust);

        AudioManager::Instance().PlaySFX(AudioManager::kSFXExplosion);
        camera.Shake(16.f, 0.5f); // 震感增强

        std::vector<EntityId> targets;
        targets.reserve(64);
        grid.QueryRange(transform->position, kRadius, targets);

        float dmg = pc->stats.damage * kDamageMul;
        int hitCount = 0;
        for (EntityId tid : targets) {
            if (tid == player) continue;
            EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(tid);
            if (!enemy || !enemy->active) continue;
            Transform* et = registry.GetComponent<Transform>(tid);
            if (!et) continue;

            sf::Vector2f kbDir = et->position - transform->position;
            float kbLen = std::sqrt(kbDir.x * kbDir.x + kbDir.y * kbDir.y);
            if (kbLen > 0.001f) kbDir /= kbLen;

            DamageInfo dmgInfo;
            dmgInfo.attacker = player;
            dmgInfo.target = tid;
            dmgInfo.amount = dmg;
            dmgInfo.isCritical = false;
            dmgInfo.element = ElementType::Physical;
            dmgInfo.knockback = kbDir * kKnockbackForce;
            dmgInfo.lifesteal = 0.f;
            combat.ApplyDamage(registry, dmgInfo);
            ++hitCount;
        }
        LOG_INFO("震地波释放: 伤害=%.1f, 命中=%d", dmg, hitCount);
        break;
    }

    // ---- 吸血打击：激活持续吸血状态（华丽化：4 层粒子 + 血红雾气）----
    case SkillType::LeechStrike: {
        // 等级缩放：持续时间 5+1*(lv-1)，吸血比例在 PlayerCombat 中按等级计算
        pc->leechStrikeActive = data.duration + 1.f * (skillLevel - 1);
        pc->leechStrikeLevel = skillLevel; // 记录等级供 PlayerCombat 读取

        // 层1：血红色爆发环（快速向外扩散，象征鲜血释放）
        EmitConfig bloodBurst;
        bloodBurst.radial = true;
        bloodBurst.speedMin = 180.f;
        bloodBurst.speedMax = 280.f;
        bloodBurst.colorMin = sf::Color(180, 20, 30, 255);
        bloodBurst.colorMax = sf::Color(255, 60, 80, 255);
        bloodBurst.sizeMin = 4.f;
        bloodBurst.sizeMax = 7.f;
        bloodBurst.lifeMin = 0.3f;
        bloodBurst.lifeMax = 0.5f;
        particles.Emit(transform->position, 20, bloodBurst);

        // 层2：深红色上升血雾（缓慢上升、长生命，营造"血气弥漫"氛围）
        EmitConfig bloodMist;
        bloodMist.radial = false;
        bloodMist.velocityMin = sf::Vector2f(-40.f, -80.f);
        bloodMist.velocityMax = sf::Vector2f(40.f, -30.f);
        bloodMist.colorMin = sf::Color(120, 10, 20, 200);
        bloodMist.colorMax = sf::Color(200, 40, 60, 220);
        bloodMist.sizeMin = 8.f;
        bloodMist.sizeMax = 14.f;
        bloodMist.lifeMin = 0.8f;
        bloodMist.lifeMax = 1.3f;
        particles.Emit(transform->position, 16, bloodMist);

        // 层3：绿色收敛光环（吸血能量的标志色，慢速向内收缩）
        EmitConfig cfg;
        cfg.radial = true;
        cfg.speedMin = 15.f;
        cfg.speedMax = 35.f;
        cfg.colorMin = sf::Color(50, 220, 80);
        cfg.colorMax = sf::Color(140, 255, 140);
        cfg.sizeMin = 5.f;
        cfg.sizeMax = 8.f;
        cfg.lifeMin = 0.7f;
        cfg.lifeMax = 1.0f;
        particles.Emit(transform->position, 22, cfg);

        // 层4：翠绿色十字光束（更高更密，象征生命汲取向上汇聚）
        EmitConfig beam;
        beam.radial = false;
        beam.velocityMin = sf::Vector2f(-25.f, -160.f);
        beam.velocityMax = sf::Vector2f(25.f, -100.f);
        beam.colorMin = sf::Color(100, 255, 100);
        beam.colorMax = sf::Color(180, 255, 200);
        beam.sizeMin = 3.f;
        beam.sizeMax = 6.f;
        beam.lifeMin = 0.5f;
        beam.lifeMax = 0.75f;
        particles.Emit(transform->position, 14, beam);

        AudioManager::Instance().PlaySFX(AudioManager::kSFXHit);
        camera.Shake(6.f, 0.3f);
        LOG_INFO("吸血打击激活: 持续%.1fs", data.duration);
        break;
    }

    // ---- 狂暴：buff 持续6秒（华丽化：4 层粒子 + 金色火花）----
    case SkillType::Berserk: {
        // 等级缩放：持续时间 6+1*(lv-1)
        pc->berserkTimer = data.duration + 1.f * (skillLevel - 1);

        // 层1：暗红色冲击波（快速向外扩散，象征怒气爆发）
        EmitConfig shockDark;
        shockDark.radial = true;
        shockDark.speedMin = 200.f;
        shockDark.speedMax = 320.f;
        shockDark.colorMin = sf::Color(150, 20, 10, 255);
        shockDark.colorMax = sf::Color(200, 50, 20, 255);
        shockDark.sizeMin = 5.f;
        shockDark.sizeMax = 8.f;
        shockDark.lifeMin = 0.25f;
        shockDark.lifeMax = 0.45f;
        particles.Emit(transform->position, 18, shockDark);

        // 层2：橙红色冲击波（中速、更大粒子，与暗红层形成层次）
        EmitConfig shockBright;
        shockBright.radial = true;
        shockBright.speedMin = 100.f;
        shockBright.speedMax = 200.f;
        shockBright.colorMin = sf::Color(255, 100, 30, 255);
        shockBright.colorMax = sf::Color(255, 180, 60, 255);
        shockBright.sizeMin = 6.f;
        shockBright.sizeMax = 10.f;
        shockBright.lifeMin = 0.4f;
        shockBright.lifeMax = 0.7f;
        particles.Emit(transform->position, 14, shockBright);

        // 层3：烈焰粒子（向上飘升、高密，主视觉）
        EmitConfig cfg;
        cfg.radial = false;
        cfg.velocityMin = sf::Vector2f(-80.f, -220.f);
        cfg.velocityMax = sf::Vector2f(80.f, -120.f);
        cfg.colorMin = sf::Color(255, 50, 20);
        cfg.colorMax = sf::Color(255, 140, 50);
        cfg.sizeMin = 5.f;
        cfg.sizeMax = 9.f;
        cfg.lifeMin = 0.7f;
        cfg.lifeMax = 1.1f;
        particles.Emit(transform->position, 30, cfg);

        // 层4：金色火花（向上飞溅、短促明亮，象征怒火迸射）
        EmitConfig sparks;
        sparks.radial = false;
        sparks.velocityMin = sf::Vector2f(-100.f, -300.f);
        sparks.velocityMax = sf::Vector2f(100.f, -180.f);
        sparks.colorMin = sf::Color(255, 220, 100, 255);
        sparks.colorMax = sf::Color(255, 255, 180, 255);
        sparks.sizeMin = 2.f;
        sparks.sizeMax = 4.f;
        sparks.lifeMin = 0.3f;
        sparks.lifeMax = 0.5f;
        particles.Emit(transform->position, 22, sparks);

        AudioManager::Instance().PlaySFX(AudioManager::kSFXExplosion);
        camera.Shake(10.f, 0.4f);
        LOG_INFO("狂暴激活: 持续%.1fs", data.duration);
        break;
    }

    // ---- 引力井：在玩家位置创建拉扯区域4秒（华丽化：4 层紫色粒子）----
    case SkillType::GravityWell: {
        // 等级缩放：持续时间 4+1*(lv-1)
        pc->gravityWellTimer = data.duration + 1.f * (skillLevel - 1);
        pc->gravityWellPos = transform->position;

        // 层1：白色核心闪光（极快、短命，象征黑洞诞生瞬间）
        EmitConfig coreFlash;
        coreFlash.radial = true;
        coreFlash.speedMin = 300.f;
        coreFlash.speedMax = 450.f;
        coreFlash.colorMin = sf::Color(240, 220, 255, 255);
        coreFlash.colorMax = sf::Color(255, 255, 255, 255);
        coreFlash.sizeMin = 4.f;
        coreFlash.sizeMax = 8.f;
        coreFlash.lifeMin = 0.15f;
        coreFlash.lifeMax = 0.25f;
        particles.Emit(transform->position, 18, coreFlash);

        // 层2：亮紫色核心爆发（快速向外，标志引力井中心能量）
        EmitConfig core;
        core.radial = true;
        core.speedMin = 200.f;
        core.speedMax = 320.f;
        core.colorMin = sf::Color(200, 100, 255);
        core.colorMax = sf::Color(240, 180, 255);
        core.sizeMin = 3.f;
        core.sizeMax = 6.f;
        core.lifeMin = 0.25f;
        core.lifeMax = 0.4f;
        particles.Emit(transform->position, 22, core);

        // 层3：深紫色扩散环（中速、大粒子、长生命，营造"虚空领域"）
        EmitConfig voidRing;
        voidRing.radial = true;
        voidRing.speedMin = 80.f;
        voidRing.speedMax = 160.f;
        voidRing.colorMin = sf::Color(80, 20, 140, 230);
        voidRing.colorMax = sf::Color(140, 50, 200, 240);
        voidRing.sizeMin = 6.f;
        voidRing.sizeMax = 10.f;
        voidRing.lifeMin = 0.9f;
        voidRing.lifeMax = 1.6f;
        particles.Emit(transform->position, 28, voidRing);

        // 层4：紫色螺旋粒子（慢速、长生命、大范围，持续可见的引力场）
        EmitConfig cfg;
        cfg.radial = true;
        cfg.speedMin = 40.f;
        cfg.speedMax = 120.f;
        cfg.colorMin = sf::Color(120, 30, 200);
        cfg.colorMax = sf::Color(180, 80, 255);
        cfg.sizeMin = 3.f;
        cfg.sizeMax = 5.f;
        cfg.lifeMin = 0.8f;
        cfg.lifeMax = 1.5f;
        particles.Emit(transform->position, 32, cfg);

        AudioManager::Instance().PlaySFX(AudioManager::kSFXExplosion);
        camera.Shake(8.f, 0.35f);
        LOG_INFO("引力井释放: 位置(%.1f,%.1f), 持续%.1fs",
                 transform->position.x, transform->position.y, data.duration);
        break;
    }

    // ---- 地刺：在玩家位置创建伤害区域5秒（华丽化：4 层粒子）----
    case SkillType::SpikeGround: {
        // 等级缩放：持续时间 5+1*(lv-1)
        pc->spikeGroundTimer = data.duration + 1.f * (skillLevel - 1);
        pc->spikeGroundPos = transform->position;

        // 层1：土黄色尖刺粒子（向上喷射、更高更密，主视觉）
        EmitConfig cfg;
        cfg.radial = false;
        cfg.velocityMin = sf::Vector2f(-70.f, -260.f);
        cfg.velocityMax = sf::Vector2f(70.f, -160.f);
        cfg.colorMin = sf::Color(140, 90, 30);
        cfg.colorMax = sf::Color(210, 160, 70);
        cfg.sizeMin = 3.f;
        cfg.sizeMax = 7.f;
        cfg.lifeMin = 0.4f;
        cfg.lifeMax = 0.65f;
        particles.Emit(transform->position, 28, cfg);

        // 层2：血红色尖刺尖端（向上喷射，象征致命穿刺）
        EmitConfig bloodTip;
        bloodTip.radial = false;
        bloodTip.velocityMin = sf::Vector2f(-50.f, -300.f);
        bloodTip.velocityMax = sf::Vector2f(50.f, -200.f);
        bloodTip.colorMin = sf::Color(180, 30, 20, 255);
        bloodTip.colorMax = sf::Color(255, 80, 60, 255);
        bloodTip.sizeMin = 2.f;
        bloodTip.sizeMax = 4.f;
        bloodTip.lifeMin = 0.3f;
        bloodTip.lifeMax = 0.5f;
        particles.Emit(transform->position, 18, bloodTip);

        // 层3：地面裂纹效果（慢速、暗色、更大范围，象征地面破裂）
        EmitConfig crack;
        crack.radial = true;
        crack.speedMin = 30.f;
        crack.speedMax = 90.f;
        crack.colorMin = sf::Color(60, 40, 15);
        crack.colorMax = sf::Color(130, 90, 45);
        crack.sizeMin = 3.f;
        crack.sizeMax = 6.f;
        crack.lifeMin = 0.6f;
        crack.lifeMax = 1.0f;
        particles.Emit(transform->position, 20, crack);

        // 层4：棕色尘土云（慢速向上扩散，营造"地刺破土"扬尘感）
        EmitConfig dirtCloud;
        dirtCloud.radial = true;
        dirtCloud.speedMin = 25.f;
        dirtCloud.speedMax = 80.f;
        dirtCloud.colorMin = sf::Color(120, 90, 50, 180);
        dirtCloud.colorMax = sf::Color(180, 140, 90, 210);
        dirtCloud.sizeMin = 8.f;
        dirtCloud.sizeMax = 14.f;
        dirtCloud.lifeMin = 0.7f;
        dirtCloud.lifeMax = 1.2f;
        particles.Emit(transform->position, 14, dirtCloud);

        AudioManager::Instance().PlaySFX(AudioManager::kSFXHit);
        camera.Shake(7.f, 0.35f);
        LOG_INFO("地刺释放: 位置(%.1f,%.1f), 持续%.1fs",
                 transform->position.x, transform->position.y, data.duration);
        break;
    }

    default:
        return false;
    }

    return true;
}

// ============================================================================
// UpdateSkillBuffs —— 更新持续技能效果
// ============================================================================
void UpdateSkillBuffs(Registry& registry, EntityId player,
                      UniformGrid& grid, CombatSystem& combat,
                      ParticleSystem& particles, float dt) {
    PlayerComponent* pc = registry.GetComponent<PlayerComponent>(player);
    Transform* transform = registry.GetComponent<Transform>(player);
    if (!pc || !transform) return;

    // ---- 吸血打击持续计时衰减 + 持续血气光环 ----
    if (pc->leechStrikeActive > 0.f) {
        pc->leechStrikeActive -= dt;
        // 持续散发深红色血气粒子，标志吸血状态激活
        pc->leechParticleTimer -= dt;
        if (pc->leechParticleTimer <= 0.f) {
            pc->leechParticleTimer = 0.12f;
            EmitConfig leechAura;
            leechAura.radial = true;
            leechAura.speedMin = 10.f;
            leechAura.speedMax = 30.f;
            leechAura.colorMin = sf::Color(150, 30, 40, 200);
            leechAura.colorMax = sf::Color(220, 60, 80, 230);
            leechAura.sizeMin = 3.f;
            leechAura.sizeMax = 5.f;
            leechAura.lifeMin = 0.4f;
            leechAura.lifeMax = 0.7f;
            particles.Emit(transform->position, 4, leechAura);
        }
        if (pc->leechStrikeActive <= 0.f) {
            pc->leechStrikeActive = 0.f;
            // 技能结束重置粒子计时器，避免下次释放首次发射时机异常
            pc->leechParticleTimer = 0.f;
            LOG_INFO("吸血打击结束");
        }
    }

    // ---- 狂暴 buff 计时 + 持续烈焰粒子 ----
    if (pc->berserkTimer > 0.f) {
        pc->berserkTimer -= dt;
        // 狂暴期间持续散发红橙火焰粒子（更密更鲜艳）
        pc->berserkParticleTimer -= dt;
        if (pc->berserkParticleTimer <= 0.f) {
            pc->berserkParticleTimer = 0.07f; // 频率提高：0.1s → 0.07s
            EmitConfig cfg;
            cfg.radial = false;
            cfg.velocityMin = sf::Vector2f(-50.f, -130.f);
            cfg.velocityMax = sf::Vector2f(50.f, -80.f);
            cfg.colorMin = sf::Color(255, 60, 20);
            cfg.colorMax = sf::Color(255, 150, 50);
            cfg.sizeMin = 3.f;
            cfg.sizeMax = 6.f;
            cfg.lifeMin = 0.35f;
            cfg.lifeMax = 0.6f;
            particles.Emit(transform->position, 5, cfg);

            // 间歇性金色火花（每 3 次火焰发射一次，制造"燃烧迸射"感）
            if (++pc->berserkSparkCounter >= 3) {
                pc->berserkSparkCounter = 0;
                EmitConfig spark;
                spark.radial = false;
                spark.velocityMin = sf::Vector2f(-70.f, -220.f);
                spark.velocityMax = sf::Vector2f(70.f, -150.f);
                spark.colorMin = sf::Color(255, 200, 80, 255);
                spark.colorMax = sf::Color(255, 240, 150, 255);
                spark.sizeMin = 2.f;
                spark.sizeMax = 3.f;
                spark.lifeMin = 0.2f;
                spark.lifeMax = 0.4f;
                particles.Emit(transform->position, 3, spark);
            }
        }
        if (pc->berserkTimer <= 0.f) {
            pc->berserkTimer = 0.f;
            // 技能结束重置粒子计时器/计数器
            pc->berserkParticleTimer = 0.f;
            pc->berserkSparkCounter = 0;
            LOG_INFO("狂暴结束");
        }
    }

    // ---- 引力井：拉扯周围敌人 + 持续漩涡粒子 ----
    if (pc->gravityWellTimer > 0.f) {
        pc->gravityWellTimer -= dt;
        if (pc->gravityWellTimer <= 0.f) {
            pc->gravityWellTimer = 0.f;
        } else {
            // 等级缩放：拉扯范围 300+30*(lv-1)，拉扯力 300+50*(lv-1)
            const int gwLevel = GetSkillLevel(*pc, SkillType::GravityWell);
            const float kPullRadius = 300.f + 30.f * (gwLevel - 1);
            const float kPullForce = 300.f + 50.f * (gwLevel - 1);

            std::vector<EntityId> targets;
            grid.QueryRange(pc->gravityWellPos, kPullRadius, targets);

            for (EntityId tid : targets) {
                if (tid == player) continue;
                EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(tid);
                if (!enemy || !enemy->active) continue;
                Transform* et = registry.GetComponent<Transform>(tid);
                Velocity* vel = registry.GetComponent<Velocity>(tid);
                if (!et || !vel) continue;

                sf::Vector2f toWell = pc->gravityWellPos - et->position;
                float dist = std::sqrt(toWell.x * toWell.x + toWell.y * toWell.y);
                if (dist > 5.f) {
                    toWell /= dist;
                    vel->linear += toWell * kPullForce * dt;
                }

                // 第十六轮新增：施加 Ice 状态（冰冻减速 2s）
                // 设计意图：引力井从"纯拉扯"升级为"控制+拉扯"组合——
                // 被引力井影响的敌人不仅被拉近，还附带 2s 冰冻减速，
                // 即使脱离引力井范围仍会减速，强化"控制场"定位，
                // 与地刺 Poison 流派形成"冰火毒"三元素策略维度。
                // CreateElementalStatus(Ice, 0) → 持续 2s，无伤害，由 EnemyAI 应用 50% 减速
                StatusEffect iceEff = CombatSystem::CreateElementalStatus(ElementType::Ice, 0.f);
                if (iceEff.duration > 0.f) {
                    combat.ApplyStatus(registry, tid, iceEff);
                }
            }

            // 持续发射紫色漩涡粒子（密度提高、双层）
            pc->gravityWellParticleTimer -= dt;
            if (pc->gravityWellParticleTimer <= 0.f) {
                pc->gravityWellParticleTimer = 0.06f; // 频率提高：0.08s → 0.06s

                // 内层：亮紫色快速粒子
                EmitConfig cfg;
                cfg.radial = true;
                cfg.speedMin = 80.f;
                cfg.speedMax = 150.f;
                cfg.colorMin = sf::Color(180, 80, 255);
                cfg.colorMax = sf::Color(230, 150, 255);
                cfg.sizeMin = 2.f;
                cfg.sizeMax = 4.f;
                cfg.lifeMin = 0.4f;
                cfg.lifeMax = 0.7f;
                particles.Emit(pc->gravityWellPos, 6, cfg);

                // 外层：深紫色慢速大粒子（漩涡雾气）
                EmitConfig aura;
                aura.radial = true;
                aura.speedMin = 30.f;
                aura.speedMax = 70.f;
                aura.colorMin = sf::Color(80, 20, 140, 180);
                aura.colorMax = sf::Color(140, 50, 200, 200);
                aura.sizeMin = 5.f;
                aura.sizeMax = 8.f;
                aura.lifeMin = 0.6f;
                aura.lifeMax = 1.0f;
                particles.Emit(pc->gravityWellPos, 4, aura);
            }
        }
        if (pc->gravityWellTimer <= 0.f) {
            // 技能结束重置粒子计时器
            pc->gravityWellParticleTimer = 0.f;
        }
    }

    // ---- 地刺：对区域内敌人造成持续伤害+减速 + 持续粒子 ----
    if (pc->spikeGroundTimer > 0.f) {
        pc->spikeGroundTimer -= dt;
        if (pc->spikeGroundTimer <= 0.f) {
            pc->spikeGroundTimer = 0.f;
            // 技能结束时重置计时器，避免下次释放首次发射/tick 时机异常
            pc->spikeTickTimer = 0.f;
            pc->spikeParticleTimer = 0.f;
            pc->spikeBloodCounter = 0;
        } else {
            // 等级缩放：范围 100+10*(lv-1)
            // DPS 缩放：基础 5+3*(lv-1) × 玩家伤害的 30%，使技能后期仍有效
            const int sgLevel = GetSkillLevel(*pc, SkillType::SpikeGround);
            const float kSpikeRadius = 100.f + 10.f * (sgLevel - 1);
            const float kSpikeBaseDPS = 5.f + 3.f * (sgLevel - 1);
            const float kSpikeDPS = kSpikeBaseDPS + pc->stats.damage * 0.3f;
            constexpr float kSlowFactor = 0.5f; // 范围内敌人移速直接降为 50%

            // 持续发射地刺粒子（密度提高、双层）
            pc->spikeParticleTimer -= dt;
            if (pc->spikeParticleTimer <= 0.f) {
                pc->spikeParticleTimer = 0.1f; // 频率提高：0.15s → 0.1s

                // 尖刺粒子（向上喷射）
                EmitConfig cfg;
                cfg.radial = false;
                cfg.velocityMin = sf::Vector2f(-40.f, -120.f);
                cfg.velocityMax = sf::Vector2f(40.f, -70.f);
                cfg.colorMin = sf::Color(140, 90, 30);
                cfg.colorMax = sf::Color(210, 160, 70);
                cfg.sizeMin = 2.f;
                cfg.sizeMax = 4.f;
                cfg.lifeMin = 0.25f;
                cfg.lifeMax = 0.45f;
                particles.Emit(pc->spikeGroundPos, 5, cfg);

                // 间歇性血红色尖端（每 2 次发射一次，强化伤害感）
                if (++pc->spikeBloodCounter >= 2) {
                    pc->spikeBloodCounter = 0;
                    EmitConfig blood;
                    blood.radial = false;
                    blood.velocityMin = sf::Vector2f(-25.f, -150.f);
                    blood.velocityMax = sf::Vector2f(25.f, -100.f);
                    blood.colorMin = sf::Color(180, 30, 20, 230);
                    blood.colorMax = sf::Color(255, 80, 60, 240);
                    blood.sizeMin = 2.f;
                    blood.sizeMax = 3.f;
                    blood.lifeMin = 0.2f;
                    blood.lifeMax = 0.4f;
                    particles.Emit(pc->spikeGroundPos, 3, blood);
                }
            }

            // 每 0.5s 造成一次伤害（使用 PlayerComponent 成员计时器，避免 static 跨局残留）
            pc->spikeTickTimer += dt;
            if (pc->spikeTickTimer >= 0.5f) {
                pc->spikeTickTimer = 0.f;

                std::vector<EntityId> targets;
                grid.QueryRange(pc->spikeGroundPos, kSpikeRadius, targets);

                float tickDmg = kSpikeDPS * 0.5f;
                for (EntityId tid : targets) {
                    if (tid == player) continue;
                    EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(tid);
                    if (!enemy || !enemy->active) continue;

                    DamageInfo dmgInfo;
                    dmgInfo.attacker = player;
                    dmgInfo.target = tid;
                    dmgInfo.amount = tickDmg;
                    dmgInfo.isCritical = false;
                    // 第十六轮新增：地刺改 Poison 元素
                    // 设计意图：让地刺技能的减速 + DoT 与"中毒"语义自洽，
                    // 同时叠加 Poison 状态（每 1s 造成 10% tickDmg，持续 5s），
                    // 即使敌人离开地刺范围仍会持续受伤，强化"持续伤害区域"定位。
                    dmgInfo.element = ElementType::Poison;
                    dmgInfo.knockback = sf::Vector2f(0.f, 0.f);
                    dmgInfo.lifesteal = 0.f;
                    combat.ApplyDamage(registry, dmgInfo);

                    // 额外施加 Poison 状态效果（与单次 tick 伤害独立，持续 5s）
                    // CreateElementalStatus(Poison, tickDmg) → 每 1s 造成 0.1*tickDmg，持续 5s
                    StatusEffect poisonEff = CombatSystem::CreateElementalStatus(
                        ElementType::Poison, tickDmg);
                    if (poisonEff.duration > 0.f) {
                        combat.ApplyStatus(registry, tid, poisonEff);
                    }
                }
            }

            // 减速效果：设置 EnemyComponent::slowFactor，由 EnemyAI 在合成
            // desiredVelocity 时应用（作用于 flowDir * moveSpeed 的主动移动速度）。
            //
            // 【历史 Bug 修复】此前直接执行 vel->linear *= (1 - kSlowFactor) 完全无效：
            //   - EnemyAI 计算 desiredVelocity = flowDir * moveSpeed 时根本不读取
            //     vel->linear，vel->linear 仅作为 externalVel 用于击退/引力井拉扯叠加；
            //   - 因此对 vel->linear 乘 0.5 只影响外部速度，敌人主动移动速度不变；
            //   - 第十二轮注释中"敌人 AI 每帧重算 vel->linear"的判断有误，实际
            //     重算的是 desiredVelocity，与 vel->linear 无关。
            // 正确做法：通过 EnemyComponent::slowFactor 字段传递减速效果。
            std::vector<EntityId> slowTargets;
            grid.QueryRange(pc->spikeGroundPos, kSpikeRadius, slowTargets);
            for (EntityId tid : slowTargets) {
                if (tid == player) continue;
                EnemyComponent* enemy = registry.GetComponent<EnemyComponent>(tid);
                if (!enemy || !enemy->active) continue;
                enemy->slowFactor = kSlowFactor; // EnemyAI 下一帧应用并重置
            }
        }
    }
}

// ============================================================================
// 技能槽管理
// ============================================================================
bool EquipSkill(PlayerComponent& pc, int backpackIndex, int slotIndex) {
    if (backpackIndex < 0 || backpackIndex >= kSkillBackpackSize) return false;
    if (slotIndex < 0 || slotIndex >= kSkillSlotCount) return false;

    SkillType backpackSkill = pc.skillBackpack[backpackIndex];
    if (backpackSkill == SkillType::Count) return false; // 背包格空

    SkillType slotSkill = pc.skillSlots[slotIndex].type;

    // 交换：背包技能→槽位，槽位技能→背包同位置
    pc.skillSlots[slotIndex].type = backpackSkill;
    pc.skillSlots[slotIndex].cooldownRemain = 0.f;
    // 背包中的技能等级固定为 1（skillBackpack 仅存类型，无法保留等级），
    // 装备时必须重置为 1，否则会继承槽位中上一个技能的等级。
    pc.skillSlots[slotIndex].level = 1;
    pc.skillBackpack[backpackIndex] = slotSkill; // 可能为 Count（空）

    return true;
}

bool UnequipSkill(PlayerComponent& pc, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= kSkillSlotCount) return false;

    SkillType slotSkill = pc.skillSlots[slotIndex].type;
    if (slotSkill == SkillType::Count) return false; // 槽位空

    // 检查背包是否有空位
    int emptyIdx = -1;
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (pc.skillBackpack[i] == SkillType::Count) {
            emptyIdx = i;
            break;
        }
    }
    if (emptyIdx < 0) return false; // 背包满

    // 注意：卸下后技能等级丢失（背包仅存类型）。若未来需保留等级，
    // 需将 skillBackpack 改为 std::array<SkillInstance, kSkillBackpackSize>。
    pc.skillBackpack[emptyIdx] = slotSkill;
    pc.skillSlots[slotIndex].type = SkillType::Count;
    pc.skillSlots[slotIndex].cooldownRemain = 0.f;
    pc.skillSlots[slotIndex].level = 1; // 重置，避免下次装备时继承

    return true;
}

bool AddSkillToBackpack(PlayerComponent& pc, SkillType type) {
    if (type == SkillType::Count) return false;

    // 检查是否已拥有
    if (PlayerHasSkill(pc, type)) return false;

    // 找空位
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (pc.skillBackpack[i] == SkillType::Count) {
            pc.skillBackpack[i] = type;
            return true;
        }
    }
    // 背包满，尝试放入空技能槽
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (pc.skillSlots[i].type == SkillType::Count) {
            pc.skillSlots[i].type = type;
            pc.skillSlots[i].cooldownRemain = 0.f;
            return true;
        }
    }
    return false; // 全满
}

bool IsSkillBackpackFull(const PlayerComponent& pc) {
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (pc.skillBackpack[i] == SkillType::Count) return false;
    }
    return true;
}

bool PlayerHasSkill(const PlayerComponent& pc, SkillType type) {
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (pc.skillSlots[i].type == type) return true;
    }
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (pc.skillBackpack[i] == type) return true;
    }
    return false;
}

// 获取玩家某技能的当前等级（未拥有返回 0）
int GetSkillLevel(const PlayerComponent& pc, SkillType type) {
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (pc.skillSlots[i].type == type) return pc.skillSlots[i].level;
    }
    // 背包中的技能默认 1 级（未装备）
    for (int i = 0; i < kSkillBackpackSize; ++i) {
        if (pc.skillBackpack[i] == type) return 1;
    }
    return 0;
}

// 升级玩家已拥有的技能（level++，仅对装备槽中的技能生效）
bool UpgradeSkillLevel(PlayerComponent& pc, SkillType type) {
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (pc.skillSlots[i].type == type) {
            if (pc.skillSlots[i].level < kSkillMaxLevel) {
                ++pc.skillSlots[i].level;
                LOG_INFO("技能 %s 升级到 Lv.%d", GetSkillName(type), pc.skillSlots[i].level);
                return true;
            }
            return false; // 已满级
        }
    }
    // 技能在背包中（未装备）：无法直接升级，需先装备
    LOG_WARN("技能 %s 在背包中未装备，无法升级", GetSkillName(type));
    return false;
}

} // namespace cu
