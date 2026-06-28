#include "core/AudioManager.h"
#include "utils/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace cu {

AudioManager& AudioManager::Instance() {
    static AudioManager instance;
    return instance;
}

AudioManager::AudioManager() {
    bgmVolume_ = 50.f;
    sfxVolume_ = 70.f;
}

void AudioManager::Initialize() {
    generateSFXBuffers();
    LOG_INFO("AudioManager 初始化完成，已生成 %zu 个占位音效缓冲", soundBuffers_.size());
}

void AudioManager::PlayBGM(const std::string& name) {
    if (!isValidBGM(name)) {
        LOG_WARN("[Audio] 未知 BGM 名称: %s", name.c_str());
        return;
    }

    if (currentBGM_ == name) return;
    StopBGM();
    currentBGM_ = name;

    // BGM 仍使用占位：记录日志（可在此替换为真实文件）
    // bgmMusic_.openFromFile("assets/bgm/" + name + ".ogg");
    // bgmMusic_.setVolume(bgmVolume_);
    // bgmMusic_.setLoop(true);
    // bgmMusic_.play();
    LOG_INFO("[Audio] BGM: %s", name.c_str());
}

void AudioManager::StopBGM() {
    if (currentBGM_.empty()) return;
    bgmMusic_.stop();
    LOG_INFO("[Audio] BGM 停止: %s", currentBGM_.c_str());
    currentBGM_.clear();
}

void AudioManager::PlaySFX(const std::string& name) {
    if (!isValidSFX(name)) {
        LOG_WARN("[Audio] 未知 SFX 名称: %s", name.c_str());
        return;
    }

    auto it = soundBuffers_.find(name);
    if (it == soundBuffers_.end() || !it->second) {
        LOG_WARN("[Audio] 音效缓冲不存在: %s", name.c_str());
        return;
    }

    sf::Sound& sfx = sfxPool_[sfxPoolIndex_];
    sfx.setBuffer(*it->second);
    sfx.setVolume(sfxVolume_);
    sfx.play();
    sfxPoolIndex_ = (sfxPoolIndex_ + 1) % kSFXPoolSize;
}

void AudioManager::SetBGMVolume(float volume) {
    bgmVolume_ = std::max(0.f, std::min(100.f, volume));
    bgmMusic_.setVolume(bgmVolume_);
    LOG_INFO("[Audio] BGM 音量: %.0f", bgmVolume_);
}

void AudioManager::SetSFXVolume(float volume) {
    sfxVolume_ = std::max(0.f, std::min(100.f, volume));
    LOG_INFO("[Audio] SFX 音量: %.0f", sfxVolume_);
}

// ============================================================================
// 过程化音效生成
// ============================================================================

std::unique_ptr<sf::SoundBuffer> AudioManager::generateSineBuffer(
    float frequency, float duration, float sampleRate) const {
    const std::size_t sampleCount = static_cast<std::size_t>(sampleRate * duration);
    std::vector<sf::Int16> samples(sampleCount);

    for (std::size_t i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float phase = 2.f * 3.14159265f * frequency * t;
        samples[i] = static_cast<sf::Int16>(32767.f * std::sin(phase));
    }

    auto buffer = std::make_unique<sf::SoundBuffer>();
    if (!buffer->loadFromSamples(samples.data(), samples.size(), 1, static_cast<unsigned>(sampleRate))) {
        LOG_WARN("生成正弦波缓冲失败");
        return nullptr;
    }
    return buffer;
}

std::unique_ptr<sf::SoundBuffer> AudioManager::generateToneBuffer(
    float frequency, float duration, bool decay, float sampleRate) const {
    const std::size_t sampleCount = static_cast<std::size_t>(sampleRate * duration);
    std::vector<sf::Int16> samples(sampleCount);

    for (std::size_t i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float phase = 2.f * 3.14159265f * frequency * t;
        float amp = 1.f;
        if (decay) {
            amp = std::max(0.f, 1.f - t / duration);
        }
        samples[i] = static_cast<sf::Int16>(32767.f * amp * std::sin(phase));
    }

    auto buffer = std::make_unique<sf::SoundBuffer>();
    if (!buffer->loadFromSamples(samples.data(), samples.size(), 1, static_cast<unsigned>(sampleRate))) {
        LOG_WARN("生成衰减正弦波缓冲失败");
        return nullptr;
    }
    return buffer;
}

