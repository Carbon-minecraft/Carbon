#include "carbon/OverworldDensity.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace carbon {
namespace {
using Json = nlohmann::json;
float f(const Json& j) { return static_cast<float>(j.get<double>()); }
int floorDiv(int v, int d) { int q=v/d, r=v%d; return r<0?q-1:q; }
float lerp(float t, float a, float b) { return a + t * (b - a); }
std::string cleanKey(std::string key) {
    if (key.rfind("minecraft:",0)==0) key.erase(0,10);
    return key;
}
}

std::size_t DensityContext::MemoHash::operator()(const MemoKey& k) const noexcept {
    auto h=std::hash<const void*>{}(k.node);
    h^=std::hash<int>{}(k.x)+0x9e3779b9+(h<<6)+(h>>2);
    h^=std::hash<int>{}(k.y)+0x9e3779b9+(h<<6)+(h>>2);
    h^=std::hash<int>{}(k.z)+0x9e3779b9+(h<<6)+(h>>2);
    return h;
}

OverworldDensityGraph::OverworldDensityGraph(std::filesystem::path dataRoot)
    : root_(std::move(dataRoot)) {}

const nlohmann::json& OverworldDensityGraph::load(const std::string& rawKey) const {
    const auto key=cleanKey(rawKey);
    auto found=files_.find(key);
    if(found!=files_.end()) return found->second;
    auto base=root_;
    if (base.filename()=="decompiled") base/=std::filesystem::path("data")/"minecraft";
    auto path=base/std::filesystem::path("worldgen")/"density_function"/(key+".json");
    std::ifstream in(path,std::ios::binary);
    if(!in) throw std::runtime_error("missing density function asset: "+path.string());
    Json value; in>>value;
    return files_.emplace(key,std::move(value)).first->second;
}

float OverworldDensityGraph::sample(const std::string& key,DensityPos pos,const DensityContext& context) const {
    return eval(load(key),pos,context,0);
}
float OverworldDensityGraph::sampleExpression(const nlohmann::json& expression,DensityPos pos,
                                               const DensityContext& context) const {
    return eval(expression,pos,context,0);
}

float RawDensityChunk::at(int x,int y,int z) const {
    if(x<0||x>=16||z<0||z>=16||y<MIN_Y||y>=MIN_Y+HEIGHT)
        throw std::out_of_range("raw density chunk coordinate outside bounds");
    return density[(static_cast<std::size_t>(z)*16+static_cast<std::size_t>(x))*HEIGHT+static_cast<std::size_t>(y-MIN_Y)];
}

int RawDensityChunk::surfaceY(int x,int z) const {
    for(int y=MIN_Y+HEIGHT-1;y>=MIN_Y;--y) if(at(x,y,z)>0.0F) return y+1;
    return MIN_Y;
}

float OverworldDensityGraph::evalSpline(const Json& spline,DensityPos p,const DensityContext& c,int depth) const {
    if(spline.is_number()) return f(spline);
    if(depth>128) throw std::runtime_error("density spline recursion limit exceeded");
    const std::string coordinate=spline.at("coordinate").get<std::string>();
    const float x=sample(coordinate,p,c);
    const auto& points=spline.at("points");
    if(points.empty()) throw std::runtime_error("empty density spline");
    const int n=static_cast<int>(points.size());
    auto loc=[&](int i){return f(points.at(static_cast<std::size_t>(i)).at("location"));};
    int lo=0,hi=n;
    while(lo<hi){int mid=(lo+hi)/2;if(x<loc(mid))hi=mid;else lo=mid+1;}
    const int start=lo-1;
    auto value=[&](int i){return evalSpline(points.at(static_cast<std::size_t>(i)).at("value"),p,c,depth+1);};
    auto derivative=[&](int i){return f(points.at(static_cast<std::size_t>(i)).at("derivative"));};
    if(start<0){float v=value(0),d=derivative(0);return d==0.0F?v:v+d*(x-loc(0));}
    if(start>=n-1){float v=value(n-1),d=derivative(n-1);return d==0.0F?v:v+d*(x-loc(n-1));}
    const float x1=loc(start),x2=loc(start+1), t=(x-x1)/(x2-x1);
    const float y1=value(start),y2=value(start+1);
    const float a=derivative(start)*(x2-x1)-(y2-y1);
    const float b=-derivative(start+1)*(x2-x1)+(y2-y1);
    return lerp(t,y1,y2)+t*(1.0F-t)*lerp(t,a,b);
}

