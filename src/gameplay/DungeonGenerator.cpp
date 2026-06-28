#include "gameplay/DungeonGenerator.h"
#include "utils/Logger.h"
#include <cmath>
#include <algorithm>
#include <queue>

namespace cu {

// ============================================================================
// 构造函数
// ============================================================================
DungeonGenerator::DungeonGenerator() = default;

// ============================================================================
// Generate —— 生成完整地牢
// ============================================================================
Dungeon DungeonGenerator::Generate(uint32_t seed, int width, int height) {
    LOG_INFO("开始生成地牢: seed=%u, size=%dx%d", seed, width, height);

    // 初始化随机数引擎
    rng_.seed(seed);

    Dungeon dungeon;
    dungeon.width = width;
    dungeon.height = height;
    dungeon.tiles.assign(static_cast<std::size_t>(width) * height, TileType::Empty);

    // 世界偏移：使地牢中心位于世界原点 (0,0)
    dungeon.worldOffset = sf::Vector2f(
        -width * kTileSize * 0.5f,
        -height * kTileSize * 0.5f
    );

    // ---- 1. 构建 BSP 树 ----
    root_ = std::make_unique<BSPNode>();
    root_->bounds = sf::IntRect(1, 1, width - 2, height - 2); // 留 1 tile 边界
    splitNode(*root_);

    // ---- 2. 在叶子节点中挖房间 ----
    dungeon.rooms.clear();
    carveRoom(dungeon, *root_);

    LOG_INFO("BSP 生成 %zu 个房间", dungeon.rooms.size());

    // ---- 3. 连接兄弟节点的房间（走廊）----
    connectRooms(dungeon, *root_);

    // ---- 4. 生成墙壁（Floor 相邻的 Empty → Wall）----
    generateWalls(dungeon);

    // ---- 5. 放置门（走廊与房间交界）----
    placeDoors(dungeon);

    // ---- 6. 分配房间类型（起点、Boss、楼梯等）----
    assignRoomTypes(dungeon);

    // ---- 7. 放置楼梯、宝箱、障碍物 ----
    placeStairs(dungeon);
    placeChests(dungeon);
    placeObstacles(dungeon);

    // 释放 BSP 树
    root_.reset();

    LOG_INFO("地牢生成完成: %zu 个房间, 起点房=%d, Boss房=%d, 楼梯房=%d",
             dungeon.rooms.size(), dungeon.startRoom, dungeon.bossRoom, dungeon.stairsRoom);

    return dungeon;
}

// ============================================================================
// splitNode —— BSP 递归分裂节点
// ----------------------------------------------------------------------------
// 算法：
//   1. 检查节点是否足够大（宽高都 >= 2 * kMinSplitSize），否则成为叶子
//   2. 选择分裂方向：
//      - 若宽 > 高，垂直分裂（左右）
//      - 若高 > 宽，水平分裂（上下）
//      - 若宽高相近，随机选择
//   3. 在 40%-60% 间随机选择分裂点
//   4. 创建左右子节点，递归分裂
// ============================================================================
void DungeonGenerator::splitNode(BSPNode& node) {
    const sf::IntRect& b = node.bounds;
    bool canSplitH = b.height >= 2 * kMinSplitSize; // 水平分裂（上下）
    bool canSplitV = b.width >= 2 * kMinSplitSize;  // 垂直分裂（左右）

    if (!canSplitH && !canSplitV) return; // 成为叶子

    // 选择分裂方向
    bool splitVertical; // true=垂直分裂（左右），false=水平分裂（上下）
    if (canSplitH && canSplitV) {
        // 两者都可：根据宽高比选择，或随机
        if (b.width > b.height * 1.25f) {
            splitVertical = true; // 偏宽，垂直分裂
        } else if (b.height > b.width * 1.25f) {
            splitVertical = false; // 偏高，水平分裂
        } else {
            splitVertical = randomChance(0.5f);
        }
    } else {
        splitVertical = canSplitV; // 只能垂直
    }

    // 分裂点：40%-60% 间随机
    if (splitVertical) {
        // 垂直分裂：左右两个子节点
        int minSplit = b.left + static_cast<int>(b.width * 0.4f);
        int maxSplit = b.left + static_cast<int>(b.width * 0.6f);
        if (minSplit >= maxSplit) return;
        int splitX = randomInt(minSplit, maxSplit);

        node.left = std::make_unique<BSPNode>();
        node.left->bounds = sf::IntRect(b.left, b.top, splitX - b.left, b.height);
        node.right = std::make_unique<BSPNode>();
        node.right->bounds = sf::IntRect(splitX, b.top, b.left + b.width - splitX, b.height);
    } else {
        // 水平分裂：上下两个子节点
        int minSplit = b.top + static_cast<int>(b.height * 0.4f);
        int maxSplit = b.top + static_cast<int>(b.height * 0.6f);
        if (minSplit >= maxSplit) return;
        int splitY = randomInt(minSplit, maxSplit);

        node.left = std::make_unique<BSPNode>();
        node.left->bounds = sf::IntRect(b.left, b.top, b.width, splitY - b.top);
        node.right = std::make_unique<BSPNode>();
        node.right->bounds = sf::IntRect(b.left, splitY, b.width, b.top + b.height - splitY);
    }

    // 递归分裂子节点
    splitNode(*node.left);
    splitNode(*node.right);
}

// ============================================================================
// carveRoom —— 在叶子节点中挖房间
// ----------------------------------------------------------------------------
// 遍历 BSP 树，对每个叶子节点：
//   1. 在叶子边界内留 kRoomMargin 边距，生成房间矩形
//   2. 房间尺寸不小于 kMinRoomSize
//   3. 将房间内的 tile 设为 Floor
//   4. 创建 Room 结构体，加入 dungeon.rooms
// ============================================================================
void DungeonGenerator::carveRoom(Dungeon& dungeon, BSPNode& node) {
    if (!node.IsLeaf()) {
        // 递归处理子节点
        carveRoom(dungeon, *node.left);
        carveRoom(dungeon, *node.right);
        return;
    }

    // 叶子节点：在边界内挖房间
    const sf::IntRect& b = node.bounds;

    // 房间边界 = 叶子边界缩小边距
    int roomX = b.left + kRoomMargin;
    int roomY = b.top + kRoomMargin;
    int maxW = b.width - 2 * kRoomMargin;
    int maxH = b.height - 2 * kRoomMargin;

    if (maxW < kMinRoomSize || maxH < kMinRoomSize) {
        // 边距后太小，直接用整个叶子（不留边距）
        roomX = b.left;
        roomY = b.top;
        maxW = b.width;
        maxH = b.height;
    }

    // 随机房间尺寸（在最大尺寸的 60%-100% 间）
    int roomW = std::max(kMinRoomSize, randomInt(static_cast<int>(maxW * 0.6f), maxW));
    int roomH = std::max(kMinRoomSize, randomInt(static_cast<int>(maxH * 0.6f), maxH));

    // 随机房间位置（在叶子内）
    int offsetX = randomInt(0, maxW - roomW);
    int offsetY = randomInt(0, maxH - roomH);

    sf::IntRect roomBounds(roomX + offsetX, roomY + offsetY, roomW, roomH);

    // 挖房间（设为 Floor）
    for (int y = roomBounds.top; y < roomBounds.top + roomBounds.height; ++y) {
        for (int x = roomBounds.left; x < roomBounds.left + roomBounds.width; ++x) {
            dungeon.SetTile(x, y, TileType::Floor);
        }
    }

    // 创建 Room 结构体
    Room room;
    room.bounds = roomBounds;
    room.center = sf::Vector2i(
        roomBounds.left + roomBounds.width / 2,
        roomBounds.top + roomBounds.height / 2
    );
    room.index = static_cast<int>(dungeon.rooms.size());
    node.roomIndex = room.index;
    dungeon.rooms.push_back(room);
}

// ============================================================================
// connectRooms —— 递归连接兄弟节点的房间
// ----------------------------------------------------------------------------
// 算法：
//   1. 若节点是叶子，返回
//   2. 递归连接左右子树
//   3. 连接左子树与右子树中各一个房间（取中心点最近的）
//   4. 用 L 形走廊连接两点
// ============================================================================
void DungeonGenerator::connectRooms(Dungeon& dungeon, BSPNode& node) {
    if (node.IsLeaf()) return;

    // 递归连接子树
    connectRooms(dungeon, *node.left);
    connectRooms(dungeon, *node.right);

    // 找到左右子树中各一个房间
    // 简化：取左右子树中第一个房间的中心
    auto findRoomInSubtree = [](const BSPNode& n) -> const Room* {
        // BFS 找到子树中第一个叶子节点的房间
        std::queue<const BSPNode*> q;
        q.push(&n);
        while (!q.empty()) {
            const BSPNode* cur = q.front();
            q.pop();
            if (cur->IsLeaf()) {
                if (cur->roomIndex >= 0) return nullptr; // 需要 dungeon 引用，这里返回空
            }
            if (cur->left) q.push(cur->left.get());
            if (cur->right) q.push(cur->right.get());
        }
        return nullptr;
    };

    // 简化：直接找左右子树的最深左叶子的房间
    auto findLeftmostRoom = [](const BSPNode& n) -> int {
        const BSPNode* cur = &n;
        while (cur->left) cur = cur->left.get();
        return cur->roomIndex;
    };
    auto findRightmostRoom = [](const BSPNode& n) -> int {
        const BSPNode* cur = &n;
        while (cur->right) cur = cur->right.get();
        return cur->roomIndex;
    };

    int leftRoomIdx = findLeftmostRoom(*node.left);
    int rightRoomIdx = findRightmostRoom(*node.right);

    if (leftRoomIdx < 0 || rightRoomIdx < 0) return;
    if (leftRoomIdx >= static_cast<int>(dungeon.rooms.size()) ||
        rightRoomIdx >= static_cast<int>(dungeon.rooms.size())) return;

    const Room& leftRoom = dungeon.rooms[leftRoomIdx];
    const Room& rightRoom = dungeon.rooms[rightRoomIdx];

    // 挖 L 形走廊
    carveCorridor(dungeon, leftRoom.center, rightRoom.center);

    // 标记已连接
    dungeon.rooms[leftRoomIdx].connected = true;
    dungeon.rooms[rightRoomIdx].connected = true;
}

// ============================================================================
// carveCorridor —— 在两点间挖 L 形走廊
// ----------------------------------------------------------------------------
// L 形走廊：先水平后垂直，或先垂直后水平（随机选择）
// 走廊宽度 = 1 tile（或 2 tile 更宽敞，这里用 2 tile 宽更易走）
// ============================================================================
void DungeonGenerator::carveCorridor(Dungeon& dungeon, sf::Vector2i start, sf::Vector2i end) {
    // 随机选择 L 形方向
    bool horizontalFirst = randomChance(0.5f);

    // 走廊宽度 = 1 tile（单 tile 走廊，便于门检测和战斗）
    const int corridorHalfWidth = 0;

    auto carveH = [&](int x1, int x2, int y) {
        if (x1 > x2) std::swap(x1, x2);
        for (int x = x1; x <= x2; ++x) {
            for (int dy = -corridorHalfWidth; dy <= corridorHalfWidth; ++dy) {
                int yy = y + dy;
                if (dungeon.GetTile(x, yy) == TileType::Empty) {
                    dungeon.SetTile(x, yy, TileType::Floor);
                }
            }
        }
    };

    auto carveV = [&](int x, int y1, int y2) {
        if (y1 > y2) std::swap(y1, y2);
        for (int y = y1; y <= y2; ++y) {
            for (int dx = -corridorHalfWidth; dx <= corridorHalfWidth; ++dx) {
                int xx = x + dx;
                if (dungeon.GetTile(xx, y) == TileType::Empty) {
                    dungeon.SetTile(xx, y, TileType::Floor);
                }
            }
        }
    };

    if (horizontalFirst) {
        carveH(start.x, end.x, start.y);
        carveV(end.x, start.y, end.y);
    } else {
        carveV(start.x, start.y, end.y);
        carveH(start.x, end.x, end.y);
    }
}

// ============================================================================
// generateWalls —— 生成墙壁
// ----------------------------------------------------------------------------
// 将所有与 Floor 相邻的 Empty tile 设为 Wall
// 这样房间和走廊周围自动生成墙壁，外部区域保持 Empty（不渲染）
// ============================================================================
void DungeonGenerator::generateWalls(Dungeon& dungeon) {
    // 收集需要设为 Wall 的位置（避免在遍历时修改）
    std::vector<sf::Vector2i> wallPositions;

    for (int y = 0; y < dungeon.height; ++y) {
        for (int x = 0; x < dungeon.width; ++x) {
            if (dungeon.GetTile(x, y) != TileType::Empty) continue;

            // 检查 8 个邻居是否有 Floor/Door/Stairs/Chest
            bool adjacentToFloor = false;
            for (int dy = -1; dy <= 1 && !adjacentToFloor; ++dy) {
                for (int dx = -1; dx <= 1 && !adjacentToFloor; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    TileType t = dungeon.GetTile(x + dx, y + dy);
                    if (t == TileType::Floor || t == TileType::Door ||
                        t == TileType::Stairs || t == TileType::Chest) {
                        adjacentToFloor = true;
                    }
                }
            }

            if (adjacentToFloor) {
                wallPositions.emplace_back(x, y);
            }
        }
    }

    // 设置墙壁
    for (const auto& pos : wallPositions) {
        dungeon.SetTile(pos.x, pos.y, TileType::Wall);
    }

    LOG_INFO("生成 %zu 个墙壁 tile", wallPositions.size());
}

// ============================================================================
// placeDoors —— 放置门
// ----------------------------------------------------------------------------
// 只在房间入口处放置门（一侧在房间内，另一侧在走廊）
// 避免在走廊中间或房间内部放置过多门
// ============================================================================
void DungeonGenerator::placeDoors(Dungeon& dungeon) {
    int doorCount = 0;

    // 辅助：检查 tile 是否在某个房间内
    auto isInRoom = [&dungeon](int tx, int ty) -> bool {
        for (const auto& room : dungeon.rooms) {
            if (tx >= room.bounds.left && tx < room.bounds.left + room.bounds.width &&
                ty >= room.bounds.top && ty < room.bounds.top + room.bounds.height) {
                return true;
            }
        }
        return false;
    };

    for (int y = 1; y < dungeon.height - 1; ++y) {
        for (int x = 1; x < dungeon.width - 1; ++x) {
            if (dungeon.GetTile(x, y) != TileType::Floor) continue;

            // 检查是否为水平门（左右是 Wall，上下是 Floor）
            bool leftWall = dungeon.GetTile(x - 1, y) == TileType::Wall;
            bool rightWall = dungeon.GetTile(x + 1, y) == TileType::Wall;
            bool upFloor = dungeon.GetTile(x, y - 1) == TileType::Floor;
            bool downFloor = dungeon.GetTile(x, y + 1) == TileType::Floor;

            // 检查是否为垂直门（上下是 Wall，左右是 Floor）
            bool upWall = dungeon.GetTile(x, y - 1) == TileType::Wall;
            bool downWall = dungeon.GetTile(x, y + 1) == TileType::Wall;
            bool leftFloor = dungeon.GetTile(x - 1, y) == TileType::Floor;
            bool rightFloor = dungeon.GetTile(x + 1, y) == TileType::Floor;

            bool isHorizontalDoor = leftWall && rightWall && upFloor && downFloor;
            bool isVerticalDoor = upWall && downWall && leftFloor && rightFloor;

            if (!isHorizontalDoor && !isVerticalDoor) continue;

            // 门本身不能在房间内
            if (isInRoom(x, y)) continue;

            // 关键改进：只在房间入口处放门
            // 水平门：上下两侧必须一侧在房间内、另一侧在走廊
            // 垂直门：左右两侧必须一侧在房间内、另一侧在走廊
            bool isRoomEntrance = false;
            if (isHorizontalDoor) {
                bool upInRoom = isInRoom(x, y - 1);
                bool downInRoom = isInRoom(x, y + 1);
                isRoomEntrance = (upInRoom && !downInRoom) || (!upInRoom && downInRoom);
            } else {
                bool leftInRoom = isInRoom(x - 1, y);
                bool rightInRoom = isInRoom(x + 1, y);
                isRoomEntrance = (leftInRoom && !rightInRoom) || (!leftInRoom && rightInRoom);
            }

            if (!isRoomEntrance) continue;

            dungeon.SetTile(x, y, TileType::Door);
            // 初始化门状态：默认关闭（阻挡子弹）
            // 怪物遇到关闭的门会自动开门通过（EnemyAI 中处理）
            dungeon.doorStates[y * dungeon.width + x] = DoorState{false, false, 30.f, 30.f};
            ++doorCount;

            // 将门添加到相邻房间
            for (auto& room : dungeon.rooms) {
                sf::IntRect expanded = room.bounds;
                if (isHorizontalDoor) {
                    if (x >= expanded.left && x < expanded.left + expanded.width) {
                        if (std::abs(y - expanded.top) <= 1 ||
                            std::abs(y - (expanded.top + expanded.height - 1)) <= 1) {
                            room.doors.emplace_back(x, y);
                        }
                    }
                } else {
                    if (y >= expanded.top && y < expanded.top + expanded.height) {
                        if (std::abs(x - expanded.left) <= 1 ||
                            std::abs(x - (expanded.left + expanded.width - 1)) <= 1) {
                            room.doors.emplace_back(x, y);
                        }
                    }
                }
            }
        }
    }
    LOG_INFO("放置 %d 个门", doorCount);
}

// ============================================================================
// assignRoomTypes —— 分配房间类型
// ----------------------------------------------------------------------------
//   - 起点房间：第一个房间（index 0）
//   - Boss 房间：离起点最远的房间（欧氏距离）
//   - 楼梯房间：Boss 房间（楼梯在 Boss 房内）
//   - 宝箱房：1-2 个随机非起点非 Boss 房间
//   - 精英房：1-2 个随机非起点非 Boss 房间
//   - 隐藏房：1 个随机小房间
//   - 其余：普通房间
// ============================================================================
void DungeonGenerator::assignRoomTypes(Dungeon& dungeon) {
    if (dungeon.rooms.empty()) return;

    // 起点房间 = 第一个房间，类型为出生房
    dungeon.startRoom = 0;
    dungeon.rooms[0].type = RoomType::Spawn;

    // Boss 房间 = 离起点最远的房间
    const Room& startRoom = dungeon.rooms[0];
    sf::Vector2i startPos = startRoom.center;
    int maxDist = -1;
    int bossIdx = -1;
    for (int i = 1; i < static_cast<int>(dungeon.rooms.size()); ++i) {
        int dx = dungeon.rooms[i].center.x - startPos.x;
        int dy = dungeon.rooms[i].center.y - startPos.y;
        int dist = dx * dx + dy * dy;
        if (dist > maxDist) {
            maxDist = dist;
            bossIdx = i;
        }
    }
    if (bossIdx >= 0) {
        dungeon.bossRoom = bossIdx;
        dungeon.rooms[bossIdx].type = RoomType::Boss;
        dungeon.stairsRoom = bossIdx; // 楼梯也在 Boss 房
    }

    const int totalRooms = static_cast<int>(dungeon.rooms.size());

    // 宝箱房：1 个随机房间（生成概率较低）
    int treasureCount = (totalRooms >= 4 && randomChance(0.6f)) ? 1 : 0;
    for (int i = 0; i < treasureCount; ++i) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            int idx = randomInt(1, totalRooms - 1);
            if (idx == bossIdx) continue;
            if (dungeon.rooms[idx].type == RoomType::Normal) {
                dungeon.rooms[idx].type = RoomType::Treasure;
                break;
            }
        }
    }

    // 陷阱房：0-1 个随机房间（低概率）
    int trapCount = (totalRooms >= 5 && randomChance(0.35f)) ? 1 : 0;
    for (int i = 0; i < trapCount; ++i) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            int idx = randomInt(1, totalRooms - 1);
            if (idx == bossIdx) continue;
            if (dungeon.rooms[idx].type == RoomType::Normal) {
                dungeon.rooms[idx].type = RoomType::Trap;
                break;
            }
        }
    }

    // 阻碍房：1-2 个随机房间
    int obstacleCount = std::min(2, totalRooms / 5);
    for (int i = 0; i < obstacleCount; ++i) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            int idx = randomInt(1, totalRooms - 1);
            if (idx == bossIdx) continue;
            if (dungeon.rooms[idx].type == RoomType::Normal) {
                dungeon.rooms[idx].type = RoomType::Obstacle;
                break;
            }
        }
    }

    // 精英房：1-2 个随机房间
    int eliteCount = std::min(2, totalRooms / 5);
    for (int i = 0; i < eliteCount; ++i) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            int idx = randomInt(1, totalRooms - 1);
            if (idx == bossIdx) continue;
            if (dungeon.rooms[idx].type == RoomType::Normal) {
                dungeon.rooms[idx].type = RoomType::Elite;
                break;
            }
        }
    }

    // 事件房：1 个随机房间（5 种事件类型随机选 1）
    // totalRooms >= 4 时 50% 概率生成
    if (totalRooms >= 4 && randomChance(0.5f)) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            int idx = randomInt(1, totalRooms - 1);
            if (idx == bossIdx) continue;
            if (dungeon.rooms[idx].type == RoomType::Normal) {
                dungeon.rooms[idx].type = RoomType::Event;
                // 随机选择事件类型（1-5）
                int evtType = randomInt(1, 5);
                dungeon.rooms[idx].eventType = static_cast<EventType>(evtType);
                LOG_INFO("房间 %d 设为事件房 (事件类型=%s)",
                         idx, EventTypeName(dungeon.rooms[idx].eventType));
                break;
            }
        }
    }

    // 诅咒房：1 个随机房间（高级奖励但进入后被诅咒）
    // totalRooms >= 5 时 30% 概率生成
    if (totalRooms >= 5 && randomChance(0.3f)) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            int idx = randomInt(1, totalRooms - 1);
            if (idx == bossIdx) continue;
            if (dungeon.rooms[idx].type == RoomType::Normal) {
                dungeon.rooms[idx].type = RoomType::Cursed;
                LOG_INFO("房间 %d 设为诅咒房", idx);
                break;
            }
        }
    }

    // 起点房间标记为已清理（无需战斗）
    dungeon.rooms[0].cleared = true;
}

