#include "carbon/PlayableWorld.hpp"
#include "carbon/CameraBasis.hpp"
#include "carbon/FenceBlock.hpp"
#include "carbon/SlabBlock.hpp"
#include "carbon/StairsBlock.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace carbon {
namespace {
int floorBlock(double v) { return static_cast<int>(std::floor(v)); }
bool solid(BlockId id) { return id != BlockId::Air; }
VoxelShape outlineBlockShape(BlockId id, SlabType type, Direction facing = Direction::North,
                              StairHalf half = StairHalf::Bottom, StairsShape shape = StairsShape::Straight,
                              FenceConnections fence = {}) {
    if (id == BlockId::StoneSlab) return slabVoxelShape(type);
    if (id == BlockId::StoneStairs) return stairsVoxelShape(facing, half, shape);
    if (id == BlockId::OakFence) return fenceOutlineShape(fence);
    return Shapes::block();
}
VoxelShape collisionBlockShape(BlockId id, SlabType type, Direction facing = Direction::North,
                               StairHalf half = StairHalf::Bottom, StairsShape shape = StairsShape::Straight,
                               FenceConnections fence = {}) {
    if (id == BlockId::OakFence) return fenceCollisionShape(fence);
    return outlineBlockShape(id, type, facing, half, shape, fence);
}
Direction hitFace(const AABB& box, const Vec3& p) {
    constexpr double e = 1.0e-6;
    if (std::abs(p.y-box.minY)<e) return Direction::Down;
    if (std::abs(p.y-box.maxY)<e) return Direction::Up;
    if (std::abs(p.z-box.minZ)<e) return Direction::North;
    if (std::abs(p.z-box.maxZ)<e) return Direction::South;
    if (std::abs(p.x-box.minX)<e) return Direction::West;
    return Direction::East;
}
bool intersectsShape(const AABB& box, const VoxelShape& shape) {
    bool hit = false;
    shape.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
        if (box.intersects(AABB{x0,y0,z0,x1,y1,z1})) hit = true;
    });
    return hit;
}
bool fullSturdyFace(const VoxelShape& shape, Direction face) {
    const Axis normal = axis(face);
    const double edge = (face == Direction::East || face == Direction::Up || face == Direction::South) ? 1.0 : 0.0;
    std::vector<AABB> patches;
    std::vector<double> a{0.0,1.0}, b{0.0,1.0};
    shape.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
        const double n0 = normal == Axis::X ? x0 : normal == Axis::Y ? y0 : z0;
        const double n1 = normal == Axis::X ? x1 : normal == Axis::Y ? y1 : z1;
        if (std::abs((edge == 0.0 ? n0 : n1) - edge) > 1e-9) return;
        AABB p{x0,y0,z0,x1,y1,z1};
        if (normal == Axis::X) { a.push_back(y0); a.push_back(y1); b.push_back(z0); b.push_back(z1); }
        else if (normal == Axis::Y) { a.push_back(x0); a.push_back(x1); b.push_back(z0); b.push_back(z1); }
        else { a.push_back(x0); a.push_back(x1); b.push_back(y0); b.push_back(y1); }
        patches.push_back(p);
    });
    auto unique = [](std::vector<double>& v) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end(), [](double x,double y){return std::abs(x-y)<1e-9;}),v.end());
    };
    unique(a); unique(b);
    auto range = [&](const AABB& p) {
        if(normal == Axis::X) return std::array<double,4>{p.minY,p.maxY,p.minZ,p.maxZ};
        if(normal == Axis::Y) return std::array<double,4>{p.minX,p.maxX,p.minZ,p.maxZ};
        return std::array<double,4>{p.minX,p.maxX,p.minY,p.maxY};
    };
    for (std::size_t i=0;i+1<a.size();++i) for (std::size_t j=0;j+1<b.size();++j) {
        if (a[i+1]-a[i] < 1e-9 || b[j+1]-b[j] < 1e-9) continue;
        bool filled=false;
        for(const auto& patch:patches) { const auto r=range(patch); if(r[0]<=a[i]+1e-9&&r[1]>=a[i+1]-1e-9&&r[2]<=b[j]+1e-9&&r[3]>=b[j+1]-1e-9){filled=true;break;} }
        if(!filled) return false;
    }
    return !patches.empty();
}
}

