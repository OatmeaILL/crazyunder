#pragma once

// ============================================================================
// DungeonGenerator —— BSP 树递归二分地牢生成器
// ----------------------------------------------------------------------------
// 核心算法：BSP（Binary Space Partitioning，二叉空间划分）
//   1. 初始矩形 = 整个地牢区域（如 100x100 tiles）
//   2. 递归二分：选择水平或垂直切分，在 40%-60% 间随机切分点
//   3. 递归直到矩形小于阈值（如 20x20）
//   4. 在每个叶子矩形内挖房间（留边距）
//   5. 连接兄弟节点房间（走廊，L 形或直线）
//   6. 标记房间类型：起点、Boss（离起点最远）、楼梯、宝箱、精英、隐藏
//
// BSP 的优势：
//   - 生成的地牢结构有机：房间大小不一，走廊自然连接，无重叠
//   - 层次化结构：兄弟节点必连，保证连通性
//   - 可控性：通过调整分裂阈值、边距等参数控制地牢风格
//
// 种子支持：
//   使用 std::mt19937 梅森旋转随机数引擎，同种子生成同地牢，
//   便于调试与分享地牢种子。
//
// Tile 坐标系：
//   tile (x,y) 对应世界 (x*tileSize + worldOffset.x, y*tileSize + worldOffset.y)
//   tileSize = 32，worldOffset 默认使地牢中心位于世界原点 (0,0)
// ============================================================================

#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdint>
#include <random>
#include <memory>
#include <unordered_map>

namespace cu {

// ---- Tile 类型枚举 ----
enum class TileType : uint8_t {
    Empty                = 0, // 空（未生成区域，不渲染）
    Floor                = 1, // 地板
    Wall                 = 2, // 墙壁
    Door                 = 3, // 门（关闭时阻挡，打开或破坏后可通行）
    Obstacle             = 4, // 障碍物（桶/箱子，可破坏）
    Stairs               = 5, // 下楼楼梯
    Chest                = 6, // 宝箱
    IndestructibleObstacle = 7 // 不可破坏障碍物（石柱/铁块）
};

// ---- 房间类型枚举 ----
enum class RoomType : uint8_t {
    Normal    = 0, // 普通房间
    Treasure  = 1, // 宝箱房
    Elite     = 2, // 精英房
    Hidden    = 3, // 隐藏房
    Boss      = 4, // Boss 房
    Stairs    = 5, // 楼梯房
    Spawn     = 6, // 出生房（初始房间）
    Trap      = 7, // 陷阱房（进入后锁门、刷大量怪）
    Obstacle  = 8, // 阻碍房（含可破坏/不可破坏障碍物）
    Event     = 9, // 事件房（特殊 NPC 交互，4 种随机事件）
    Cursed    = 10 // 诅咒房（进入后被诅咒，清理后解除并掉落高级装备）
};

// ---- 事件房的事件类型 ----
enum class EventType : uint8_t {
    None       = 0, // 非事件房
    Beggar     = 1, // 乞丐：给金币换经验/回血
    Mage       = 2, // 神秘法师：献祭 HP 换装备
    ChestMimic = 3, // 宝箱怪：假宝箱变怪物
    Altar      = 4, // 祭坛：献祭金币换永久强化
    Forge      = 5  // 锻造房：花费金币升级穿戴中的装备品质
};

// ---- Tile 尺寸常量 ----
inline constexpr int kTileSize = 32; // 每个 Tile 的像素尺寸

// ---- 门状态结构体 ----
struct DoorState {
    bool open = false;        // false=关闭(阻挡), true=打开(可通行)
    bool locked = false;      // true=上锁（玩家无法 E 键切换，陷阱房用）
    float hp = 100.f;         // 门当前 HP
    float maxHp = 100.f;      // 门最大 HP
};

// ---- 房间结构体 ----
struct Room {
    sf::IntRect bounds;                  // 房间边界（tile 坐标）
    RoomType type = RoomType::Normal;    // 房间类型
    bool cleared = false;                // 是否已清理（敌人全灭）
    bool connected = false;              // 是否已连接到走廊
    std::vector<sf::Vector2i> doors;     // 门的位置（tile 坐标）
    sf::Vector2i center{0, 0};           // 房间中心（tile 坐标）
    int index = -1;                      // 房间在 Dungeon::rooms 中的索引
    EventType eventType = EventType::None; // 事件房的具体事件类型（仅 type==Event 时有效）
    bool eventTriggered = false;         // 事件房是否已被触发（避免重复触发）

