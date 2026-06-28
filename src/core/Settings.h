#pragma once

// ============================================================================
// Settings —— 游戏设置（音量、分辨率），以配置文件格式持久化存储
// ----------------------------------------------------------------------------
// 配置文件路径：可执行文件同目录下的 settings.ini
// 文件格式（键值对，# 开头为注释）：
//   # CrazyUnder 游戏设置
//   bgm_volume=50
//   sfx_volume=70
//   width=1280
//   height=720
//
// 使用方式：
//   Settings s;
//   s.Load();            // 读取配置（失败用默认值）
//   s.SetBGMVolume(80);  // 修改
//   s.Save();            // 保存到文件
// ============================================================================

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace cu {

class Settings {
public:
    Settings() = default;

    // 从文件加载设置（文件不存在或读取失败时使用默认值）
    void Load() {
        // 默认值
        bgmVolume_ = 50.f;
        sfxVolume_ = 70.f;
        width_ = 1280;
        height_ = 720;

        const std::string path = getFilePath();
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            // 文件不存在，使用默认值（首次运行）
            return;
        }
        // 强制使用 C locale，避免读取时受系统 locale 影响（如千位分隔符）
        ifs.imbue(std::locale::classic());

        std::string line;
        while (std::getline(ifs, line)) {
            // 跳过空行和注释
            if (line.empty() || line[0] == '#' || line[0] == '[') continue;
            // 解析 key=value
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;
            std::string key = line.substr(0, eqPos);
            std::string val = line.substr(eqPos + 1);
            // 去除首尾空白
            trim(key);
            trim(val);

            if (key == "bgm_volume") {
                bgmVolume_ = clamp(std::stof(val), 0.f, 100.f);
            } else if (key == "sfx_volume") {
                sfxVolume_ = clamp(std::stof(val), 0.f, 100.f);
            } else if (key == "width") {
                int w = std::stoi(val);
                // 合理性校验，防止异常值导致窗口无法显示
                if (w >= 640 && w <= 7680) width_ = w;
            } else if (key == "height") {
                int h = std::stoi(val);
                if (h >= 480 && h <= 4320) height_ = h;
            }
        }
    }

    // 保存设置到文件
    bool Save() const {
        const std::string path = getFilePath();
        std::ofstream ofs(path);
        if (!ofs.is_open()) return false;
        // 强制使用 C locale，避免输出时带千位分隔符（如 1600 → "1,600"）
        ofs.imbue(std::locale::classic());
        ofs << "# CrazyUnder 游戏设置\n";
        ofs << "bgm_volume=" << bgmVolume_ << "\n";
        ofs << "sfx_volume=" << sfxVolume_ << "\n";
        ofs << "width=" << width_ << "\n";
        ofs << "height=" << height_ << "\n";
        return true;
    }

    // ---- 音量 ----
    [[nodiscard]] float GetBGMVolume() const noexcept { return bgmVolume_; }
    [[nodiscard]] float GetSFXVolume() const noexcept { return sfxVolume_; }
    void SetBGMVolume(float v) { bgmVolume_ = clamp(v, 0.f, 100.f); }
    void SetSFXVolume(float v) { sfxVolume_ = clamp(v, 0.f, 100.f); }

    // ---- 分辨率 ----
    [[nodiscard]] int GetWidth() const noexcept { return width_; }
    [[nodiscard]] int GetHeight() const noexcept { return height_; }
    void SetResolution(int w, int h) { width_ = w; height_ = h; }

private:
    float bgmVolume_ = 50.f;  // BGM 音量 0-100
    float sfxVolume_ = 70.f;  // 音效音量 0-100
    int width_ = 1280;        // 窗口宽度
    int height_ = 720;        // 窗口高度

    // 获取配置文件路径（可执行文件同目录下的 settings.ini）
    static std::string getFilePath() {
        // 优先使用当前工作目录
        return "settings.ini";
    }

    // 去除字符串首尾空白
    static void trim(std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) { s.clear(); return; }
        size_t end = s.find_last_not_of(" \t\r\n");
        s = s.substr(start, end - start + 1);
    }

    // 数值钳制
    static float clamp(float v, float lo, float hi) {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }
};

} // namespace cu
