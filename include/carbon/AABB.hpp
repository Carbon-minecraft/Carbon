#pragma once

// Arithmetic and segment clipping subset of net.minecraft.world.phys.AABB
// from the supplied Java Edition 26.3 sources. The implementation preserves
// the Java source's strict intersection/containment boundaries and Math
// min/max NaN and signed-zero behavior.

#include "carbon/Coordinates.hpp"
#include "carbon/Vec3.hpp"

#include <cmath>
#include <limits>
#include <optional>

namespace carbon {
namespace aabb_detail {
inline double javaMin(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
    if (a == b) return std::signbit(a) ? a : b;
    return a < b ? a : b;
}
inline double javaMax(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
    if (a == b) return std::signbit(a) ? b : a;
    return a > b ? a : b;
}
}

struct AABB {
    double minX{}, minY{}, minZ{}, maxX{}, maxY{}, maxZ{};

    AABB() = default;
    AABB(double x0, double y0, double z0, double x1, double y1, double z1)
        : minX(aabb_detail::javaMin(x0, x1)), minY(aabb_detail::javaMin(y0, y1)),
          minZ(aabb_detail::javaMin(z0, z1)), maxX(aabb_detail::javaMax(x0, x1)),
          maxY(aabb_detail::javaMax(y0, y1)), maxZ(aabb_detail::javaMax(z0, z1)) {}
    explicit AABB(const Vec3& begin, const Vec3& end)
        : AABB(begin.x, begin.y, begin.z, end.x, end.y, end.z) {}
    explicit AABB(const BlockPos& p)
        : AABB(p.x,p.y,p.z,coordinate_detail::add(p.x,1),coordinate_detail::add(p.y,1),coordinate_detail::add(p.z,1)) {}
    static AABB unitCubeFromLowerCorner(const Vec3& p) {
        return {p.x,p.y,p.z,p.x+1.0,p.y+1.0,p.z+1.0};
    }
    static AABB encapsulatingFullBlocks(const BlockPos& a,const BlockPos& b) {
        return {static_cast<double>(a.x < b.x ? a.x : b.x),
                static_cast<double>(a.y < b.y ? a.y : b.y),
                static_cast<double>(a.z < b.z ? a.z : b.z),
                static_cast<double>(coordinate_detail::add(a.x > b.x ? a.x : b.x,1)),
                static_cast<double>(coordinate_detail::add(a.y > b.y ? a.y : b.y,1)),
                static_cast<double>(coordinate_detail::add(a.z > b.z ? a.z : b.z,1))};
    }