    [[nodiscard]] sf::FloatRect getWorldBounds(sf::Vector2f worldOffset) const noexcept {
        return sf::FloatRect(
            bounds.left * static_cast<float>(kTileSize) + worldOffset.x,
            bounds.top * static_cast<float>(kTileSize) + worldOffset.y,
            bounds.width * static_cast<float>(kTileSize),
            bounds.height * static_cast<float>(kTileSize)
        );
    }
};

// ---- 地牢结构体 ----
struct Dungeon {
    std::vector<TileType> tiles;         // 二维 TileType 数组（行优先，[y*width+x]）
    std::vector<Room> rooms;             // 所有房间
    std::unordered_map<int, DoorState> doorStates; // 门状态（key = y*width+x）
    int width = 0;                       // 宽度（tile 数）
    int height = 0;                      // 高度（tile 数）
    int startRoom = -1;                  // 起点房间索引
    int bossRoom = -1;                   // Boss 房间索引
    int stairsRoom = -1;                 // 楼梯房间索引
    sf::Vector2i stairsPos{0, 0};        // 楼梯位置（tile 坐标）
    sf::Vector2f worldOffset{0.f, 0.f};  // 世界偏移（tile (0,0) 对应的世界坐标）

    // ---- Tile 访问 ----

    // 获取 tile（带边界检查，越界返回 Wall）
    [[nodiscard]] TileType GetTile(int x, int y) const noexcept {
        if (x < 0 || x >= width || y < 0 || y >= height) return TileType::Wall;
        return tiles[static_cast<std::size_t>(y) * width + x];
    }

    // 设置 tile（带边界检查）
    void SetTile(int x, int y, TileType type) noexcept {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        tiles[static_cast<std::size_t>(y) * width + x] = type;
    }

    // ---- 门状态访问 ----

    // 获取门状态指针（非门 tile 返回 nullptr）
    DoorState* GetDoorState(int x, int y) noexcept {
        int idx = y * width + x;
        auto it = doorStates.find(idx);
        return (it != doorStates.end()) ? &it->second : nullptr;
    }

    // 获取门状态指针（const 版本）
    [[nodiscard]] const DoorState* GetDoorState(int x, int y) const noexcept {
        int idx = y * width + x;
        auto it = doorStates.find(idx);
        return (it != doorStates.end()) ? &it->second : nullptr;
    }

    // 检查指定位置的门是否打开（非门 tile 返回 true=可通行）
    [[nodiscard]] bool IsDoorOpen(int x, int y) const noexcept {
        const DoorState* ds = GetDoorState(x, y);
        return ds ? ds->open : true;
    }

    // 检查指定位置的 tile 是否阻挡移动（Wall/Obstacle/IndestructibleObstacle/Chest/Empty 或关闭的门）
    [[nodiscard]] bool IsBlocked(int x, int y) const noexcept {
        TileType t = GetTile(x, y);
        if (t == TileType::Wall || t == TileType::Obstacle ||
            t == TileType::IndestructibleObstacle ||
            t == TileType::Chest || t == TileType::Empty) {
            return true;
        }
        if (t == TileType::Door) {
            return !IsDoorOpen(x, y); // 关闭的门阻挡
        }
        return false;
    }

    // ---- 坐标转换 ----

    // tile 坐标 → 世界坐标（tile 左上角）
    [[nodiscard]] sf::Vector2f TileToWorld(sf::Vector2i tile) const noexcept {
        return sf::Vector2f(
            tile.x * static_cast<float>(kTileSize) + worldOffset.x,
            tile.y * static_cast<float>(kTileSize) + worldOffset.y
        );
    }

    // tile 坐标 → 世界坐标（tile 中心）
    [[nodiscard]] sf::Vector2f TileCenterToWorld(sf::Vector2i tile) const noexcept {
        return sf::Vector2f(
            (tile.x + 0.5f) * static_cast<float>(kTileSize) + worldOffset.x,
            (tile.y + 0.5f) * static_cast<float>(kTileSize) + worldOffset.y
        );
    }

    // 世界坐标 → tile 坐标
    [[nodiscard]] sf::Vector2i WorldToTile(sf::Vector2f world) const noexcept {
        return sf::Vector2i(
            static_cast<int>((world.x - worldOffset.x) / kTileSize),
            static_cast<int>((world.y - worldOffset.y) / kTileSize)
        );
    }

    // 获取 tile 的世界边界
    [[nodiscard]] sf::FloatRect GetTileWorldBounds(int tileX, int tileY) const noexcept {
        return sf::FloatRect(
            tileX * static_cast<float>(kTileSize) + worldOffset.x,
            tileY * static_cast<float>(kTileSize) + worldOffset.y,
            static_cast<float>(kTileSize),
            static_cast<float>(kTileSize)
        );
    }
};

// ---- BSP 节点结构体 ----
struct BSPNode {
    sf::IntRect bounds;                          // 矩形边界（tile 坐标）
    std::unique_ptr<BSPNode> left;               // 左子节点
    std::unique_ptr<BSPNode> right;              // 右子节点
    int roomIndex = -1;                          // 叶子节点的房间索引（-1=非叶子）

    [[nodiscard]] bool IsLeaf() const noexcept { return !left && !right; }
};

// ============================================================================
// DungeonGenerator —— 地牢生成器
// ============================================================================
class DungeonGenerator {
public:
    DungeonGenerator();
    ~DungeonGenerator() = default;

