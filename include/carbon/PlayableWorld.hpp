#pragma once

// A deliberately limited, source-traced Java Edition 26.3 creative mode slice.
#include "carbon/AABB.hpp"
#include "carbon/Direction.hpp"
#include "carbon/Vec3.hpp"

#include <optional>
#include <memory>
#include <string_view>
#include <vector>

namespace carbon {

enum class BlockId { Air, Bedrock, Dirt, GrassBlock, Stone, Water, Lava, StoneSlab, StoneStairs, OakFence };
enum class SlabType { Top, Bottom, Double };
enum class StairHalf { Bottom, Top };
enum class StairsShape { Straight, InnerLeft, InnerRight, OuterLeft, OuterRight };
struct FenceConnections {
    bool north{};
    bool east{};
    bool south{};
    bool west{};
    friend bool operator==(const FenceConnections&, const FenceConnections&) = default;
};
struct BlockInstance {
    BlockPos pos;
    BlockId block{BlockId::Air};
    SlabType slabType{SlabType::Bottom};
    Direction stairFacing{Direction::North};
    StairHalf stairHalf{StairHalf::Bottom};
    StairsShape stairShape{StairsShape::Straight};
    FenceConnections fenceConnections{};
    std::string_view generatedSourceBlockId{};
};
struct PlayerState {
    Vec3 position{0.5, -60.0, 0.5}; // feet position, at the CLASSIC_FLAT spawn height
    float yaw{};                  // degrees; zero faces south (+Z)
    float pitch{};                // degrees; positive looks down
    bool flying{};
    bool onGround{true};
    bool sprinting{};
    bool crouching{};
    double eyeHeight{1.62};
};
struct MoveInput {
    float forward{}; // signed movement axes; diagonal input is normalized
    float strafe{};
    bool jump{};
    bool jumpPressed{};
    bool descend{};
    bool sprint{};
};
struct BlockHit { BlockPos block; Direction face{Direction::Up}; Vec3 location; double distance{}; };

class PlayableWorld {
public:
    static constexpr double EYE_HEIGHT = 1.62;
    static constexpr double PLAYER_WIDTH = 0.6;
    static constexpr double PLAYER_HEIGHT = 1.8;
    static constexpr double BLOCK_REACH = 5.0; // creative: base 4.5 + ServerPlayer creative modifier 0.5

    PlayableWorld();
    static PlayableWorld createDefaultFlat();
    static PlayableWorld createSeededOverworld(std::shared_ptr<const class SeededOverworldChunkProvider> terrain);
    bool hasSeededTerrain() const noexcept { return static_cast<bool>(seededTerrain_); }
    std::shared_ptr<const class SeededOverworldChunkProvider> seededTerrainProvider() const noexcept { return seededTerrain_; }

    PlayerState player;
    BlockId getBlock(const BlockPos& pos) const;
    SlabType getSlabType(const BlockPos& pos) const;
    Direction getStairFacing(const BlockPos& pos) const;
    StairHalf getStairHalf(const BlockPos& pos) const;
    StairsShape getStairShape(const BlockPos& pos) const;
    FenceConnections getFenceConnections(const BlockPos& pos) const;
    std::uint64_t sceneRevision() const noexcept { return sceneRevision_; }
    bool setBlock(const BlockPos& pos, BlockId block);
    bool setSlab(const BlockPos& pos, SlabType type);
    bool setStairs(const BlockPos& pos, Direction facing, StairHalf half,
                   StairsShape shape = StairsShape::Straight);
    bool setFence(const BlockPos& pos, FenceConnections connections = {});
    std::vector<BlockInstance> blocksInRange(const BlockPos& center, int radius) const;
    std::vector<BlockInstance> blocksInSectionWindow(const BlockPos& center,
                                                      int horizontalSectionRadius,
                                                      int verticalRadius) const;

    // One invocation is one Java simulation tick (20 ticks/second).
    void tickCreativeMove(const MoveInput& input); // legacy name; ticks grounded/creative movement
    Vec3 eyePosition() const;
    Vec3 lookDirection() const;
    std::optional<BlockHit> raycast(const Vec3& origin, const Vec3& direction, double reach = 5.0) const;
    std::optional<BlockHit> lookAtBlock(double reach = 5.0) const;
    bool breakTarget();
    bool placeTarget(BlockId selected);

private:
    std::vector<BlockInstance> blocks_;
    std::shared_ptr<const class SeededOverworldChunkProvider> seededTerrain_;
    Vec3 velocity_{};
    bool previousJump_{};
    int flightTapTicks_{};
    std::uint64_t sceneRevision_{};
    bool horizontalCollision_{};
    bool stepping_{};
    void moveWithBlockCollision(const Vec3& delta);
    void updateStairsAround(const BlockPos& pos);
    void updateFencesAround(const BlockPos& pos);
};

} // namespace carbon