// ============================================================================
// placeStairs —— 放置楼梯
// ============================================================================
void DungeonGenerator::placeStairs(Dungeon& dungeon) {
    if (dungeon.bossRoom < 0) return;
    const Room& boss = dungeon.rooms[dungeon.bossRoom];
    // 楼梯放在 Boss 房间中心
    dungeon.stairsPos = boss.center;
    dungeon.SetTile(boss.center.x, boss.center.y, TileType::Stairs);
    LOG_INFO("楼梯放置在 (%d, %d)", boss.center.x, boss.center.y);
}

// ============================================================================
// placeChests —— 放置宝箱
// ----------------------------------------------------------------------------
// 宝箱房：固定 3 个宝箱
// 陷阱房：随机 1-4 个宝箱
// ============================================================================
void DungeonGenerator::placeChests(Dungeon& dungeon) {
    int chestCount = 0;
    for (const auto& room : dungeon.rooms) {
        int targetChests = 0;
        if (room.type == RoomType::Treasure) {
            targetChests = 3;
        } else if (room.type == RoomType::Trap) {
            targetChests = randomInt(1, 4);
        } else if (room.type == RoomType::Cursed) {
            // 诅咒房：1 个高级宝箱（清理后才能安全开启）
            targetChests = 1;
        } else if (room.type == RoomType::Event) {
            // 事件房：仅在 ChestMimic 事件时放置假宝箱作为诱饵
            if (room.eventType == EventType::ChestMimic) {
                targetChests = 1;
            } else {
                continue;
            }
        } else {
            continue;
        }

        for (int i = 0; i < targetChests; ++i) {
            // 在房间中心附近偏移放置，避免重叠
            sf::Vector2i chestPos(
                room.center.x + randomInt(-2, 2),
                room.center.y + randomInt(-2, 2)
            );
            if (dungeon.GetTile(chestPos.x, chestPos.y) == TileType::Floor) {
                dungeon.SetTile(chestPos.x, chestPos.y, TileType::Chest);
                ++chestCount;
            }
        }
    }
    LOG_INFO("放置 %d 个宝箱", chestCount);
}