    AABB contract(double xa, double ya, double za) const {
        double x0=minX,y0=minY,z0=minZ,x1=maxX,y1=maxY,z1=maxZ;
        if(xa<0.0)x0-=xa; else if(xa>0.0)x1-=xa;
        if(ya<0.0)y0-=ya; else if(ya>0.0)y1-=ya;
        if(za<0.0)z0-=za; else if(za>0.0)z1-=za;
        return {x0,y0,z0,x1,y1,z1};
    }
    AABB expandTowards(double xa,double ya,double za) const {
        double x0=minX,y0=minY,z0=minZ,x1=maxX,y1=maxY,z1=maxZ;
        if(xa<0.0)x0+=xa; else if(xa>0.0)x1+=xa;
        if(ya<0.0)y0+=ya; else if(ya>0.0)y1+=ya;
        if(za<0.0)z0+=za; else if(za>0.0)z1+=za;
        return {x0,y0,z0,x1,y1,z1};
    }
    AABB expandTowards(const Vec3& d) const { return expandTowards(d.x,d.y,d.z); }
    AABB inflate(double x,double y,double z) const { return {minX-x,minY-y,minZ-z,maxX+x,maxY+y,maxZ+z}; }
    AABB inflate(double amount) const { return inflate(amount,amount,amount); }
    AABB deflate(double x,double y,double z) const { return inflate(-x,-y,-z); }
    AABB deflate(double amount) const { return inflate(-amount); }
    AABB intersect(const AABB& o) const {
        return {aabb_detail::javaMax(minX,o.minX),aabb_detail::javaMax(minY,o.minY),aabb_detail::javaMax(minZ,o.minZ),
                aabb_detail::javaMin(maxX,o.maxX),aabb_detail::javaMin(maxY,o.maxY),aabb_detail::javaMin(maxZ,o.maxZ)};
    }
    AABB minmax(const AABB& o) const {
        return {aabb_detail::javaMin(minX,o.minX),aabb_detail::javaMin(minY,o.minY),aabb_detail::javaMin(minZ,o.minZ),
                aabb_detail::javaMax(maxX,o.maxX),aabb_detail::javaMax(maxY,o.maxY),aabb_detail::javaMax(maxZ,o.maxZ)};
    }
    AABB move(double x,double y,double z) const { return {minX+x,minY+y,minZ+z,maxX+x,maxY+y,maxZ+z}; }
    AABB move(const Vec3& p) const { return move(p.x,p.y,p.z); }
    AABB move(const BlockPos& p) const { return move(p.x,p.y,p.z); }
    bool intersects(const AABB& o) const { return intersects(o.minX,o.minY,o.minZ,o.maxX,o.maxY,o.maxZ); }
    bool intersects(double x0,double y0,double z0,double x1,double y1,double z1) const {
        return minX<x1&&maxX>x0&&minY<y1&&maxY>y0&&minZ<z1&&maxZ>z0;
    }
    bool intersects(const Vec3& a,const Vec3& b) const {
        return intersects(aabb_detail::javaMin(a.x,b.x),aabb_detail::javaMin(a.y,b.y),aabb_detail::javaMin(a.z,b.z),
                          aabb_detail::javaMax(a.x,b.x),aabb_detail::javaMax(a.y,b.y),aabb_detail::javaMax(a.z,b.z));
    }
    bool intersects(const BlockPos& p) const { return intersects(p.x,p.y,p.z,coordinate_detail::add(p.x,1),coordinate_detail::add(p.y,1),coordinate_detail::add(p.z,1)); }
    bool contains(const Vec3& p) const { return contains(p.x,p.y,p.z); }
    bool contains(double x,double y,double z) const { return x>=minX&&x<maxX&&y>=minY&&y<maxY&&z>=minZ&&z<maxZ; }
    double getXsize() const { return maxX-minX; }
    double getYsize() const { return maxY-minY; }
    double getZsize() const { return maxZ-minZ; }
    double getSize() const { return (getXsize()+getYsize()+getZsize())/3.0; }
    double distanceToSqr(const Vec3& p) const {
        const double dx=aabb_detail::javaMax(aabb_detail::javaMax(minX-p.x,p.x-maxX),0.0);
        const double dy=aabb_detail::javaMax(aabb_detail::javaMax(minY-p.y,p.y-maxY),0.0);
        const double dz=aabb_detail::javaMax(aabb_detail::javaMax(minZ-p.z,p.z-maxZ),0.0);
        return dx*dx+dy*dy+dz*dz;
    }
    double distanceToSqr(const AABB& b) const {
        const double dx=aabb_detail::javaMax(aabb_detail::javaMax(minX-b.maxX,b.minX-maxX),0.0);
        const double dy=aabb_detail::javaMax(aabb_detail::javaMax(minY-b.maxY,b.minY-maxY),0.0);
        const double dz=aabb_detail::javaMax(aabb_detail::javaMax(minZ-b.maxZ,b.minZ-maxZ),0.0);
        return dx*dx+dy*dy+dz*dz;
    }

    // Mirrors AABB.getDirection/clipPoint's face order, epsilon and strict
    // segment parameter tests, returning just the hit point (the Java overload
    // that returns Optional<Vec3>). It does not expose the face direction.
    std::optional<Vec3> clip(const Vec3& from,const Vec3& to) const {
        constexpr double eps=1.0E-7;
        const double dx=to.x-from.x,dy=to.y-from.y,dz=to.z-from.z;
        double best=1.0;
        bool found=false;
        auto point=[&](double da,double db,double dc,double plane,double minB,double maxB,double minC,double maxC,
                       double fromA,double fromB,double fromC) {
            const double s=(plane-fromA)/da;
            const double pb=fromB+s*db,pc=fromC+s*dc;
            if(0.0<s&&s<best&&minB-eps<pb&&pb<maxB+eps&&minC-eps<pc&&pc<maxC+eps){best=s;found=true;}
        };
        if(dx>eps)point(dx,dy,dz,minX,minY,maxY,minZ,maxZ,from.x,from.y,from.z);
        else if(dx < -eps)point(dx,dy,dz,maxX,minY,maxY,minZ,maxZ,from.x,from.y,from.z);
        if(dy>eps)point(dy,dz,dx,minY,minZ,maxZ,minX,maxX,from.y,from.z,from.x);
        else if(dy < -eps)point(dy,dz,dx,maxY,minZ,maxZ,minX,maxX,from.y,from.z,from.x);
        if(dz>eps)point(dz,dx,dy,minZ,minX,maxX,minY,maxY,from.z,from.x,from.y);
        else if(dz < -eps)point(dz,dx,dy,maxZ,minX,maxX,minY,maxY,from.z,from.x,from.y);
        if(!found)return std::nullopt;
        return Vec3{from.x+best*dx,from.y+best*dy,from.z+best*dz};
    }
};
}