std::unique_ptr<sf::SoundBuffer> AudioManager::generateNoiseBuffer(
    float duration, float sampleRate) const {
    const std::size_t sampleCount = static_cast<std::size_t>(sampleRate * duration);
    std::vector<sf::Int16> samples(sampleCount);

    for (std::size_t i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float amp = std::max(0.f, 1.f - t / duration);
        float noise = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.f - 1.f;
        samples[i] = static_cast<sf::Int16>(32767.f * amp * noise);
    }

    auto buffer = std::make_unique<sf::SoundBuffer>();
    if (!buffer->loadFromSamples(samples.data(), samples.size(), 1, static_cast<unsigned>(sampleRate))) {
        LOG_WARN("生成噪声缓冲失败");
        return nullptr;
    }
    return buffer;
}

void AudioManager::generateSFXBuffers() {
    // 从文件加载音效（sound文件夹）
    // 检查多个可能的路径：当前目录、可执行文件目录、项目根目录
    std::string soundDir = "sound/";
    if (!std::filesystem::exists(soundDir)) {
        // 尝试可执行文件所在目录
        soundDir = "./sound/";
        if (!std::filesystem::exists(soundDir)) {
            // 尝试项目根目录（调试时）
            soundDir = "../sound/";
            if (!std::filesystem::exists(soundDir)) {
                soundDir = "../../sound/";
            }
        }
    }
    LOG_INFO("音效文件目录: %s", soundDir.c_str());
    
    // 基础命中：子弹命中
    loadSoundFromFile(kSFXHit, soundDir + "bullet_hit.mp3");
    // 射击
    loadSoundFromFile(kSFXShoot, soundDir + "player_atk_shoot.mp3");
    // 爆炸（玩家技能爆炸和怪物爆炸共用）
    loadSoundFromFile(kSFXExplosion, soundDir + "playerskill_monster_explode.mp3");
    // 升级
    loadSoundFromFile(kSFXLevelUp, soundDir + "level_up.mp3");
    // 拾取（菜单点击）
    loadSoundFromFile(kSFXPickup, soundDir + "menu_click.mp3");
    // 死亡（怪物死亡）- 复用玩家受伤音效
    loadSoundFromFile(kSFXDeath, soundDir + "player_hurt.mp3");
    // Boss 命中（受伤）
    loadSoundFromFile(kSFXBossHit, soundDir + "player_hurt.mp3");

    // 新增音效
    // 经验拾取
    loadSoundFromFile(kSFXExpPickup, soundDir + "exp_gain.mp3");
    // 开门
    loadSoundFromFile(kSFXDoorOpen, soundDir + "door_open.mp3");
    // 开宝箱
    loadSoundFromFile(kSFXChestOpen, soundDir + "chest_open.mp3");
    // 金币拾取
    loadSoundFromFile(kSFXCoinPickup, soundDir + "get_coin_and_equipment.mp3");
    // 购买（使用 sell_2，与商人交互音区分）
    loadSoundFromFile(kSFXBuy, soundDir + "sell_2.mp3");
    // AOE（爆炸，和爆炸共用音效）
    loadSoundFromFile(kSFXAOE, soundDir + "playerskill_monster_explode.mp3");

    // 新增音效
    loadSoundFromFile(kSFXDoorClose, soundDir + "door_close.mp3");
    loadSoundFromFile(kSFXEquip, soundDir + "equip_equipment_and_skill.mp3");
    loadSoundFromFile(kSFXMerchant, soundDir + "interact_with_merchant.mp3");
    loadSoundFromFile(kSFXFootstep, soundDir + "player_footstep.mp3");
    loadSoundFromFile(kSFXPlayerHurt, soundDir + "player_hurt.mp3");
    loadSoundFromFile(kSFXSell, soundDir + "sell_1.mp3");
    
    // 第二十七轮新增音效
    loadSoundFromFile(kSFXChallengeComplete, soundDir + "challenge_or_achevement_complete.mp3");
    loadSoundFromFile(kSFXBomberWalk, soundDir + "bomber_walking_sound.mp3");
    loadSoundFromFile(kSFXBomberCharge, soundDir + "bomber_chargeing_bomb.mp3");
    loadSoundFromFile(kSFXVictory, soundDir + "next_floor_victory_sound.mp3");
    loadSoundFromFile(kSFXQuestTip, soundDir + "quest_can_be_complete_tipsound.mp3");
    loadSoundFromFile(kSFXQuestReward, soundDir + "quest_obtain_reward_sound.mp3");
    loadSoundFromFile(kSFXPlayerDeath, soundDir + "player_die_sound.mp3");
    
    LOG_INFO("已加载 %zu 个音效文件", soundBuffers_.size());
}