float OverworldDensityGraph::eval(const Json& n,DensityPos p,const DensityContext& c,int depth) const {
    const DensityContext::MemoKey key{&n,p.x,p.y,p.z};
    if(c.memo) { auto it=c.memo->find(key); if(it!=c.memo->end()) return it->second; }
    const float value=evalImpl(n,p,c,depth);
    if(c.memo) c.memo->insert_or_assign(key,value);
    return value;
}

float OverworldDensityGraph::evalImpl(const Json& n,DensityPos p,const DensityContext& c,int depth) const {
    if(depth>256) throw std::runtime_error("density graph recursion limit exceeded");
    if(n.is_number()) return f(n);
    if(n.is_string()) return sample(n.get<std::string>(),p,c);
    const std::string type=n.value("type","");
    auto child=[&](const char* key){return eval(n.at(key),p,c,depth+1);};
    if(type=="minecraft:constant") return f(n.at("argument"));
    if(type=="minecraft:add"||type=="minecraft:sub"||type=="minecraft:mul"||type=="minecraft:div"||type=="minecraft:min"||type=="minecraft:max") {
        const float x=n.contains("left")?eval(n.at("left"),p,c,depth+1):child("argument1");
        const float y=n.contains("right")?eval(n.at("right"),p,c,depth+1):child("argument2");
        if(type=="minecraft:add")return x+y;if(type=="minecraft:sub")return x-y;if(type=="minecraft:mul")return x*y;
        if(type=="minecraft:div")return x/y;if(type=="minecraft:min")return std::min(x,y);return std::max(x,y);
    }
    if(type=="minecraft:cache"||type=="minecraft:cache_once"||type=="minecraft:cache_2d"||type=="minecraft:cache_all_in_cell")return child("input");
    if(type=="minecraft:abs"||type=="minecraft:square"||type=="minecraft:cube"||type=="minecraft:half_negative"||type=="minecraft:quarter_negative"||type=="minecraft:negate"||type=="minecraft:squeeze"||type=="minecraft:sqrt") {
        const float x=child("input");
        if(type=="minecraft:abs")return std::abs(x);if(type=="minecraft:square")return x*x;if(type=="minecraft:cube")return x*x*x;
        if(type=="minecraft:half_negative")return x>0?x:x*0.5F;if(type=="minecraft:quarter_negative")return x>0?x:x*0.25F;
        if(type=="minecraft:negate")return -x;if(type=="minecraft:squeeze"){float v=std::clamp(x,-1.0F,1.0F);return v*0.5F-v*v*v/24.0F;}
        return std::sqrt(x);
    }
    if(type=="minecraft:clamp") return std::clamp(child("input"),f(n.at("min")),f(n.at("max")));
    if(type=="minecraft:gradient") {
        const auto axis=n.value("axis","y"); const float v=axis=="x"?float(p.x):axis=="z"?float(p.z):float(p.y);
        const float a=float(n.at("from_coordinate").get<int>()),b=float(n.at("to_coordinate").get<int>());
        const float t=std::clamp((v-a)/(b-a),0.0F,1.0F);return lerp(t,f(n.at("from_value")),f(n.at("to_value")));
    }
    if(type=="minecraft:lerp") return lerp(child("alpha"),child("first"),child("second"));
    if(type=="minecraft:range_choice") {const float x=child("input");return x>=f(n.at("min_inclusive"))&&x<f(n.at("max_exclusive"))?child("when_in_range"):child("when_out_of_range");}
    if(type=="minecraft:interval_select") {
        const float x=child("input");const auto& thresholds=n.at("thresholds");
        std::size_t i=0;while(i<thresholds.size()&&x>=f(thresholds.at(i)))++i;
        return eval(n.at("functions").at(i),p,c,depth+1);
    }
    if(type=="minecraft:spline") return evalSpline(n.at("spline"),p,c,depth+1);
    if(type=="minecraft:noise") {
        if(!c.noise) throw std::runtime_error("noise density requires seeded noise provider");
        double x=p.x*n.value("xz_scale",1.0),y=p.y*n.value("y_scale",1.0),z=p.z*n.value("xz_scale",1.0);
        if(n.contains("shift_x"))x+=eval(n.at("shift_x"),p,c,depth+1);
        if(n.contains("shift_y"))y+=eval(n.at("shift_y"),p,c,depth+1);
        if(n.contains("shift_z"))z+=eval(n.at("shift_z"),p,c,depth+1);
        return c.noise(n.at("noise").get<std::string>(),x,y,z);
    }
    if(type=="minecraft:shift_a"||type=="minecraft:shift_b"||type=="minecraft:shift") {
        if(!c.noise) throw std::runtime_error("shift density requires seeded noise provider");
        const double x=(type=="minecraft:shift_b"?p.z:p.x)*0.25;
        const double y=(type=="minecraft:shift_a"?0.0:(type=="minecraft:shift_b"?p.x:p.y))*0.25;
        const double z=(type=="minecraft:shift_b"?0.0:((type=="minecraft:shift_a")?0.0:p.z*0.25));
        return 4.0F*c.noise(n.at("noise").get<std::string>(),x,y,z);
    }
    if(type=="minecraft:interpolated") {
        const int sx=n.value("cell_size_xz",4),sy=n.value("cell_size_y",8);
        const int x0=floorDiv(p.x,sx)*sx,y0=floorDiv(p.y,sy)*sy,z0=floorDiv(p.z,sx)*sx;
        const float tx=float(p.x-x0)/sx,ty=float(p.y-y0)/sy,tz=float(p.z-z0)/sx;
        float v[2][2][2];for(int ix=0;ix<2;++ix)for(int iy=0;iy<2;++iy)for(int iz=0;iz<2;++iz)v[ix][iy][iz]=eval(n.at("input"),{x0+ix*sx,y0+iy*sy,z0+iz*sx},c,depth+1);
        const float a00=lerp(tx,v[0][0][0],v[1][0][0]),a10=lerp(tx,v[0][1][0],v[1][1][0]);
        const float a01=lerp(tx,v[0][0][1],v[1][0][1]),a11=lerp(tx,v[0][1][1],v[1][1][1]);
        return lerp(tz,lerp(ty,a00,a10),lerp(ty,a01,a11));
    }
    if(type=="minecraft:find_top_surface") {
        const int cellHeight=n.at("cell_height").get<int>();
        if(cellHeight<=0) throw std::runtime_error("find_top_surface cell_height must be positive");
        const int lowerBound=n.at("lower_bound").get<int>();
        const float upperBound=eval(n.at("upper_bound"),p,c,depth+1);
        const int topY=static_cast<int>(std::floor(upperBound/static_cast<float>(cellHeight)))*cellHeight;
        if(topY<=lowerBound) return static_cast<float>(lowerBound);
        for(int probeY=topY;probeY>=lowerBound;probeY-=cellHeight) {
            if(eval(n.at("density"),{p.x,probeY,p.z},c,depth+1)>0.0F)
                return static_cast<float>(probeY);
        }
        return static_cast<float>(lowerBound);
    }
    if(type=="minecraft:blend_density") return child("input");
    if(type=="minecraft:blend_alpha"||type=="minecraft:blend_offset"||type=="minecraft:beardifier"||type=="minecraft:old_blended_noise") {
        if(!c.external) throw std::runtime_error(type+" requires worldgen context");
        return c.external(n,p);
    }
    throw std::runtime_error("unsupported density function type: "+type);
}

