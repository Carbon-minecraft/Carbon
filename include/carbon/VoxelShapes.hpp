#pragma once

// Focused Java Edition 26.3 net.minecraft.world.phys.shapes port.
// Supports coordinate-grid voxel occupancy, box/empty/full-block shapes,
// Boolean joins, bounds, box decomposition and axis collision. Face slicing,
// rotations, clipping, edge iteration and Minecraft's merger optimizations are
// deliberately unsupported rather than approximated.

#include "carbon/AABB.hpp"
#include "carbon/Direction.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace carbon {

enum class BooleanOp {
    False, NotOr, OnlySecond, NotFirst, OnlyFirst, NotSecond, NotSame, NotAnd,
    And, Same, Second, Causes, First, CausedBy, Or, True
};

struct Shapes;

inline bool apply(BooleanOp op, bool a, bool b) {
    switch (op) {
    case BooleanOp::False: return false;
    case BooleanOp::NotOr: return !a && !b;
    case BooleanOp::OnlySecond: return b && !a;
    case BooleanOp::NotFirst: return !a;
    case BooleanOp::OnlyFirst: return a && !b;
    case BooleanOp::NotSecond: return !b;
    case BooleanOp::NotSame: return a != b;
    case BooleanOp::NotAnd: return !a || !b;
    case BooleanOp::And: return a && b;
    case BooleanOp::Same: return a == b;
    case BooleanOp::Second: return b;
    case BooleanOp::Causes: return !a || b;
    case BooleanOp::First: return a;
    case BooleanOp::CausedBy: return a || !b;
    case BooleanOp::Or: return a || b;
    case BooleanOp::True: return true;
    }
    throw std::invalid_argument("Unknown BooleanOp");
}

class DiscreteVoxelShape {
public:
    DiscreteVoxelShape(int x, int y, int z)
        : sizes_{x,y,z}, cells_(checkedCount(x,y,z), false) {}
    int size(Axis a) const { return sizes_[static_cast<std::size_t>(a)]; }
    bool isFull(int x,int y,int z) const {
        if(x<0||y<0||z<0||x>=sizes_[0]||y>=sizes_[1]||z>=sizes_[2]) return false;
        return cells_[index(x,y,z)];
    }
    void fill(int x,int y,int z) { cells_.at(indexChecked(x,y,z))=true; }
    bool isEmpty() const { return std::none_of(cells_.begin(),cells_.end(),[](bool b){return b;}); }
    int firstFull(Axis a) const {
        const int n=size(a);
        for(int i=0;i<n;++i) {
            for(int x=0;x<sizes_[0];++x) for(int y=0;y<sizes_[1];++y) for(int z=0;z<sizes_[2];++z)
                if(component(x,y,z,a)==i && isFull(x,y,z)) return i;
        }
        return n;
    }
    int lastFull(Axis a) const {
        for(int i=size(a)-1;i>=0;--i) {
            for(int x=0;x<sizes_[0];++x) for(int y=0;y<sizes_[1];++y) for(int z=0;z<sizes_[2];++z)
                if(component(x,y,z,a)==i && isFull(x,y,z)) return i+1;
        }
        return 0;
    }
    int firstFull(Axis a,int b,int c) const {
        Axis ba=cycle(a,1), ca=cycle(a,2);
        if(b<0||c<0||b>=size(ba)||c>=size(ca)) return size(a);
        for(int i=0;i<size(a);++i) if(cellAt(a,i,b,c)) return i;
        return size(a);
    }
    int lastFull(Axis a,int b,int c) const {
        Axis ba=cycle(a,1), ca=cycle(a,2);
        if(b<0||c<0||b>=size(ba)||c>=size(ca)) return 0;
        for(int i=size(a)-1;i>=0;--i) if(cellAt(a,i,b,c)) return i+1;
        return 0;
    }
private:
    static std::size_t checkedCount(int x,int y,int z) {
        if(x<0||y<0||z<0) throw std::invalid_argument("Voxel grid sizes must be nonnegative");
        return static_cast<std::size_t>(x)*static_cast<std::size_t>(y)*static_cast<std::size_t>(z);
    }
    std::size_t index(int x,int y,int z) const { return (static_cast<std::size_t>(x)*sizes_[1]+y)*sizes_[2]+z; }
    std::size_t indexChecked(int x,int y,int z) const {
        if(x<0||y<0||z<0||x>=sizes_[0]||y>=sizes_[1]||z>=sizes_[2]) throw std::out_of_range("Voxel cell index");
        return index(x,y,z);
    }
    bool cellAt(Axis a,int i,int b,int c) const {
        int p[3]{}; p[static_cast<int>(a)]=i; p[static_cast<int>(cycle(a,1))]=b; p[static_cast<int>(cycle(a,2))]=c;
        return isFull(p[0],p[1],p[2]);
    }
    std::array<int,3> sizes_;
    std::vector<bool> cells_;
    static int component(int x,int y,int z,Axis a) { return a==Axis::X?x:a==Axis::Y?y:z; }
    static Axis cycle(Axis a,int n) { return static_cast<Axis>((static_cast<int>(a)+n)%3); }
};

