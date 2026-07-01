#pragma once

// ============================================================================
// AudioManager —— SFML Audio 音效系统
// ----------------------------------------------------------------------------
// 职责：
//   1. 提供统一的音频播放接口（PlayBGM/PlaySFX）
//   2. 使用过程化生成的 SoundBuffer 作为占位音效（无需外部音频文件）
//   3. 不抛异常，不阻断游戏
//
// 单例模式：
//   全局唯一实例，通过 Instance() 访问。
//   生命周期与游戏进程一致。
//
// 预定义音效名：
//   hit、shoot、explosion、levelup、pickup、death、bosshit、
//   exppickup、dooropen、chestopen、coinpickup、buy、aoe、
//   doorclose、equip、merchant、footstep、playerhurt、sell
//
// 预定义 BGM 名：
//   menu、dungeon、boss
// ============================================================================

#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <array>
#include <memory>
#include <vector>

namespace cu {

class AudioManager {
public:
    // 获取单例实例
    static AudioManager& Instance();

    ~AudioManager() = default;

    // 禁止拷贝与移动
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    AudioManager(AudioManager&&) = delete;
    AudioManager& operator=(AudioManager&&) = delete;

    // 初始化：生成过程化音频缓冲
    void Initialize();

    // 播放背景音乐
    // name: BGM 名称（menu/dungeon/boss）
    void PlayBGM(const std::string& name);

    // 停止背景音乐
    void StopBGM();

    // 播放音效
    void PlaySFX(const std::string& name);

    // 设置 BGM 音量（0~100）
    void SetBGMVolume(float volume);

    // 设置 SFX 音量（0~100）
    void SetSFXVolume(float volume);

    // 获取当前正在播放的 BGM 名称
    [[nodiscard]] const std::string& GetCurrentBGM() const noexcept { return currentBGM_; }

    // 获取 BGM 音量
    [[nodiscard]] float GetBGMVolume() const noexcept { return bgmVolume_; }

    // 获取 SFX 音量
    [[nodiscard]] float GetSFXVolume() const noexcept { return sfxVolume_; }

    // 预定义音效名称
    static constexpr const char* kSFXHit       = "hit";
    static constexpr const char* kSFXShoot     = "shoot";
    static constexpr const char* kSFXExplosion = "explosion";
    static constexpr const char* kSFXLevelUp   = "levelup";
    static constexpr const char* kSFXPickup    = "pickup";
    static constexpr const char* kSFXDeath     = "death";
    static constexpr const char* kSFXBossHit   = "bosshit";

    // 新增音效
    static constexpr const char* kSFXExpPickup  = "exppickup";  // 拾取经验：清脆叮叮声
    static constexpr const char* kSFXDoorOpen   = "dooropen";   // 开门
    static constexpr const char* kSFXChestOpen  = "chestopen";  // 开宝箱
    static constexpr const char* kSFXCoinPickup = "coinpickup"; // 拾取金币
    static constexpr const char* kSFXBuy        = "buy";        // 购买/出售
    static constexpr const char* kSFXAOE        = "aoe";        // AOE 爆炸
    static constexpr const char* kSFXDoorClose  = "doorclose";  // 关门
    static constexpr const char* kSFXEquip      = "equip";      // 装备/技能穿戴
    static constexpr const char* kSFXMerchant   = "merchant";   // 商人交互
    static constexpr const char* kSFXFootstep   = "footstep";   // 脚步声
    static constexpr const char* kSFXPlayerHurt = "playerhurt"; // 玩家受伤
    static constexpr const char* kSFXSell       = "sell";       // 出售

    // 第二十七轮新增音效
    static constexpr const char* kSFXChallengeComplete = "challengecomplete"; // 成就/挑战完成
    static constexpr const char* kSFXBomberWalk        = "bomberwalk";        // 炸弹怪物行走
    static constexpr const char* kSFXBomberCharge      = "bombercharge";      // 炸弹怪物引信声
    static constexpr const char* kSFXVictory           = "victory";           // 过关胜利
    static constexpr const char* kSFXQuestTip          = "questtip";          // 任务可完成提示
    static constexpr const char* kSFXQuestReward       = "questreward";       // 任务领取奖励
    static constexpr const char* kSFXPlayerDeath       = "playerdeath"; // 玩家死亡

    // 剑士专属音效
    static constexpr const char* kSFXSwordSweep = "swordsweep"; // 剑士挥砍

    static constexpr const char* kBGMMenu    = "menu";
    static constexpr const char* kBGMDungeon = "dungeon";
    static constexpr const char* kBGMBoss    = "boss";

private:
    AudioManager();

    // ---- SFML Audio ----
    sf::Music bgmMusic_; // 背景音乐（流式播放）

    // 音效对象池（避免频繁创建）
    static constexpr int kSFXPoolSize = 24;
    std::array<sf::Sound, kSFXPoolSize> sfxPool_;
    int sfxPoolIndex_ = 0;

    // 音频缓冲缓存
    std::unordered_map<std::string, std::unique_ptr<sf::SoundBuffer>> soundBuffers_;

    std::string currentBGM_;
    float bgmVolume_ = 50.f;  // BGM 音量（0~100）
    float sfxVolume_ = 70.f;  // SFX 音量（0~100）

    // 过程化生成音效缓冲
    void generateSFXBuffers();

    // 从文件加载音效（失败时回退到过程化生成）
    void loadSoundFromFile(const std::string& name, const std::string& path);

    // 辅助：生成正弦波缓冲
    [[nodiscard]] std::unique_ptr<sf::SoundBuffer> generateSineBuffer(
        float frequency, float duration, float sampleRate = 44100.f) const;

    // 辅助：生成衰减正弦波（打击感）
    [[nodiscard]] std::unique_ptr<sf::SoundBuffer> generateToneBuffer(
        float frequency, float duration, bool decay = true,
        float sampleRate = 44100.f) const;

    // 辅助：生成噪声爆发（爆炸感）
    [[nodiscard]] std::unique_ptr<sf::SoundBuffer> generateNoiseBuffer(
        float duration, float sampleRate = 44100.f) const;

    // 检查 BGM 名称是否有效
    [[nodiscard]] bool isValidBGM(const std::string& name) const;

    // 检查 SFX 名称是否有效
    [[nodiscard]] bool isValidSFX(const std::string& name) const;
};

} // namespace cu