PlayableWorld::PlayableWorld() = default;

PlayableWorld PlayableWorld::createDefaultFlat() {
    PlayableWorld world;
    // CLASSIC_FLAT layers are the unmodified base; blocks_ stores local edits.
    world.player = PlayerState{};
    return world;
}

BlockId PlayableWorld::getBlock(const BlockPos& pos) const {
    for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it) if (it->pos == pos) return it->block;
    if (pos.y == -64) return BlockId::Bedrock;
    if (pos.y == -63 || pos.y == -62) return BlockId::Dirt;
    if (pos.y == -61) return BlockId::GrassBlock;
    return BlockId::Air;
}

SlabType PlayableWorld::getSlabType(const BlockPos& pos) const {
    for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it)
        if (it->pos == pos) return it->slabType;
    return SlabType::Bottom;
}

Direction PlayableWorld::getStairFacing(const BlockPos& pos) const {
    for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it) if (it->pos == pos) return it->stairFacing;
    return Direction::North;
}
StairHalf PlayableWorld::getStairHalf(const BlockPos& pos) const {
    for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it) if (it->pos == pos) return it->stairHalf;
    return StairHalf::Bottom;
}
StairsShape PlayableWorld::getStairShape(const BlockPos& pos) const {
    for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it) if (it->pos == pos) return it->stairShape;
    return StairsShape::Straight;
}
FenceConnections PlayableWorld::getFenceConnections(const BlockPos& pos) const {
    for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it)
        if (it->pos == pos) return it->block == BlockId::OakFence ? it->fenceConnections : FenceConnections{};
    return {};
}

void PlayableWorld::updateStairsAround(const BlockPos& pos) {
    auto refresh = [&](const BlockPos& target) {
        if (getBlock(target) != BlockId::StoneStairs) return;
        const auto shape = stairsShapeForPlacement(*this, target, getStairFacing(target), getStairHalf(target));
        auto it = std::find_if(blocks_.begin(), blocks_.end(), [&](const auto& b) { return b.pos == target; });
        if (it != blocks_.end()) it->stairShape = shape;
    };
    refresh(pos);
    for (const auto direction : DirectionValues) {
        const auto n = step(direction);
        refresh({pos.x+n.x,pos.y+n.y,pos.z+n.z});
    }
}

void PlayableWorld::updateFencesAround(const BlockPos& pos) {
    auto refresh = [&](const BlockPos& target) {
        if (getBlock(target) != BlockId::OakFence) return;
        auto connects = [&](Direction d, BlockPos n) {
            const BlockId neighbor = getBlock(n);
            if (neighbor == BlockId::OakFence) return true; // same wooden fence tag
            if (neighbor == BlockId::Air) return false;
            const auto sturdy = fullSturdyFace(collisionBlockShape(neighbor, getSlabType(n),
                getStairFacing(n), getStairHalf(n), getStairShape(n), getFenceConnections(n)), opposite(d));
            // The supported block subset has no exception-tagged blocks.
            return sturdy;
        };
        FenceConnections c;
        auto n = step(Direction::North); c.north = connects(Direction::North,{target.x+n.x,target.y+n.y,target.z+n.z});
        n = step(Direction::East); c.east = connects(Direction::East,{target.x+n.x,target.y+n.y,target.z+n.z});
        n = step(Direction::South); c.south = connects(Direction::South,{target.x+n.x,target.y+n.y,target.z+n.z});
        n = step(Direction::West); c.west = connects(Direction::West,{target.x+n.x,target.y+n.y,target.z+n.z});
        auto it = std::find_if(blocks_.begin(), blocks_.end(), [&](const auto& b) { return b.pos == target; });
        if (it != blocks_.end()) it->fenceConnections = c;
    };
    refresh(pos);
    for (Direction d : {Direction::North,Direction::East,Direction::South,Direction::West}) {
        const auto n = step(d); refresh({pos.x+n.x,pos.y+n.y,pos.z+n.z});
    }
}