class VoxelShape {
public:
    using BoxConsumer=std::function<void(double,double,double,double,double,double)>;
    VoxelShape() : grid_(0,0,0), coords_{{{0.0},{0.0},{0.0}}} {}
    VoxelShape(DiscreteVoxelShape grid,std::array<std::vector<double>,3> coords)
        : grid_(std::move(grid)),coords_(std::move(coords)) {
        for(int a=0;a<3;++a) if(coords_[a].size()!=static_cast<std::size_t>(grid_.size(static_cast<Axis>(a))+1))
            throw std::invalid_argument("Coordinate count must equal voxel size + 1");
    }
    bool isEmpty() const { return grid_.isEmpty(); }
    double min(Axis a) const { const int i=grid_.firstFull(a); return i>=grid_.size(a)?std::numeric_limits<double>::infinity():coords_[static_cast<int>(a)][i]; }
    double max(Axis a) const { const int i=grid_.lastFull(a); return i<=0?-std::numeric_limits<double>::infinity():coords_[static_cast<int>(a)][i]; }
    AABB bounds() const {
        if(isEmpty()) throw std::logic_error("No bounds for empty shape");
        return {min(Axis::X),min(Axis::Y),min(Axis::Z),max(Axis::X),max(Axis::Y),max(Axis::Z)};
    }
    const std::vector<double>& coords(Axis a) const { return coords_[static_cast<int>(a)]; }
    VoxelShape move(double x,double y,double z) const;
    void forAllBoxes(const BoxConsumer& consumer) const {
        // Java BitSetDiscreteVoxelShape.forAllBoxes merge order: Y, X, then Z.
        for(int y=0;y<grid_.size(Axis::Y);++y) for(int x=0;x<grid_.size(Axis::X);++x) {
            int start=-1;
            for(int z=0;z<=grid_.size(Axis::Z);++z) {
                if(grid_.isFull(x,y,z)) { if(start<0) start=z; }
                else if(start>=0) { consumer(coords_[0][x],coords_[1][y],coords_[2][start],coords_[0][x+1],coords_[1][y+1],coords_[2][z]); start=-1; }
            }
        }
    }
    double collide(Axis axis,const AABB& moving,double distance) const {
        constexpr double e=1.0e-7;
        if(isEmpty()) return distance;
        if(std::abs(distance)<e) return 0.0;
        const int a=static_cast<int>(axis), b=(a+1)%3, c=(a+2)%3;
        const double mins[3]{moving.minX,moving.minY,moving.minZ}, maxs[3]{moving.maxX,moving.maxY,moving.maxZ};
        int amin=findIndex(axis,mins[a]+e), amax=findIndex(axis,maxs[a]-e);
        int bmin=std::max(0,findIndex(static_cast<Axis>(b),mins[b]+e));
        int bmax=std::min(grid_.size(static_cast<Axis>(b)),findIndex(static_cast<Axis>(b),maxs[b]-e)+1);
        int cmin=std::max(0,findIndex(static_cast<Axis>(c),mins[c]+e));
        int cmax=std::min(grid_.size(static_cast<Axis>(c)),findIndex(static_cast<Axis>(c),maxs[c]-e)+1);
        const int asize=grid_.size(axis);
        if(distance>0) {
            for(int i=amax+1;i<asize;++i) for(int j=bmin;j<bmax;++j) for(int k=cmin;k<cmax;++k) if(cellAt(axis,i,j,k)) {
                double d=coords_[a][i]-maxs[a]; if(d>=-e) distance=std::min(distance,d); return distance;
            }
        } else if(distance<0) {
            for(int i=amin-1;i>=0;--i) for(int j=bmin;j<bmax;++j) for(int k=cmin;k<cmax;++k) if(cellAt(axis,i,j,k)) {
                double d=coords_[a][i+1]-mins[a]; if(d<=e) distance=std::max(distance,d); return distance;
            }
        }
        return distance;
    }
    const DiscreteVoxelShape& discrete() const { return grid_; }
private:
    DiscreteVoxelShape grid_;
    std::array<std::vector<double>,3> coords_;
    int findIndex(Axis a,double v) const { const auto& c=coords_[static_cast<int>(a)]; return static_cast<int>(std::upper_bound(c.begin(),c.end(),v)-c.begin())-1; }
    bool cellAt(Axis a,int i,int j,int k) const {
        int p[3]{}; p[static_cast<int>(a)]=i; p[(static_cast<int>(a)+1)%3]=j; p[(static_cast<int>(a)+2)%3]=k;
        return grid_.isFull(p[0],p[1],p[2]);
    }
    friend struct Shapes;
};

