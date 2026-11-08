#include "carbon/VoxelShapes.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace carbon;

static bool near(double a,double b) { return std::abs(a-b)<1e-12; }

int main() {
    const auto empty=Shapes::empty();
    assert(empty.isEmpty());
    bool threw=false; try { (void)empty.bounds(); } catch(const std::logic_error&) { threw=true; }
    assert(threw);
    const auto cube=Shapes::block();
    assert(!cube.isEmpty());
    assert(near(cube.min(Axis::X),0)&&near(cube.max(Axis::Z),1));

    // Non-cubic slab retains its source coordinate boundaries during collision.
    const auto slab=Shapes::box(0.25,0.0,0.0,0.75,1.0,1.0);
    assert(near(slab.collide(Axis::X,AABB(-1,0.1,0.1,-0.5,0.9,0.9),2),0.75));
    assert(near(slab.collide(Axis::X,AABB(1.0,0.1,0.1,1.5,0.9,0.9),-2),-0.25));
    // Exactly touching a face blocks motion at zero clearance; away motion remains free.
    assert(near(cube.collide(Axis::X,AABB(-1,0.1,0.1,0,0.9,0.9),1),0));
    assert(near(cube.collide(Axis::X,AABB(-1,0.1,0.1,0,0.9,0.9),-1),-1));
    // Orthogonal overlap is required; merely overlapping the bounding projection is insufficient.
    assert(near(slab.collide(Axis::X,AABB(-1,2,2,-0.5,3,3),2),2));
    // Axis permutation exercises the Z and Y collision paths independently.
    assert(near(slab.collide(Axis::Y,AABB(0.3,-1,0.3,0.7,-0.5,0.7),2),0.5));
    assert(near(slab.collide(Axis::Z,AABB(0.3,0.1,-1,0.7,0.9,-0.5),2),0.5));

    // Boolean joins partition disjoint geometry and preserve merged cell occupancy.
    const auto left=Shapes::box(0,0,0,0.25,1,1);
    const auto right=Shapes::box(0.75,0,0,1,1,1);
    const auto separated=Shapes::join(left,right,BooleanOp::Or);
    std::vector<AABB> boxes;
    separated.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1){boxes.emplace_back(x0,y0,z0,x1,y1,z1);});
    assert(boxes.size()==2 && near(boxes[0].minX,0) && near(boxes[1].minX,0.75));
    assert(Shapes::joinIsNotEmpty(left,right,BooleanOp::OnlyFirst));
    assert(Shapes::join(left,right,BooleanOp::And).isEmpty());
    const auto joined=Shapes::join(Shapes::box(0,0,0,0.5,1,1),Shapes::box(0.5,0,0,1,1,1),BooleanOp::Or);
    const auto merged=joined.bounds();
    assert(near(merged.minX,0)&&near(merged.maxX,1));
    assert(apply(BooleanOp::NotSame,true,false) && !apply(BooleanOp::NotSame,true,true));
    assert(apply(BooleanOp::Causes,true,true) && !apply(BooleanOp::Causes,true,false));
    threw=false; try { (void)Shapes::join(empty,empty,BooleanOp::True); } catch(const std::invalid_argument&) { threw=true; }
    assert(threw);

    // Negative motion, epsilon stop and axis sequence retain requested motion.
    assert(near(Shapes::collide(Axis::X,AABB(2,2,2,3,3,3),{cube},-0.25),-0.25));
    assert(near(Shapes::collide(Axis::X,AABB(2,2,2,3,3,3),{cube},1e-8),0));
    std::cout << "collision " << slab.collide(Axis::X,AABB(-1,0.1,0.1,-0.5,0.9,0.9),2) << ' '
              << slab.collide(Axis::X,AABB(1,0.1,0.1,1.5,0.9,0.9),-2) << ' '
              << cube.collide(Axis::X,AABB(-1,0.1,0.1,0,0.9,0.9),1) << ' '
              << slab.collide(Axis::X,AABB(-1,2,2,-0.5,3,3),2) << ' '
              << slab.collide(Axis::Y,AABB(0.3,-1,0.3,0.7,-0.5,0.7),2) << ' '
              << slab.collide(Axis::Z,AABB(0.3,0.1,-1,0.7,0.9,-0.5),2) << '\n';
    std::cout << "voxel shapes source-derived cases passed\n";
}