bool PlayableWorld::setBlock(const BlockPos& pos, BlockId block) {
    auto it = std::find_if(blocks_.begin(), blocks_.end(), [&](const auto& b) { return b.pos == pos; });
    if (it == blocks_.end()) blocks_.push_back({pos, block, SlabType::Bottom});
    else {
        it->block = block;
        it->slabType = SlabType::Bottom;
        it->stairFacing = Direction::North;
        it->stairHalf = StairHalf::Bottom;
        it->stairShape = StairsShape::Straight;
    }
    updateStairsAround(pos);
    updateFencesAround(pos);
    ++sceneRevision_;
    return true;
}

bool PlayableWorld::setSlab(const BlockPos& pos, SlabType type) {
    auto it = std::find_if(blocks_.begin(), blocks_.end(), [&](const auto& b) { return b.pos == pos; });
    if (it == blocks_.end()) blocks_.push_back({pos, BlockId::StoneSlab, type});
    else { it->block = BlockId::StoneSlab; it->slabType = type; }
    updateStairsAround(pos);
    updateFencesAround(pos);
    ++sceneRevision_;
    return true;
}

bool PlayableWorld::setStairs(const BlockPos& pos, Direction facing, StairHalf half, StairsShape shape) {
    auto it = std::find_if(blocks_.begin(), blocks_.end(), [&](const auto& b) { return b.pos == pos; });
    if (it == blocks_.end()) blocks_.push_back({pos, BlockId::StoneStairs, SlabType::Bottom, facing, half, shape});
    else { it->block = BlockId::StoneStairs; it->stairFacing = facing; it->stairHalf = half; it->stairShape = shape; }
    updateStairsAround(pos);
    updateFencesAround(pos);
    ++sceneRevision_;
    return true;
}

bool PlayableWorld::setFence(const BlockPos& pos, FenceConnections connections) {
    auto it = std::find_if(blocks_.begin(), blocks_.end(), [&](const auto& b) { return b.pos == pos; });
    if (it == blocks_.end()) blocks_.push_back({pos, BlockId::OakFence, SlabType::Bottom, Direction::North,
        StairHalf::Bottom, StairsShape::Straight, connections});
    else { it->block = BlockId::OakFence; it->fenceConnections = connections; }
    updateStairsAround(pos);
    updateFencesAround(pos);
    ++sceneRevision_;
    return true;
}

std::vector<BlockInstance> PlayableWorld::blocksInRange(const BlockPos& center, int radius) const {
    std::vector<BlockInstance> result;
    const auto r = std::max(radius, 0);
    std::unordered_map<std::int64_t,const BlockInstance*> overrides;
    overrides.reserve(blocks_.size());
    for (const auto& block:blocks_) overrides.insert_or_assign(block.pos.asLong(),&block);
    for (int x=center.x-r; x<=center.x+r; ++x) for (int z=center.z-r; z<=center.z+r; ++z)
        for (int y=std::max(center.y-r,-64); y<=std::min(center.y+r,-61); ++y) {
        BlockPos p{x,y,z};
        const auto found=overrides.find(p.asLong());
        if (found!=overrides.end()) {
            if (found->second->block!=BlockId::Air) result.push_back(*found->second);
        } else {
            const BlockId block=y==-64?BlockId::Bedrock:y==-61?BlockId::GrassBlock:BlockId::Dirt;
            result.push_back({p,block});
        }
        }
    for (const auto& block : blocks_) if (block.block != BlockId::Air && (block.pos.y < -64 || block.pos.y > -61) &&
        std::abs(block.pos.x-center.x)<=r && std::abs(block.pos.y-center.y)<=r && std::abs(block.pos.z-center.z)<=r)
        result.push_back(block);
    return result;
}