// ============================================================================
// placeObstacles —— 放置障碍物（木墙装饰 + 阻碍房混合障碍）
// ----------------------------------------------------------------------------
// 普通房间：3-5 个可破坏木墙（边缘和内部随机分布）
// 阻碍房：8-12 个障碍物，混合可破坏木墙和不可破坏石柱
// 放置规则：
//   - 确保放置后仍有通路（不卡住怪物）
//   - 不与已有的 Door/Chest/Stairs/Obstacle 重叠
//   - 可破坏物（木墙）占多数，提供战斗互动
// ============================================================================
void DungeonGenerator::placeObstacles(Dungeon& dungeon) {
    int obstacleCount = 0;
    int indestructibleCount = 0;

    for (const auto& room : dungeon.rooms) {
        if (room.type == RoomType::Boss || room.type == RoomType::Stairs) continue;

        // 计算房间面积，用于确定障碍密度
        int area = room.bounds.width * room.bounds.height;
        
        // 普通房间：5-8 个可破坏木墙
        int baseCount = 5 + randomInt(0, 3);
        if (room.type == RoomType::Obstacle) {
            // 阻碍房：15-22 个障碍物（密度更高）
            baseCount = 15 + randomInt(0, 7);
            // 大面积房间可以更多
            if (area > 100) baseCount += 6;
        } else if (area > 80) {
            // 大房间多放一些
            baseCount += 3;
        }

        // 记录尝试放置的位置，用于路径检查
        std::vector<sf::Vector2i> placedObstacles;
        
        for (int i = 0; i < baseCount; ++i) {
            sf::Vector2i pos;
            bool isIndestructible = false;

            if (room.type == RoomType::Obstacle) {
                // 阻碍房：30% 不可破坏石柱，70% 可破坏木墙
                isIndestructible = randomChance(0.3f);
            } else {
                // 普通房间：全部可破坏木墙
                isIndestructible = false;
            }

            // 尝试找到合适的位置（最多尝试 10 次）
            bool foundValidPos = false;
            for (int attempt = 0; attempt < 10; ++attempt) {
                if (isIndestructible) {
                    // 不可破坏障碍物：放在房间中心区域（距离边界 >= 2 tile）
                    int margin = 2;
                    if (room.bounds.width <= margin * 2 + 2 ||
                        room.bounds.height <= margin * 2 + 2) {
                        // 房间太小，跳过不可破坏障碍
                        break;
                    }
                    pos = sf::Vector2i(
                        room.bounds.left + margin +
                            randomInt(0, room.bounds.width - 2 * margin - 1),
                        room.bounds.top + margin +
                            randomInt(0, room.bounds.height - 2 * margin - 1)
                    );
                } else {
                    // 可破坏木墙：随机分布在房间内（距离边界 >= 1 tile）
                    pos = sf::Vector2i(
                        room.bounds.left + 1 + randomInt(0, room.bounds.width - 2),
                        room.bounds.top + 1 + randomInt(0, room.bounds.height - 2)
                    );
                }

                // 检查位置是否有效
                TileType existing = dungeon.GetTile(pos.x, pos.y);
                if (existing != TileType::Floor) continue;

                // 检查是否与已放置的障碍相邻（避免形成墙壁）
                bool adjacentToObstacle = false;
                for (const auto& placed : placedObstacles) {
                    int dx = std::abs(placed.x - pos.x);
                    int dy = std::abs(placed.y - pos.y);
                    // 如果正交相邻（上下左右），则跳过；允许对角相邻
                    if (dx + dy == 1) {
                        adjacentToObstacle = true;
                        break;
                    }
                }
                if (adjacentToObstacle) continue;

                // 检查是否会阻断主要通道（简化检查：确保周围至少有 2 个方向可通行）
                int passableNeighbors = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = pos.x + dx;
                        int ny = pos.y + dy;
                        TileType nt = dungeon.GetTile(nx, ny);
                        if (nt == TileType::Floor || nt == TileType::Door) {
                            ++passableNeighbors;
                        }
                    }
                }
                // 至少需要 2 个可通行邻居（放宽要求，允许更多障碍物放置）
                if (passableNeighbors < 2) continue;

                foundValidPos = true;
                break;
            }

            if (!foundValidPos) continue;

            // 放置障碍物
            dungeon.SetTile(pos.x, pos.y,
                            isIndestructible ? TileType::IndestructibleObstacle
                                             : TileType::Obstacle);
            placedObstacles.push_back(pos);
            ++obstacleCount;
            if (isIndestructible) ++indestructibleCount;
        }
    }
    LOG_INFO("放置 %d 个障碍物（其中不可破坏 %d 个，可破坏木墙 %d 个）", 
             obstacleCount, indestructibleCount, obstacleCount - indestructibleCount);
}