struct Shapes {
    static constexpr double Epsilon=1.0e-7;
    static VoxelShape empty() { return {}; }
    static VoxelShape block() { return box(0,0,0,1,1,1); }
    static VoxelShape box(double x0,double y0,double z0,double x1,double y1,double z1) {
        if(x0>x1||y0>y1||z0>z1) throw std::invalid_argument("The min values need to be smaller or equal to max values");
        if(x1-x0<Epsilon||y1-y0<Epsilon||z1-z0<Epsilon) return empty();
        DiscreteVoxelShape d(1,1,1); d.fill(0,0,0);
        return VoxelShape(std::move(d),std::array<std::vector<double>,3>{{{x0,x1},{y0,y1},{z0,z1}}});
    }
    static VoxelShape box(const AABB& a) { return box(a.minX,a.minY,a.minZ,a.maxX,a.maxY,a.maxZ); }
    static VoxelShape join(const VoxelShape& a,const VoxelShape& b,BooleanOp op) {
        if(apply(op,false,false)) throw std::invalid_argument("BooleanOp must be false for two empty regions");
        std::array<std::vector<double>,3> cs;
        std::array<int,3> ns{};
        for(int axis=0;axis<3;++axis) {
            auto& out=cs[axis];
            out=a.coords_[axis]; out.insert(out.end(),b.coords_[axis].begin(),b.coords_[axis].end());
            std::sort(out.begin(),out.end());
            out.erase(std::unique(out.begin(),out.end()),out.end());
            if(out.size()<2) { out={0.0}; ns[axis]=0; } else ns[axis]=static_cast<int>(out.size())-1;
        }
        DiscreteVoxelShape d(ns[0],ns[1],ns[2]);
        for(int x=0;x<ns[0];++x) for(int y=0;y<ns[1];++y) for(int z=0;z<ns[2];++z) {
            double px=mid(cs[0][x],cs[0][x+1]),py=mid(cs[1][y],cs[1][y+1]),pz=mid(cs[2][z],cs[2][z+1]);
            if(apply(op,contains(a,px,py,pz),contains(b,px,py,pz))) d.fill(x,y,z);
        }
        return {std::move(d),std::move(cs)};
    }
    static bool joinIsNotEmpty(const VoxelShape& a,const VoxelShape& b,BooleanOp op) { return !join(a,b,op).isEmpty(); }
    static double collide(Axis a,const AABB& moving,const std::vector<VoxelShape>& shapes,double distance) {
        for(const auto& shape:shapes) { if(std::abs(distance)<Epsilon) return 0; distance=shape.collide(a,moving,distance); }
        return distance;
    }
private:
    static double mid(double a,double b) { return a+(b-a)*0.5; }
    static bool contains(const VoxelShape& s,double x,double y,double z) {
        if(s.isEmpty()) return false;
        const double v[3]{x,y,z}; int p[3];
        for(int i=0;i<3;++i) p[i]=static_cast<int>(std::upper_bound(s.coords_[i].begin(),s.coords_[i].end(),v[i])-s.coords_[i].begin())-1;
        return s.grid_.isFull(p[0],p[1],p[2]);
    }
};

inline VoxelShape VoxelShape::move(double x,double y,double z) const {
    if(isEmpty()) return Shapes::empty();
    auto c=coords_;
    const double d[3]{x,y,z};
    for(int a=0;a<3;++a) for(double& v:c[a]) v+=d[a];
    return {grid_,std::move(c)};
}

} // namespace carbon