std::vector<BlockInstance> PlayableWorld::blocksInSectionWindow(const BlockPos& center,
                                                                 int horizontalSectionRadius,
                                                                 int verticalRadius) const {
    const int sectionRadius=std::max(horizontalSectionRadius,0);
    const int vertical=std::max(verticalRadius,0);
    const auto section=[](int value) { return value>=0?value/16:-((15-value)/16); };
    const int sx=section(center.x),sz=section(center.z);
    const int minX=(sx-sectionRadius)*16,maxX=(sx+sectionRadius+1)*16-1;
    const int minZ=(sz-sectionRadius)*16,maxZ=(sz+sectionRadius+1)*16-1;
    const int minY=center.y-vertical,maxY=center.y+vertical;
    std::vector<BlockInstance> result;
    std::unordered_map<std::int64_t,const BlockInstance*> overrides;
    overrides.reserve(blocks_.size());
    for (const auto& block:blocks_) overrides.insert_or_assign(block.pos.asLong(),&block);
    for (int x=minX;x<=maxX;++x) for (int z=minZ;z<=maxZ;++z)
        for (int y=std::max(minY,-64);y<=std::min(maxY,-61);++y) {
            const BlockPos pos{x,y,z};
            const auto found=overrides.find(pos.asLong());
            if (found!=overrides.end()) {
                if (found->second->block!=BlockId::Air) result.push_back(*found->second);
            } else {
                const BlockId block=y==-64?BlockId::Bedrock:y==-61?BlockId::GrassBlock:BlockId::Dirt;
                result.push_back({pos,block});
            }
        }
    for (const auto& block:blocks_) if (block.block!=BlockId::Air &&
        (block.pos.y < -64 || block.pos.y > -61) && block.pos.x>=minX && block.pos.x<=maxX &&
        block.pos.z>=minZ && block.pos.z<=maxZ && block.pos.y>=minY && block.pos.y<=maxY)
        result.push_back(block);
    return result;
}

Vec3 PlayableWorld::eyePosition() const { return player.position.add(0.0, player.eyeHeight, 0.0); }

Vec3 PlayableWorld::lookDirection() const {
    return cameraBasis(player.yaw, player.pitch).forward.normalize();
}

std::optional<BlockHit> PlayableWorld::raycast(const Vec3& origin, const Vec3& direction, double reach) const {
    if (!(reach > 0.0) || direction.lengthSqr() == 0.0) return std::nullopt;
    const Vec3 end = origin.add(direction.normalize().scale(reach));
    const int minX = floorBlock(std::min(origin.x, end.x)) - 1, maxX = floorBlock(std::max(origin.x, end.x)) + 1;
    const int minY = floorBlock(std::min(origin.y, end.y)) - 1, maxY = floorBlock(std::max(origin.y, end.y)) + 1;
    const int minZ = floorBlock(std::min(origin.z, end.z)) - 1, maxZ = floorBlock(std::max(origin.z, end.z)) + 1;
    std::optional<BlockHit> best;
    for (int x = minX; x <= maxX; ++x) for (int y = minY; y <= maxY; ++y) for (int z = minZ; z <= maxZ; ++z) {
        BlockPos pos{x,y,z};
        const auto block = getBlock(pos);
        if (!solid(block)) continue;
        const auto shape = outlineBlockShape(block, getSlabType(pos), getStairFacing(pos), getStairHalf(pos), getStairShape(pos), getFenceConnections(pos)).move(pos.x,pos.y,pos.z);
        shape.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
            const AABB box{x0,y0,z0,x1,y1,z1};
            const auto point = box.clip(origin,end);
            if (!point) return;
            const double distance = origin.distanceTo(*point);
            if (!best || distance < best->distance) best = BlockHit{pos,hitFace(box,*point),*point,distance};
        });
    }
    return best;
}

std::optional<BlockHit> PlayableWorld::lookAtBlock(double reach) const { return raycast(eyePosition(), lookDirection(), reach); }