    // 生成完整地牢
    // seed: 随机种子（同种子生成同地牢）
    // width/height: 地牢尺寸（tile 数）
    [[nodiscard]] Dungeon Generate(uint32_t seed,
                                    int width = kDefaultDungeonSize,
                                    int height = kDefaultDungeonSize);

    // ---- 工具函数 ----

    // 获取房间内随机位置（世界坐标，用于敌人/道具生成）
    [[nodiscard]] static sf::Vector2f GetSpawnPosition(const Dungeon& dungeon,
                                                        const Room& room);

    // 检查 tile 是否可通行（Floor/Door(打开)/Stairs）
    [[nodiscard]] static bool IsWalkable(const Dungeon& dungeon, int tileX, int tileY) noexcept;

    // 检查世界坐标处的 tile 是否可通行
    [[nodiscard]] static bool IsWalkableWorld(const Dungeon& dungeon,
                                              sf::Vector2f worldPos) noexcept;

    // 获取世界坐标处的 tile 类型
    [[nodiscard]] static TileType GetTileAt(const Dungeon& dungeon,
                                             sf::Vector2f worldPos) noexcept;

    // ---- 默认参数 ----
    static constexpr int kDefaultDungeonSize = 100;       // 默认地牢尺寸
    static constexpr int kMinSplitSize = 20;              // 最小可分裂尺寸（小于此值成为叶子）
    static constexpr int kRoomMargin = 2;                 // 房间与叶子边界的边距
    static constexpr int kMinRoomSize = 6;                // 最小房间尺寸

private:
    // ---- BSP 递归分裂 ----
    // 递归二分节点，直到节点尺寸小于阈值
    void splitNode(BSPNode& node);

    // ---- 房间挖掘 ----
    // 在叶子节点内挖房间，返回房间索引
    void carveRoom(Dungeon& dungeon, BSPNode& node);

    // ---- 走廊连接 ----
    // 递归连接兄弟节点的房间
    void connectRooms(Dungeon& dungeon, BSPNode& node);

    // 在两点间挖 L 形走廊
    void carveCorridor(Dungeon& dungeon, sf::Vector2i start, sf::Vector2i end);

    // ---- 墙壁生成 ----
    // 将所有与 Floor 相邻的 Empty tile 设为 Wall
    void generateWalls(Dungeon& dungeon);

    // ---- 门放置 ----
    // 检测走廊与房间交界处，放置门
    void placeDoors(Dungeon& dungeon);

    // ---- 房间类型分配 ----
    // 标记起点、Boss（离起点最远）、楼梯、宝箱、精英、隐藏
    void assignRoomTypes(Dungeon& dungeon);

    // ---- 楼梯/宝箱/障碍物放置 ----
    void placeStairs(Dungeon& dungeon);
    void placeChests(Dungeon& dungeon);
    void placeObstacles(Dungeon& dungeon);

    // ---- 随机数辅助 ----
    [[nodiscard]] int randomInt(int min, int max) noexcept;
    [[nodiscard]] float randomFloat(float min, float max) noexcept;
    [[nodiscard]] bool randomChance(float probability) noexcept;

    // ---- BSP 根节点 ----
    std::unique_ptr<BSPNode> root_;
    std::mt19937 rng_; // 梅森旋转随机数引擎
};

// ---- 房间类型名称（调试用）----
[[nodiscard]] inline const char* RoomTypeName(RoomType t) noexcept {
    switch (t) {
        case RoomType::Normal:    return "Normal";
        case RoomType::Treasure:  return "Treasure";
        case RoomType::Elite:     return "Elite";
        case RoomType::Hidden:    return "Hidden";
        case RoomType::Boss:      return "Boss";
        case RoomType::Stairs:    return "Stairs";
        case RoomType::Spawn:     return "Spawn";
        case RoomType::Trap:      return "Trap";
        case RoomType::Obstacle:  return "Obstacle";
        case RoomType::Event:     return "Event";
        case RoomType::Cursed:    return "Cursed";
    }
    return "?";
}

// ---- 事件类型名称（调试/UI 用）----
[[nodiscard]] inline const char* EventTypeName(EventType t) noexcept {
    switch (t) {
        case EventType::None:       return "None";
        case EventType::Beggar:     return "Beggar";
        case EventType::Mage:       return "Mage";
        case EventType::ChestMimic: return "ChestMimic";
        case EventType::Altar:      return "Altar";
        case EventType::Forge:      return "Forge";
    }
    return "?";
}

// ---- Tile 类型名称（调试用）----
[[nodiscard]] inline const char* TileTypeName(TileType t) noexcept {
    switch (t) {
        case TileType::Empty:                return "Empty";
        case TileType::Floor:                return "Floor";
        case TileType::Wall:                 return "Wall";
        case TileType::Door:                 return "Door";
        case TileType::Obstacle:             return "Obstacle";
        case TileType::Stairs:               return "Stairs";
        case TileType::Chest:                return "Chest";
        case TileType::IndestructibleObstacle: return "IndestructibleObstacle";
    }
    return "?";
}

} // namespace cu