// ============================================================================
// GetSpawnPosition —— 获取房间内随机位置（世界坐标）
// ============================================================================
sf::Vector2f DungeonGenerator::GetSpawnPosition(const Dungeon& dungeon, const Room& room) {
    // 在房间内随机选一个 Floor tile
    // 简化：用房间中心 + 随机偏移
    sf::Vector2i tilePos(
        room.bounds.left + 1 + (std::rand() % std::max(1, room.bounds.width - 2)),
        room.bounds.top + 1 + (std::rand() % std::max(1, room.bounds.height - 2))
    );
    return dungeon.TileCenterToWorld(tilePos);
}

// ============================================================================
// IsWalkable —— 检查 tile 是否可通行
// ============================================================================
bool DungeonGenerator::IsWalkable(const Dungeon& dungeon, int tileX, int tileY) noexcept {
    TileType t = dungeon.GetTile(tileX, tileY);
    // Floor、Door（已打开）、Stairs 可通行
    // Wall、Obstacle、Chest 不可通行
    // Door 默认可通行（清理后打开；未清理时由 RoomSystem 控制）
    return t == TileType::Floor || t == TileType::Door || t == TileType::Stairs;
}

// ============================================================================
// IsWalkableWorld —— 检查世界坐标处是否可通行
// ============================================================================
bool DungeonGenerator::IsWalkableWorld(const Dungeon& dungeon, sf::Vector2f worldPos) noexcept {
    sf::Vector2i tile = dungeon.WorldToTile(worldPos);
    return IsWalkable(dungeon, tile.x, tile.y);
}

// ============================================================================
// GetTileAt —— 获取世界坐标处的 tile 类型
// ============================================================================
TileType DungeonGenerator::GetTileAt(const Dungeon& dungeon, sf::Vector2f worldPos) noexcept {
    sf::Vector2i tile = dungeon.WorldToTile(worldPos);
    return dungeon.GetTile(tile.x, tile.y);
}

// ============================================================================
// 随机数辅助函数
// ============================================================================
int DungeonGenerator::randomInt(int min, int max) noexcept {
    if (min > max) std::swap(min, max);
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng_);
}

float DungeonGenerator::randomFloat(float min, float max) noexcept {
    if (min > max) std::swap(min, max);
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng_);
}

bool DungeonGenerator::randomChance(float probability) noexcept {
    std::uniform_real_distribution<float> dist(0.f, 1.f);
    return dist(rng_) < probability;
}

} // namespace cu