void PlayableWorld::moveWithBlockCollision(const Vec3& delta) {
    const Vec3 start = player.position;
    const bool startedGrounded = player.onGround;
    Vec3 adjusted = delta;
    const double half = PLAYER_WIDTH / 2.0;
    auto supportedAt = [&](double x, double z) {
        const AABB foot{x-half, player.position.y-0.04, z-half,
                        x+half, player.position.y, z+half};
        const int sx=floorBlock(foot.minX), ex=floorBlock(foot.maxX);
        const int sy=floorBlock(foot.minY), ey=floorBlock(foot.maxY);
        const int sz=floorBlock(foot.minZ), ez=floorBlock(foot.maxZ);
        for(int bx=sx;bx<=ex;++bx) for(int by=sy;by<=ey;++by) for(int bz=sz;bz<=ez;++bz) {
            const BlockPos p{bx,by,bz}; const auto block=getBlock(p);
            if(solid(block)&&intersectsShape(foot,collisionBlockShape(block,getSlabType(p),getStairFacing(p),
                getStairHalf(p),getStairShape(p),getFenceConnections(p)).move(bx,by,bz))) return true;
        }
        return false;
    };
    if(player.crouching&&player.onGround) {
        while(std::abs(adjusted.x)>1e-9&&!supportedAt(player.position.x+adjusted.x,player.position.z))
            adjusted.x=std::abs(adjusted.x)<0.05?0.0:adjusted.x-std::copysign(0.05,adjusted.x);
        while(std::abs(adjusted.z)>1e-9&&!supportedAt(player.position.x+adjusted.x,player.position.z+adjusted.z))
            adjusted.z=std::abs(adjusted.z)<0.05?0.0:adjusted.z-std::copysign(0.05,adjusted.z);
    }
    AABB box{player.position.x-half, player.position.y, player.position.z-half,
             player.position.x+half, player.position.y+PLAYER_HEIGHT, player.position.z+half};
    const int minX = floorBlock(std::min(box.minX, box.minX + adjusted.x)) - 1;
    const int maxX = floorBlock(std::max(box.maxX, box.maxX + adjusted.x)) + 1;
    const int minY = floorBlock(std::min(box.minY, box.minY + adjusted.y)) - 1;
    const int maxY = floorBlock(std::max(box.maxY, box.maxY + adjusted.y)) + 1;
    const int minZ = floorBlock(std::min(box.minZ, box.minZ + adjusted.z)) - 1;
    const int maxZ = floorBlock(std::max(box.maxZ, box.maxZ + adjusted.z)) + 1;
    auto resolve = [&](int axis, double amount) {
        for (int x=minX; x<=maxX; ++x) for (int y=minY; y<=maxY; ++y) for (int z=minZ; z<=maxZ; ++z) {
            BlockPos p{x,y,z}; const auto block = getBlock(p);
            if (solid(block)) amount = collisionBlockShape(block,getSlabType(p),getStairFacing(p),getStairHalf(p),getStairShape(p),getFenceConnections(p)).move(p.x,p.y,p.z).collide(
                static_cast<Axis>(axis), box, amount);
        }
        if (axis==0) { box=box.move(amount,0,0); player.position.x+=amount; }
        else if (axis==1) { box=box.move(0,amount,0); player.position.y+=amount; }
        else { box=box.move(0,0,amount); player.position.z+=amount; }
    };
    // Entity.collideBoundingBox uses YZX when |x| < |z|, else YXZ.
    resolve(1, adjusted.y);
    if (std::abs(adjusted.x) < std::abs(adjusted.z)) { resolve(2, adjusted.z); resolve(0, adjusted.x); }
    else { resolve(0, adjusted.x); resolve(2, adjusted.z); }
    const double movedX = player.position.x-start.x;
    const double movedY = player.position.y-start.y;
    const double movedZ = player.position.z-start.z;
    horizontalCollision_ = std::abs(movedX-adjusted.x)>1e-8 || std::abs(movedZ-adjusted.z)>1e-8;
    player.onGround = adjusted.y < 0.0 && std::abs(movedY-adjusted.y)>1e-8;
    if (!player.onGround && adjusted.y >= 0.0) {
        const AABB foot{player.position.x-half, player.position.y-0.01, player.position.z-half,
                        player.position.x+half, player.position.y, player.position.z+half};
        const int sx=floorBlock(foot.minX), ex=floorBlock(foot.maxX);
        const int sy=floorBlock(foot.minY), ey=floorBlock(foot.maxY);
        const int sz=floorBlock(foot.minZ), ez=floorBlock(foot.maxZ);
        for(int x=sx;x<=ex&&!player.onGround;++x) for(int y=sy;y<=ey&&!player.onGround;++y)
            for(int z=sz;z<=ez&&!player.onGround;++z) {
                const BlockPos p{x,y,z}; const auto block=getBlock(p);
                if(solid(block)) player.onGround=intersectsShape(foot,collisionBlockShape(block,getSlabType(p),
                    getStairFacing(p),getStairHalf(p),getStairShape(p),getFenceConnections(p)).move(x,y,z));
            }
    }
    const Vec3 ordinaryPosition = player.position;
    const bool ordinaryGround = player.onGround;
    const bool ordinaryHorizontalCollision = horizontalCollision_;
    if (ordinaryHorizontalCollision && startedGrounded && !stepping_ && !player.flying) {
        stepping_ = true;
        player.position = start;
        player.onGround = false;
        moveWithBlockCollision({adjusted.x, 0.6, adjusted.z});
        const Vec3 raisedPosition = player.position;
        if (!horizontalCollision_ && raisedPosition.y > start.y + 1e-6) {
            moveWithBlockCollision({0.0, start.y-raisedPosition.y, 0.0});
            const Vec3 steppedPosition = player.position;
            if (player.onGround && std::hypot(steppedPosition.x-start.x,steppedPosition.z-start.z)
                    > std::hypot(ordinaryPosition.x-start.x,ordinaryPosition.z-start.z)+1e-6) {
                stepping_ = false;
                return;
            }
        }
        player.position = ordinaryPosition;
        player.onGround = ordinaryGround;
        horizontalCollision_ = ordinaryHorizontalCollision;
        stepping_ = false;
    }
}