void AudioManager::loadSoundFromFile(const std::string& name, const std::string& path) {
    auto buffer = std::make_unique<sf::SoundBuffer>();
    if (buffer->loadFromFile(path)) {
        soundBuffers_[name] = std::move(buffer);
    } else {
        LOG_WARN("加载音效文件失败: %s，使用过程化生成作为回退", path.c_str());
        // 回退到过程化生成
        if (name == kSFXHit || name == kSFXBossHit) {
            soundBuffers_[name] = generateToneBuffer(440.f, 0.08f);
        } else if (name == kSFXShoot) {
            soundBuffers_[name] = generateToneBuffer(880.f, 0.06f);
        } else if (name == kSFXExplosion || name == kSFXAOE) {
            soundBuffers_[name] = generateNoiseBuffer(0.25f);
        } else if (name == kSFXLevelUp) {
            soundBuffers_[name] = generateToneBuffer(660.f, 0.15f);
        } else if (name == kSFXPickup) {
            soundBuffers_[name] = generateToneBuffer(1320.f, 0.08f);
        } else if (name == kSFXDeath) {
            soundBuffers_[name] = generateToneBuffer(220.f, 0.35f);
        } else if (name == kSFXExpPickup) {
            soundBuffers_[name] = generateToneBuffer(1760.f, 0.06f);
        } else if (name == kSFXDoorOpen) {
            soundBuffers_[name] = generateToneBuffer(350.f, 0.12f);
        } else if (name == kSFXChestOpen) {
            soundBuffers_[name] = generateToneBuffer(880.f, 0.18f);
        } else if (name == kSFXCoinPickup) {
            soundBuffers_[name] = generateToneBuffer(1568.f, 0.05f);
        } else if (name == kSFXBuy) {
            soundBuffers_[name] = generateToneBuffer(1175.f, 0.08f);
        } else if (name == kSFXDoorClose) {
            soundBuffers_[name] = generateToneBuffer(300.f, 0.10f);
        } else if (name == kSFXEquip) {
            soundBuffers_[name] = generateToneBuffer(660.f, 0.12f);
        } else if (name == kSFXMerchant) {
            soundBuffers_[name] = generateToneBuffer(550.f, 0.15f);
        } else if (name == kSFXFootstep) {
            soundBuffers_[name] = generateNoiseBuffer(0.05f);
        } else if (name == kSFXPlayerHurt) {
            soundBuffers_[name] = generateToneBuffer(330.f, 0.15f);
        } else if (name == kSFXSell) {
            soundBuffers_[name] = generateToneBuffer(880.f, 0.10f);
        } else if (name == kSFXChallengeComplete) {
            soundBuffers_[name] = generateToneBuffer(1320.f, 0.20f);
        } else if (name == kSFXBomberWalk) {
            soundBuffers_[name] = generateNoiseBuffer(0.15f);
        } else if (name == kSFXBomberCharge) {
            soundBuffers_[name] = generateToneBuffer(660.f, 0.30f, true);
        } else if (name == kSFXVictory) {
            soundBuffers_[name] = generateToneBuffer(1047.f, 0.40f);
        } else if (name == kSFXQuestTip) {
            soundBuffers_[name] = generateToneBuffer(1760.f, 0.08f);
        } else if (name == kSFXQuestReward) {
            soundBuffers_[name] = generateToneBuffer(1175.f, 0.18f);
        } else if (name == kSFXPlayerDeath) {
            soundBuffers_[name] = generateToneBuffer(220.f, 0.50f);
        }
    }
}

bool AudioManager::isValidBGM(const std::string& name) const {
    return name == kBGMMenu || name == kBGMDungeon || name == kBGMBoss;
}

bool AudioManager::isValidSFX(const std::string& name) const {
    return name == kSFXHit || name == kSFXShoot || name == kSFXExplosion ||
           name == kSFXLevelUp || name == kSFXPickup || name == kSFXDeath ||
           name == kSFXBossHit || name == kSFXExpPickup || name == kSFXDoorOpen ||
           name == kSFXChestOpen || name == kSFXCoinPickup || name == kSFXBuy ||
           name == kSFXAOE || name == kSFXDoorClose || name == kSFXEquip ||
           name == kSFXMerchant || name == kSFXFootstep || name == kSFXPlayerHurt ||
           name == kSFXSell ||
           name == kSFXChallengeComplete || name == kSFXBomberWalk ||
           name == kSFXBomberCharge || name == kSFXVictory ||
           name == kSFXQuestTip || name == kSFXQuestReward || name == kSFXPlayerDeath;
}

} // namespace cu