std::string OverworldDensityGraph::firstUnsupported(const std::filesystem::path& root,const std::string& key) {
    OverworldDensityGraph graph(root);
    DensityContext context;
    try { (void)graph.sample(key,{0,0,0},context); }
    catch(const std::exception& e) { return e.what(); }
    return {};
}

SeededOverworldDensity::SeededOverworldDensity(std::int64_t seed,std::filesystem::path root)
    : seed_(seed),graph_(std::move(root)),noises_(seed),
      positional_([&](){XoroshiroRandomSource random(seed);return random.forkPositionalSeed();}()) {}

float SeededOverworldDensity::finalDensity(int x,int y,int z) const {
    return function("overworld/final_density",x,y,z);
}

float SeededOverworldDensity::function(const std::string& identifier,int x,int y,int z) const {
    return beginSession().function(identifier,x,y,z);
}

SeededOverworldDensity::Session SeededOverworldDensity::beginSession() const {
    DensityContext context=makeContext();
    return Session(this,std::move(context));
}

DensityContext SeededOverworldDensity::makeContext() const {
    DensityContext context;
    context.noise=[this](const std::string& key,double nx,double ny,double nz){return noises_.sample(key,nx,ny,nz);};
    context.external=[this](const nlohmann::json& node,DensityPos pos) {
        const auto type=node.value("type","");
        if(type=="minecraft:blend_alpha") return 1.0F; // Blender.empty() for new terrain
        if(type=="minecraft:blend_offset"||type=="minecraft:beardifier") return 0.0F; // empty blend, structures disabled
        if(type=="minecraft:old_blended_noise") {
            if(!blended_) {
                auto random=positional_.fromHashOf("minecraft:terrain");
                blended_=std::make_unique<BlendedTerrainNoise>(random,
                    node.value("xz_scale",0.25),node.value("y_scale",0.125),
                    node.value("xz_factor",80.0),node.value("y_factor",160.0),
                    node.value("smear_scale_multiplier",8.0));
            }
            return blended_->sample(pos.x,pos.y,pos.z);
        }
        throw std::runtime_error("unsupported seeded density context: "+type);
    };
    return context;
}