void PlayableWorld::tickCreativeMove(const MoveInput& input) {
    const bool jumpStarted = input.jumpPressed || (input.jump && !previousJump_);
    previousJump_ = input.jump;
    if (flightTapTicks_ > 0) --flightTapTicks_;
    if (jumpStarted) {
        if (flightTapTicks_ > 0) {
            player.flying = !player.flying;
            flightTapTicks_ = 0;
            if (player.flying && player.onGround) velocity_.y = std::max(velocity_.y, 0.42);
        } else {
            flightTapTicks_ = 7;
        }
    }
    player.crouching = input.descend && !player.flying;
    player.eyeHeight = player.crouching ? 1.27 : EYE_HEIGHT;
    player.sprinting = input.sprint && input.forward > 0.0F && !player.crouching;
    float f = input.forward, s = input.strafe;
    const float mag = std::sqrt(f*f+s*s);
    if (mag > 1.0F) { f/=mag; s/=mag; }
    const float yaw = player.yaw * static_cast<float>(3.14159265358979323846 / 180.0);
    const double forwardX = -std::sin(yaw), forwardZ = std::cos(yaw);
    const double rightX = -std::cos(yaw), rightZ = -std::sin(yaw);
    const double wishX = s*rightX + f*forwardX;
    const double wishZ = s*rightZ + f*forwardZ;
    if (player.flying) {
        const CameraBasis camera = cameraBasis(player.yaw, player.pitch);
        const double flyAcceleration = player.sprinting ? 0.1 : 0.05;
        velocity_.x += (s*camera.right.x + f*camera.forward.x) * flyAcceleration;
        velocity_.z += (s*camera.right.z + f*camera.forward.z) * flyAcceleration;
        if (input.jump) velocity_.y += 0.05 * 3.0;
        if (input.descend) velocity_.y -= 0.05 * 3.0;
        moveWithBlockCollision(velocity_);
        velocity_.x *= 0.91; velocity_.z *= 0.91; velocity_.y *= 0.6;
        return;
    }

    // Source values: Player's walking-speed ability defaults to 0.1, sprinting
    // adds 30%, and Attributes.JUMP_STRENGTH defaults to 0.42. LivingEntity uses
    // 0.08 gravity, 0.91 horizontal air drag, and 0.98 vertical air drag.
    if (jumpStarted && player.onGround) velocity_.y = 0.42;
    const double friction = player.onGround ? 0.6 : 0.91;
    double speed = 0.1 * (player.sprinting ? 1.3 : 1.0);
    // LivingEntity.getFrictionInfluencedSpeed applies this ground factor;
    // Player.getFlyingSpeed returns 0.02 (0.026 while sprinting) in ordinary air.
    const double acceleration = player.onGround ? speed * (0.21600002 / (friction*friction*friction))
                                                 : (player.sprinting ? 0.026 : 0.02);
    const double sneakScale = player.crouching ? 0.3 : 1.0;
    velocity_.x += wishX * acceleration * sneakScale;
    velocity_.z += wishZ * acceleration * sneakScale;
    moveWithBlockCollision(velocity_);
    if (horizontalCollision_) { velocity_.x = 0.0; velocity_.z = 0.0; }
    if (player.onGround) velocity_.y = 0.0;
    else velocity_.y = (velocity_.y - 0.08) * 0.98;
    velocity_.x *= friction; velocity_.z *= friction;
}

