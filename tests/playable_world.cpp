#include "carbon/PlayableWorld.hpp"
#include "carbon/CameraBasis.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace carbon;
    const CameraBasis facingSouth = cameraBasis(0.0F, 0.0F);
    assert(std::abs(facingSouth.forward.z - 1.0) < 1e-9);
    assert(std::abs(facingSouth.right.x + 1.0) < 1e-9);
    const CameraBasis turnedRight = cameraBasis(90.0F, 0.0F);
    assert(std::abs(turnedRight.forward.x + 1.0) < 1e-9);
    assert(std::abs(turnedRight.right.z + 1.0) < 1e-9);
    // Positive raw mouse X follows Entity.turn: it raises yaw. The real renderer
    // view matrix must put the resulting forward ray down camera -Z.
    const auto mouseRightView = cameraViewMatrix(0.15F, 0.0F, {0,0,0});
    const Vec3 mouseRightForward = cameraBasis(0.15F, 0.0F).forward;
    assert(mouseRightForward.x < 0.0);
    assert(std::abs(mouseRightView[2]*mouseRightForward.x
        + mouseRightView[6]*mouseRightForward.y
        + mouseRightView[10]*mouseRightForward.z + 1.0) < 1e-9);
    const auto pitchedDown = cameraBasis(0.0F, 10.0F).forward;
    assert(pitchedDown.y < 0.0);

    auto world = PlayableWorld::createDefaultFlat();
    world.player.position = {0.5, 10.0, 0.5};
    MoveInput strafeRight; strafeRight.strafe = 1.0F;
    world.tickCreativeMove(strafeRight);
    assert(world.player.position.x < 0.5);
    auto leftWorld = PlayableWorld::createDefaultFlat();
    leftWorld.player.position = {0.5, 10.0, 0.5};
    MoveInput strafeLeft; strafeLeft.strafe = -1.0F;
    leftWorld.tickCreativeMove(strafeLeft);
    assert(leftWorld.player.position.x > 0.5);
    assert(world.getBlock({-200,-64,91}) == BlockId::Bedrock);
    assert(world.getBlock({-200,-63,91}) == BlockId::Dirt);
    assert(world.getBlock({-200,-62,91}) == BlockId::Dirt);
    assert(world.getBlock({-200,-61,91}) == BlockId::GrassBlock);
    assert(world.getBlock({-200,-60,91}) == BlockId::Air);
    assert(world.blocksInRange({0,-62,0}, 3).size() == 7*7*4);
    auto sectionWorld=PlayableWorld::createDefaultFlat();
    const auto chunkWindow=sectionWorld.blocksInSectionWindow({8,-56,8},2,32);
    assert(chunkWindow.size()==80*80*4);
    assert(chunkWindow.front().pos==BlockPos(-32,-64,-32));
    assert(chunkWindow.back().pos==BlockPos(47,-61,47));
    const auto negativeChunkWindow=sectionWorld.blocksInSectionWindow({-8,-56,-8},2,32);
    assert(negativeChunkWindow.size()==80*80*4);
    assert(negativeChunkWindow.front().pos==BlockPos(-48,-64,-48));
    assert(negativeChunkWindow.back().pos==BlockPos(31,-61,31));
    sectionWorld.setBlock({40,-60,0},BlockId::Stone);
    const auto editedWindow=sectionWorld.blocksInSectionWindow({8,-56,8},2,32);
    assert(std::any_of(editedWindow.begin(),editedWindow.end(),[](const BlockInstance& b) {
        return b.pos==BlockPos(40,-60,0) && b.block==BlockId::Stone;
    }));

    const auto north = world.raycast({0.5,-60.5,-0.5}, {0,0,1}, 5.0);
    assert(north && north->block == BlockPos(0,-61,0) && north->face == Direction::North);
    assert(std::abs(north->distance - 0.5) < 1e-9);
    const auto negative = world.raycast({-1.5,-60.5,-0.5}, {0,0,1}, 5.0);
    assert(negative && negative->block == BlockPos(-2,-61,0));
    assert(!world.raycast({0.5,-59.0,-0.5}, {0,0,1}, 5.0));
    for (int z=-7; z<=-3; ++z) world.setBlock({0,-61,z}, BlockId::Air);
    world.setBlock({0,-61,-2}, BlockId::Stone);
    assert(!world.raycast({0.5,-60.5,-7.0}, {0,0,1}, 5.0)); // hit exactly at reach is excluded
    assert(world.raycast({0.5,-60.5,-7.0}, {0,0,1}, 5.01));

    world.player.position = {0.5,-58.0,0.5};
    world.player.pitch = 90.0F;
    assert(world.placeTarget(BlockId::Stone));
    assert(world.getBlock({0,-60,0}) == BlockId::Stone);
    assert(world.breakTarget());
    assert(world.getBlock({0,-60,0}) == BlockId::Air);
    world.player.position.y = -60.0;
    assert(!world.placeTarget(BlockId::Stone)); // placement cell intersects player bounds
    world.player.position.y = -40.0;
    assert(!world.placeTarget(BlockId::Stone)); // hit is outside the creative interaction range

    world.player = PlayerState{};
    world.player.pitch = 45.0F;
    const auto closeTarget = world.lookAtBlock(5.0);
    assert(closeTarget);
    world.player.position = {0.5,-60.0,0.5};
    world.player.yaw = 90.0F;
    world.player.pitch = 0.0F;
    world.setBlock({-2,-59,0}, BlockId::Stone);
    const auto turnedTarget = world.lookAtBlock(5.0);
    assert(turnedTarget && turnedTarget->block == BlockPos(-2,-59,0));
    world.player.position = {0.5,-60.0,0.5};
    world.player.yaw = -90.0F;
    world.setBlock({1,-60,0}, BlockId::Stone);
    MoveInput forward; forward.forward = 1.0F;
    for (int i=0; i<100; ++i) world.tickCreativeMove(forward);
    assert(world.player.position.x < 0.71); // full-cube collision keeps player outside the wall
    const double yBefore = world.player.position.y;
    MoveInput jump; jump.jump = true;
    world.tickCreativeMove(jump);
    assert(world.player.position.y > yBefore);

    auto grounded = PlayableWorld::createDefaultFlat();
    assert(!grounded.player.flying && grounded.player.onGround);
    assert(std::abs(grounded.player.position.y + 60.0) < 1e-9);
    MoveInput walk; walk.forward=1.0F;
    for(int i=0;i<20;++i) grounded.tickCreativeMove(walk);
    assert(grounded.player.onGround);
    const double walked = grounded.player.position.z-0.5;
    assert(walked > 4.0 && walked < 5.0); // grounded friction caps speed near source walk pace
    MoveInput sprint=walk; sprint.sprint=true;
    auto sprinter=PlayableWorld::createDefaultFlat();
    for(int i=0;i<20;++i) sprinter.tickCreativeMove(sprint);
    assert(sprinter.player.position.z-0.5 > walked);

    auto jumper=PlayableWorld::createDefaultFlat();
    MoveInput takeoff; takeoff.jump=true;
    jumper.tickCreativeMove(takeoff);
    assert(jumper.player.position.y > -60.0 && !jumper.player.onGround);
    takeoff.jump=false;
    for(int i=0;i<40;++i) jumper.tickCreativeMove(takeoff);
    assert(jumper.player.onGround && std::abs(jumper.player.position.y+60.0)<1e-6);

    auto falling=PlayableWorld::createDefaultFlat();
    falling.player.position.y=-40.0; falling.player.onGround=false;
    for(int i=0;i<100;++i) falling.tickCreativeMove(takeoff);
    assert(falling.player.onGround && std::abs(falling.player.position.y+60.0)<1e-6);

    auto flyer=PlayableWorld::createDefaultFlat();
    flyer.tickCreativeMove(takeoff); // establish released-key state
    takeoff.jump=true; flyer.tickCreativeMove(takeoff);
    takeoff.jump=false; flyer.tickCreativeMove(takeoff);
    takeoff.jump=true; flyer.tickCreativeMove(takeoff);
    assert(flyer.player.flying); // Java creative flight double-tap window

    auto croucher=PlayableWorld::createDefaultFlat();
    croucher.player.position.x=0.70;
    MoveInput sneak; sneak.strafe=1.0F; sneak.descend=true;
    const double edgeX=croucher.player.position.x;
    for(int i=0;i<30;++i) croucher.tickCreativeMove(sneak);
    assert(croucher.player.crouching && croucher.player.eyeHeight<1.62);
    assert(croucher.player.position.x-edgeX < 0.35); // shift slows and prevents walking off a ledge

    auto ledge=PlayableWorld::createDefaultFlat();
    for(int x=1;x<5;++x) ledge.setBlock({x,-61,0},BlockId::Air);
    MoveInput careful; careful.strafe=-1.0F; careful.descend=true;
    for(int i=0;i<80;++i) ledge.tickCreativeMove(careful);
    assert(ledge.player.onGround && ledge.player.position.x<1.31);

    auto stepper=PlayableWorld::createDefaultFlat();
    stepper.player.yaw=-90.0F;
    stepper.setSlab({1,-60,0},SlabType::Bottom);
    MoveInput approach; approach.forward=1.0F;
    for(int i=0;i<7;++i) stepper.tickCreativeMove(approach);
    assert(stepper.player.position.x>0.8 && stepper.player.position.y>-60.0);

    auto fenceCollision=PlayableWorld::createDefaultFlat();
    fenceCollision.player.yaw=-90.0F;
    fenceCollision.setFence({1,-60,0});
    for(int i=0;i<30;++i) fenceCollision.tickCreativeMove(approach);
    assert(fenceCollision.player.position.x<1.2);

    std::cout << "playable world tests passed\n";
}