float SeededOverworldDensity::noise(const std::string& identifier,double x,double y,double z) const {
    return noises_.sample(identifier,x,y,z);
}

XoroshiroPositionalRandomFactory SeededOverworldDensity::positionalFactory(const std::string& rawName) const {
    std::string name=rawName;
    if(name.rfind("minecraft:",0)!=0) name="minecraft:"+name;
    auto random=positional_.fromHashOf(name);
    return XoroshiroPositionalRandomFactory(random.forkPositionalSeed());
}

float SeededOverworldDensity::Session::function(const std::string& identifier,int x,int y,int z) const {
    return owner_->graph_.sample(identifier,{x,y,z},context_);
}

float SeededOverworldDensity::Session::finalDensity(int x,int y,int z) const {
    return function("overworld/final_density",x,y,z);
}

float SeededOverworldDensity::Session::noise(const std::string& identifier,double x,double y,double z) const {
    return owner_->noises_.sample(identifier,x,y,z);
}

RawDensityChunk SeededOverworldDensity::densityChunk(int chunkX,int chunkZ) const {
    RawDensityChunk result;
    result.chunkX=chunkX;result.chunkZ=chunkZ;
    result.density.resize(static_cast<std::size_t>(16)*16*RawDensityChunk::HEIGHT);
    auto session=beginSession();
    for(int z=0;z<16;++z) for(int x=0;x<16;++x) for(int y=RawDensityChunk::MIN_Y;y<RawDensityChunk::MIN_Y+RawDensityChunk::HEIGHT;++y) {
        const auto index=(static_cast<std::size_t>(z)*16+static_cast<std::size_t>(x))*RawDensityChunk::HEIGHT+static_cast<std::size_t>(y-RawDensityChunk::MIN_Y);
        result.density[index]=session.finalDensity(chunkX*16+x,y,chunkZ*16+z);
    }
    return result;
}
}