bool PlayableWorld::breakTarget() {
    const auto hit = lookAtBlock(BLOCK_REACH);
    if (!hit) return false;
    return setBlock(hit->block, BlockId::Air);
}

bool PlayableWorld::placeTarget(BlockId selected) {
    if (selected == BlockId::Air) return false;
    const auto hit = lookAtBlock(BLOCK_REACH);
    if (!hit || hit->distance >= BLOCK_REACH) return false;
    BlockPos target{};
    SlabType newType = SlabType::Bottom;
    if (selected == BlockId::StoneSlab && getBlock(hit->block) == BlockId::StoneSlab &&
        slabCanReplace(getSlabType(hit->block),hit->face,hit->location.y,hit->block.y,true)) {
        target = hit->block;
        newType = SlabType::Double;
    } else {
        const auto n = step(hit->face);
        target = {hit->block.x+n.x, hit->block.y+n.y, hit->block.z+n.z};
        if (getBlock(target) != BlockId::Air) return false;
        if (selected == BlockId::StoneSlab)
            newType = slabTypeForPlacement(hit->face,hit->location.y,hit->block.y);
    }
    const double half = PLAYER_WIDTH / 2.0;
    const AABB playerBox{player.position.x-half, player.position.y, player.position.z-half,
        player.position.x+half, player.position.y+PLAYER_HEIGHT, player.position.z+half};
    StairHalf newHalf = StairHalf::Bottom;
    Direction newFacing = fromYRot(player.yaw);
    if (selected == BlockId::StoneStairs)
        newHalf = stairHalfForPlacement(hit->face, hit->location.y, hit->block.y);
    FenceConnections fence{};
    if (selected == BlockId::OakFence) {
        auto check = [&](Direction d) {
            const auto n = step(d); const BlockPos p{target.x+n.x,target.y+n.y,target.z+n.z};
            if (getBlock(p) == BlockId::OakFence) return true;
            return getBlock(p) != BlockId::Air && fullSturdyFace(collisionBlockShape(getBlock(p),getSlabType(p),
                getStairFacing(p),getStairHalf(p),getStairShape(p),getFenceConnections(p)),opposite(d));
        };
        fence = {check(Direction::North),check(Direction::East),check(Direction::South),check(Direction::West)};
    }
    const auto placedShape = collisionBlockShape(selected,newType,newFacing,newHalf,
        selected == BlockId::StoneStairs ? stairsShapeForPlacement(*this,target,newFacing,newHalf) : StairsShape::Straight, fence)
        .move(target.x,target.y,target.z);
    if (intersectsShape(playerBox,placedShape)) return false;
    if (selected == BlockId::StoneSlab) return setSlab(target,newType);
    if (selected == BlockId::StoneStairs)
        return setStairs(target,newFacing,newHalf,stairsShapeForPlacement(*this,target,newFacing,newHalf));
    if (selected == BlockId::OakFence) return setFence(target, fence);
    return setBlock(target, selected);
}

} // namespace carbon
