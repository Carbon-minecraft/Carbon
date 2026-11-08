#include "carbon/Rotation.hpp"
#include <iostream>
#include <sstream>
#include <string>

using namespace carbon;
int main() {
    std::string line;
    while (std::getline(std::cin,line)) {
        std::istringstream in(line); std::string op; in>>op;
        if(op=="id3"||op=="id2"||op=="byid"||op=="rotbyid") { int x; in>>x; std::cout<<(op=="id3"?static_cast<int>(from3DDataValue(x)):op=="id2"?static_cast<int>(from2DDataValue(x)):op=="byid"?static_cast<int>(directionById(x)):static_cast<int>(rotationById(x)))<<'\n'; }
        else if(op=="opp"||op=="cw"||op=="ccw") { int d; in>>d; auto v=static_cast<Direction>(d); std::cout<<static_cast<int>(op=="opp"?opposite(v):op=="cw"?clockwise(v):counterClockwise(v))<<'\n'; }
        else if(op=="cwa"||op=="ccwa") { int d,a; in>>d>>a; auto v=static_cast<Direction>(d); auto ax=static_cast<Axis>(a); std::cout<<static_cast<int>(op=="cwa"?clockwise(v,ax):counterClockwise(v,ax))<<'\n'; }
        else if(op=="rotD"||op=="mirD"||op=="mirR") { int x,y; in>>x>>y; if(op=="rotD") std::cout<<static_cast<int>(rotate(static_cast<Rotation>(x),static_cast<Direction>(y)))<<'\n'; else if(op=="mirD") std::cout<<static_cast<int>(mirror(static_cast<Mirror>(x),static_cast<Direction>(y)))<<'\n'; else std::cout<<static_cast<int>(mirrorRotation(static_cast<Mirror>(x),static_cast<Direction>(y)))<<'\n'; }
        else if(op=="rotI"||op=="mirI") { int a,b,c; in>>a>>b>>c; std::cout<<(op=="rotI"?rotate(static_cast<Rotation>(a),b,c):mirror(static_cast<Mirror>(a),b,c))<<'\n'; }
        else if(op=="near") { int x,y,z,f; in>>x>>y>>z>>f; std::cout<<static_cast<int>(nearest(x,y,z,static_cast<Direction>(f)))<<'\n'; }
        else if(op=="approx") { float x,y,z; in>>x>>y>>z; std::cout<<static_cast<int>(approximateNearest(x,y,z))<<'\n'; }
        else if(op=="yaw") { double x; in>>x; std::cout<<static_cast<int>(fromYRot(x))<<'\n'; }
        else if(op=="meta") { int d; in>>d; const auto v=static_cast<Direction>(d); const auto info=directionInfo(v); std::cout<<info.data3d<<' '<<info.data2d<<' '<<static_cast<int>(info.axis)<<' '<<static_cast<int>(info.axisDirection)<<' '<<info.normal.x<<' '<<info.normal.y<<' '<<info.normal.z<<' '<<name(v)<<'\n'; }
        else return 2;
    }
}
