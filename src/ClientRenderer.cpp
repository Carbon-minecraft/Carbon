#include "carbon/ClientRenderer.hpp"
#include "carbon/CameraBasis.hpp"
#include "carbon/SmoothLighting.hpp"
#include "carbon/SlabBlock.hpp"
#include "carbon/StairsBlock.hpp"
#include "carbon/FenceBlock.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL/gl.h>
#include <wincodec.h>
#include <wrl/client.h>


#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <vector>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace carbon {
namespace {
using Microsoft::WRL::ComPtr;

std::uint64_t mixHash(std::uint64_t h, std::uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

Direction directionFromName(std::string_view value) {
    for (Direction direction : DirectionValues) if (directionInfo(direction).name == value) return direction;
    throw std::runtime_error("unsupported face direction: " + std::string(value));
}

VoxelShape renderCollisionShape(const BlockInstance& b) {
    switch (b.block) {
    case BlockId::StoneSlab: return slabVoxelShape(b.slabType);
    case BlockId::StoneStairs: return stairsVoxelShape(b.stairFacing,b.stairHalf,b.stairShape);
    case BlockId::OakFence: return fenceCollisionShape(b.fenceConnections);
    case BlockId::Air: case BlockId::Water: return Shapes::empty();
    default: return Shapes::block();
    }
}

bool fullRenderSupportFace(const VoxelShape& shape, Direction face) {
    if (shape.isEmpty()) return false;
    constexpr int divisions = 16;
    std::array<std::array<bool, divisions>, divisions> covered{};
    shape.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
        const double n0 = face==Direction::West ? x0 : face==Direction::East ? x1 : face==Direction::Down ? y0 : face==Direction::Up ? y1 : face==Direction::North ? z0 : z1;
        const double edge = (face==Direction::West || face==Direction::Down || face==Direction::North) ? 0.0 : 1.0;
        if (std::abs(n0-edge)>1e-9) return;
        const double a0 = (face==Direction::West || face==Direction::East) ? y0 : x0;
        const double a1 = (face==Direction::West || face==Direction::East) ? y1 : x1;
        const double b0 = (face==Direction::North || face==Direction::South) ? y0 : z0;
        const double b1 = (face==Direction::North || face==Direction::South) ? y1 : z1;
        for (int a=0;a<divisions;++a) for (int b=0;b<divisions;++b) {
            const double pa=(a+0.5)/divisions, pb=(b+0.5)/divisions;
            if (pa>=a0-1e-9 && pa<=a1+1e-9 && pb>=b0-1e-9 && pb<=b1+1e-9) covered[a][b]=true;
        }
    });
    for (const auto& row:covered) if (!std::all_of(row.begin(),row.end(),[](bool v){return v;})) return false;
    return true;
}

struct CachedSourceResources {
    std::filesystem::path root;
    std::shared_ptr<BlockModelLoader> loader;
    std::map<std::string,BlockStateModels,std::less<>> states;
    std::map<std::string,ModelLoadResult,std::less<>> models;
    std::map<std::string,std::shared_ptr<const SourceModelGeometry>,std::less<>> geometries;
};
std::mutex sourceResourceMutex;
CachedSourceResources sourceResourceCache;

const BlockModelLoader& cachedLoader(const std::filesystem::path& root) {
    if (sourceResourceCache.root!=root) {
        sourceResourceCache={};
        sourceResourceCache.root=root;
        sourceResourceCache.loader=std::make_shared<BlockModelLoader>(root);
    }
    return *sourceResourceCache.loader;
}


const BlockStateModels& cachedBlockState(const BlockModelLoader& loader,
                                         const std::filesystem::path& root,
                                         std::string_view id) {
    if (sourceResourceCache.root!=root) {
        sourceResourceCache={}; sourceResourceCache.root=root;
    }
    const auto key=std::string(id);
    auto [it,inserted]=sourceResourceCache.states.try_emplace(key);
    if (inserted) it->second=loader.loadBlockState(id);
    return it->second;
}

const ModelLoadResult& cachedModel(const BlockModelLoader& loader,
                                   const std::filesystem::path& root,
                                   std::string_view id) {
    if (sourceResourceCache.root!=root) {
        sourceResourceCache={}; sourceResourceCache.root=root;
    }
    const auto key=std::string(id);
    auto [it,inserted]=sourceResourceCache.models.try_emplace(key);
    if (inserted) it->second=loader.loadModel(id);
    return it->second;
}

struct Image {
    UINT width{}, height{};
    std::vector<std::uint8_t> rgba;
};

std::array<float, 4> faceUv(const ModelElement& element, const ModelFace& face,
                            std::string_view direction) {
    if (face.uv) return *face.uv;
    const auto& f = element.from;
    const auto& t = element.to;
    if (direction == "down") return {f[0], 16.0f - t[2], t[0], 16.0f - f[2]};
    if (direction == "up") return {f[0], f[2], t[0], t[2]};
    if (direction == "north") return {16.0f - t[0], 16.0f - t[1], 16.0f - f[0], 16.0f - f[1]};
    if (direction == "south") return {f[0], 16.0f - t[1], t[0], 16.0f - f[1]};
    if (direction == "west") return {f[2], 16.0f - t[1], t[2], 16.0f - f[1]};
    if (direction == "east") return {16.0f - t[2], 16.0f - t[1], 16.0f - f[2], 16.0f - f[1]};
    throw std::runtime_error("unsupported model face direction: " + std::string(direction));
}

std::array<std::array<float, 3>, 4> facePositions(const ModelElement& e, std::string_view d) {
    const auto& f=e.from; const auto& t=e.to;
    // These corners correspond to increasing U then V in the direction's
    // conventional model-space projection. Positions are normalized to blocks.
    if (d=="down") return {{{f[0],f[1],t[2]},{f[0],f[1],f[2]},{t[0],f[1],f[2]},{t[0],f[1],t[2]}}};
    if (d=="up") return {{{f[0],t[1],t[2]},{t[0],t[1],t[2]},{t[0],t[1],f[2]},{f[0],t[1],f[2]}}};
    if (d=="north") return {{{t[0],t[1],f[2]},{t[0],f[1],f[2]},{f[0],f[1],f[2]},{f[0],t[1],f[2]}}};
    if (d=="south") return {{{f[0],t[1],t[2]},{f[0],f[1],t[2]},{t[0],f[1],t[2]},{t[0],t[1],t[2]}}};
    if (d=="west") return {{{f[0],t[1],f[2]},{f[0],f[1],f[2]},{f[0],f[1],t[2]},{f[0],t[1],t[2]}}};
    if (d=="east") return {{{t[0],t[1],t[2]},{t[0],f[1],t[2]},{t[0],f[1],f[2]},{t[0],t[1],f[2]}}};
    throw std::runtime_error("unsupported model face direction: " + std::string(d));
}

SourceModelQuad compileFace(const ModelElement& element, const std::string& direction,
                            const ModelFace& face, const BlockModelVariant& variant) {
    SourceModelQuad quad;
    const auto raw=facePositions(element,direction);
    const float rx=variant.xRotation*3.14159265358979323846f/180.0f;
    const float ry=variant.yRotation*3.14159265358979323846f/180.0f;
    const float rz=variant.zRotation*3.14159265358979323846f/180.0f;
    const float cx=std::cos(rx),sx=std::sin(rx),cy=std::cos(ry),sy=std::sin(ry),cz=std::cos(rz),sz=std::sin(rz);
    auto transformElement=[&](std::array<float,3> p, bool normal=false) {
        if (!element.rotation) return p;
        const auto& e=*element.rotation;
        float x=p[0]-(normal?0.0f:e.origin[0]);
        float y=p[1]-(normal?0.0f:e.origin[1]);
        float z=p[2]-(normal?0.0f:e.origin[2]);
        const float angle=e.angle*3.14159265358979323846f/180.0f;
        const float c=std::cos(angle),s=std::sin(angle);
        float a=x,b=y;
        if(e.axis=="x") { y=c*b-s*z; z=s*b+c*z; }
        else if(e.axis=="y") { const float oldX=x,oldZ=z; x=c*oldX+s*oldZ; z=-s*oldX+c*oldZ; }
        else { x=c*x-s*y; y=s*a+c*y; }
        if(e.rescale) {
            const float scale=1.0f/c;
            const float normalScale=c;
            const float k=normal?normalScale:scale;
            if(e.axis!="x") x*=k;
            if(e.axis!="y") y*=k;
            if(e.axis!="z") z*=k;
        }
        if(!normal) { x+=e.origin[0]; y+=e.origin[1]; z+=e.origin[2]; }
        return std::array<float,3>{x,y,z};
    };
    auto rotateAfterElement=[&](const std::array<float,3>& p) {
        float x=p[0]/16.0f-0.5f,y=p[1]/16.0f-0.5f,z=p[2]/16.0f-0.5f;
        // Quadrant.fromXYZAngles: Rz * Ry * Rx, centered on the block.
        float y1=cx*y-sx*z,z1=sx*y+cx*z; y=y1; z=z1;
        float x1=cy*x+sy*z; z1=-sy*x+cy*z; x=x1; z=z1;
        x1=cz*x-sz*y; y1=sz*x+cz*y; x=x1; y=y1;
        auto snap=[](float v) { const float r=std::round(v); return std::abs(v-r)<1.0e-5f?r:v; };
        return std::array<float,3>{snap(x+0.5f),snap(y+0.5f),snap(z+0.5f)};
    };
    auto rotate=[&](const std::array<float,3>& rawPoint) { return rotateAfterElement(transformElement(rawPoint)); };
    for (std::size_t i=0;i<raw.size();++i) quad.positions[i]=rotate(raw[i]);
    const auto uv=faceUv(element,face,direction);
    const std::array<std::array<float,2>,4> sourceUv{{
        {uv[0],uv[1]}, {uv[0],uv[3]}, {uv[2],uv[3]}, {uv[2],uv[1]}
    }};
    const int turns=face.rotation/90;
    for (int i=0;i<4;++i) quad.uvs[i]=sourceUv[(i+turns)%4];
    if(variant.uvLock && (variant.xRotation || variant.yRotation || variant.zRotation)) {
        auto localToWorld=[](const std::array<float,3>& v,std::string_view face) {
            if(face=="south") return v;
            if(face=="east") return std::array<float,3>{v[2],v[1],-v[0]};
            if(face=="west") return std::array<float,3>{-v[2],v[1],v[0]};
            if(face=="north") return std::array<float,3>{-v[0],v[1],-v[2]};
            if(face=="up") return std::array<float,3>{v[0],v[2],-v[1]};
            return std::array<float,3>{v[0],-v[2],v[1]}; // down
        };
        const std::array<float,3> sourceNormal=direction=="down"?std::array<float,3>{0,-1,0}:direction=="up"?std::array<float,3>{0,1,0}:direction=="north"?std::array<float,3>{0,0,-1}:direction=="south"?std::array<float,3>{0,0,1}:direction=="west"?std::array<float,3>{-1,0,0}:std::array<float,3>{1,0,0};
        const auto elementNormal=transformElement(sourceNormal,true);
        auto rn=rotateAfterElement({elementNormal[0]*16+8,elementNormal[1]*16+8,elementNormal[2]*16+8});
        const std::array<float,3> worldNormal{rn[0]-.5f,rn[1]-.5f,rn[2]-.5f};
        const std::array<std::pair<const char*,std::array<float,3>>,6> dirs{{{"down",{0,-1,0}},{"up",{0,1,0}},{"north",{0,0,-1}},{"south",{0,0,1}},{"west",{-1,0,0}},{"east",{1,0,0}}}};
        auto target=std::max_element(dirs.begin(),dirs.end(),[&](const auto& a,const auto& b){
            const float da=a.second[0]*worldNormal[0]+a.second[1]*worldNormal[1]+a.second[2]*worldNormal[2];
            const float db=b.second[0]*worldNormal[0]+b.second[1]*worldNormal[1]+b.second[2]*worldNormal[2]; return da<db;
        });
        auto inverseLocal=[](std::array<float,3> v,std::string_view face) {
            if(face=="south") return v;
            if(face=="east") return std::array<float,3>{-v[2],v[1],v[0]};
            if(face=="west") return std::array<float,3>{v[2],v[1],-v[0]};
            if(face=="north") return std::array<float,3>{-v[0],v[1],-v[2]};
            if(face=="up") return std::array<float,3>{v[0],-v[2],v[1]};
            return std::array<float,3>{v[0],v[2],-v[1]}; // down
        };
        for(auto& uvp:quad.uvs) {
            auto v=localToWorld({uvp[0]-8.0f,uvp[1]-8.0f,0.0f},direction);
            auto p=rotate({v[0]+8.0f,v[1]+8.0f,v[2]+8.0f});
            v=inverseLocal({(p[0]-.5f)*16.0f,(p[1]-.5f)*16.0f,(p[2]-.5f)*16.0f},target->first);
            uvp={v[0]+8.0f,v[1]+8.0f};
        }
    }
    quad.texture=face.texture;
    quad.cullFace=face.cullFace;
    if (quad.cullFace) {
        const std::array<std::pair<const char*,std::array<float,3>>,6> normals{{
            {"down",{0,-1,0}},{"up",{0,1,0}},{"north",{0,0,-1}},
            {"south",{0,0,1}},{"west",{-1,0,0}},{"east",{1,0,0}}}};
        auto it=std::find_if(normals.begin(),normals.end(),[&](const auto& v){return *quad.cullFace==v.first;});
        if(it==normals.end()) throw std::runtime_error("unsupported cullface: "+*quad.cullFace);
        auto n=it->second;
        const auto elementNormal=transformElement(n,true);
        auto transformed=rotateAfterElement({elementNormal[0]*16.0f+8.0f,elementNormal[1]*16.0f+8.0f,elementNormal[2]*16.0f+8.0f});
        const float dx=transformed[0]-0.5f,dy=transformed[1]-0.5f,dz=transformed[2]-0.5f;
        const std::array<float,3> tn{dx,dy,dz};
        auto mapped=std::max_element(normals.begin(),normals.end(),[&](const auto& a,const auto& b){
            const float da=a.second[0]*tn[0]+a.second[1]*tn[1]+a.second[2]*tn[2];
            const float db=b.second[0]*tn[0]+b.second[1]*tn[1]+b.second[2]*tn[2]; return da<db;
        });
        quad.cullFace=mapped->first;
    }
    // The same proper rotation applies to the face normal. Face corners are
    // kept in source order; their transformed cross product preserves winding.
    const std::array<float,3> n = direction=="down" ? std::array<float,3>{0,-1,0} : direction=="up" ? std::array<float,3>{0,1,0} : direction=="north" ? std::array<float,3>{0,0,-1} : direction=="south" ? std::array<float,3>{0,0,1} : direction=="west" ? std::array<float,3>{-1,0,0} : std::array<float,3>{1,0,0};
    const auto elementNormal=transformElement(n,true);
    const auto center=rotateAfterElement({elementNormal[0]*16.0f+8.0f,elementNormal[1]*16.0f+8.0f,elementNormal[2]*16.0f+8.0f});
    quad.normal={center[0]-0.5f,center[1]-0.5f,center[2]-0.5f};
    quad.tintIndex=face.tintIndex;
    return quad;
}

std::string geometryKey(std::string_view stateId, std::string_view selector) {
    return std::string(stateId) + "|" + std::string(selector);
}

// LegacyRandomSource and Mth.getSeed are the exact position model-selection
// path used by BlockBehaviour and WeightedList in the supplied Java sources.
std::int32_t javaNextInt(std::uint64_t& state, std::int32_t bound) {
    state=(state*25214903917ULL+11ULL)&((1ULL<<48)-1);
    std::int32_t bits=static_cast<std::int32_t>(state>>17);
    if((bound&-bound)==bound)
        return static_cast<std::int32_t>((static_cast<std::uint64_t>(bound)*static_cast<std::uint32_t>(bits))>>31);
    std::int32_t value=bits%bound;
    while (bits-value+(bound-1)<0) {
        state=(state*25214903917ULL+11ULL)&((1ULL<<48)-1);
        bits=static_cast<std::int32_t>(state>>17); value=bits%bound;
    }
    return value;
}

std::int64_t javaNextLong(std::uint64_t& state) {
    const auto next=[&]() { state=(state*25214903917ULL+11ULL)&((1ULL<<48)-1); return static_cast<std::uint32_t>(state>>16); };
    const auto upper=std::bit_cast<std::int32_t>(next());
    const auto lower=std::bit_cast<std::int32_t>(next());
    return (static_cast<std::int64_t>(upper)<<32)+static_cast<std::int64_t>(lower);
}

std::uint64_t modelSeed(std::int32_t x,std::int32_t y,std::int32_t z) {
    // Mth.getSeed first overflows x*3129871 as a Java int, then evaluates the
    // rest of the polynomial as wrapping Java longs before arithmetic >> 16.
    const auto u32=[](std::int32_t v){return static_cast<std::uint32_t>(v);};
    const auto xProduct=std::bit_cast<std::int32_t>(u32(x)*3129871u);
    std::uint64_t seed=static_cast<std::uint64_t>(static_cast<std::int64_t>(xProduct)) ^
        static_cast<std::uint64_t>(static_cast<std::int64_t>(z)*116129781LL) ^
        static_cast<std::uint64_t>(static_cast<std::int64_t>(y));
    seed=seed*seed*42317861ULL+seed*11ULL;
    return static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(seed)>>16);
}

bool readPng(const std::filesystem::path& path, Image& image) {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory)))) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, &decoder))) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) return false;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;
    if (FAILED(converter->GetSize(&image.width, &image.height)) || image.width == 0 || image.height == 0) return false;
    image.rgba.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    return SUCCEEDED(converter->CopyPixels(nullptr, image.width * 4,
            static_cast<UINT>(image.rgba.size()), image.rgba.data()));
}

std::string_view guiItemId(BlockId block) {
    switch (block) {
    case BlockId::Bedrock: return "minecraft:bedrock";
    case BlockId::Dirt: return "minecraft:dirt";
    case BlockId::GrassBlock: return "minecraft:grass_block";
    case BlockId::Stone: return "minecraft:stone";
    case BlockId::StoneSlab: return "minecraft:stone_slab";
    case BlockId::StoneStairs: return "minecraft:stone_stairs";
    case BlockId::OakFence: return "minecraft:oak_fence";
    default: return {};
    }
}

} // namespace

SourceModelGeometry loadSourceBlockGeometry(const std::filesystem::path& resourceRoot,
                                            std::string_view blockStateId,
                                            std::string_view selector,
                                            std::int32_t blockX,
                                            std::int32_t blockY,
                                            std::int32_t blockZ,
                                            std::shared_ptr<const SourceModelGeometry>* cached) {
    SourceModelGeometry result;
    if (cached) cached->reset();
    try {
        std::lock_guard cacheLock(sourceResourceMutex);
        const auto& loader=cachedLoader(resourceRoot);
        const auto& state=cachedBlockState(loader,resourceRoot,blockStateId);
        if (!state) throw std::runtime_error("blockstate " + std::string(blockStateId) + ": " + state.error);
        auto choose=[&](const std::vector<BlockModelVariant>& alternatives, std::uint64_t seed) -> const BlockModelVariant& {
            if (alternatives.empty()) throw std::runtime_error("empty model alternative list for " + std::string(blockStateId));
            std::int64_t total=0;
            for(const auto& v:alternatives) total+=v.weight;
            if(total<=0 || total>std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("invalid weighted model total for " + std::string(blockStateId));
            std::uint64_t random=(seed^25214903917ULL)&((1ULL<<48)-1);
            int value=javaNextInt(random,static_cast<std::int32_t>(total));
            for(const auto& candidate:alternatives) { value-=candidate.weight; if(value<0) return candidate; }
            throw std::runtime_error("weighted model selection exceeded source alternatives");
        };
        std::vector<BlockModelVariant> selected;
        const std::uint64_t seed=modelSeed(blockX,blockY,blockZ);
        if(!state.isMultipart) {
            const auto variant=state.variants.find(selector);
            if(variant==state.variants.end()) throw std::runtime_error("unsupported blockstate selector '" + std::string(selector) + "' for " + std::string(blockStateId));
            selected.push_back(choose(variant->second,seed));
        } else {
            std::uint64_t multipartRandom=(seed^25214903917ULL)&((1ULL<<48)-1);
            const auto alternativesSeed=static_cast<std::uint64_t>(javaNextLong(multipartRandom));
            std::map<std::string,std::string,std::less<>> values;
            std::istringstream fields{std::string(selector)}; std::string field;
            while(std::getline(fields,field,',')) {
                if(field.empty()) continue;
                const auto eq=field.find('=');
                if(eq==std::string::npos || eq==0 || eq+1==field.size() || !values.emplace(field.substr(0,eq),field.substr(eq+1)).second)
                    throw std::runtime_error("invalid multipart state selector: " + std::string(selector));
            }
            std::function<bool(const BlockStateModels::Condition&)> matches;
            matches=[&](const BlockStateModels::Condition& condition) {
                using Kind=BlockStateModels::Condition::Kind;
                if(condition.kind==Kind::Property) {
                    const auto it=values.find(condition.property);
                    if(it==values.end()) throw std::runtime_error("multipart condition requires missing state property: " + condition.property);
                    return std::any_of(condition.values.begin(),condition.values.end(),[&](const std::string& term) {
                        const bool negated=!term.empty() && term.front()=='!';
                        const std::string_view expected=negated?std::string_view(term).substr(1):std::string_view(term);
                        if(expected.empty()) throw std::runtime_error("invalid negated multipart property value");
                        return (it->second==expected)!=negated;
                    });
                }
                if(condition.children.empty()) throw std::runtime_error("empty multipart combined condition");
                if(condition.kind==Kind::Any) return std::any_of(condition.children.begin(),condition.children.end(),matches);
                return std::all_of(condition.children.begin(),condition.children.end(),matches);
            };
            for(const auto& part:state.multipart) {
                if(part.when && !matches(*part.when)) continue;
                selected.push_back(choose(part.apply,alternativesSeed));
            }
        }
        std::string selectedKey=std::string(blockStateId)+"|"+std::string(selector);
        for (const auto& variant:selected) {
            selectedKey+="|"+variant.model+","+std::to_string(variant.xRotation)+","+
                std::to_string(variant.yRotation)+","+std::to_string(variant.zRotation)+","+
                std::to_string(variant.uvLock);
        }
        if (const auto found=sourceResourceCache.geometries.find(selectedKey);found!=sourceResourceCache.geometries.end()) {
            if (cached) { *cached=found->second; return {}; }
            return *found->second;
        }
        for (const auto& variant:selected) {
            const auto& loaded=cachedModel(loader,resourceRoot,variant.model);
            if(!loaded) throw std::runtime_error("model " + variant.model + ": " + loaded.error);
            for(const auto& element:loaded.model->elements)
                for(const auto& [direction,face]:element.faces) result.quads.push_back(compileFace(element,direction,face,variant));
        }
        if(result.quads.empty()) throw std::runtime_error("source blockstate selected no model faces: " + std::string(blockStateId));
        auto shared=std::make_shared<const SourceModelGeometry>(result);
        sourceResourceCache.geometries.emplace(std::move(selectedKey),shared);
        if (cached) { *cached=std::move(shared); return {}; }
    } catch (const std::exception& error) {
        result.quads.clear();
        result.error=error.what();
    }
    return result;
}

SourceModelGeometry loadSourceModelGeometry(const std::filesystem::path& resourceRoot, std::string_view modelId) {
    SourceModelGeometry result;
    try {
        std::lock_guard cacheLock(sourceResourceMutex);
        const auto& loader=cachedLoader(resourceRoot);
        const auto& loaded=cachedModel(loader,resourceRoot,modelId);
        if(!loaded) throw std::runtime_error("model " + std::string(modelId) + ": " + loaded.error);
        const BlockModelVariant identity{};
        for(const auto& element:loaded.model->elements)
            for(const auto& [direction,face]:element.faces) result.quads.push_back(compileFace(element,direction,face,identity));
        if(result.quads.empty()) throw std::runtime_error("source model has no element faces: " + std::string(modelId));
    } catch(const std::exception& error) { result.quads.clear(); result.error=error.what(); }
    return result;
}

ClientRenderer::ClientRenderer() = default;
ClientRenderer::~ClientRenderer() { shutdown(); }

bool ClientRenderer::initialize(HWND window, int width, int height,
                                std::filesystem::path sourceRoot,
                                std::string* error) {
    shutdown();
    lastError_.clear(); presentedFrames_=0; stats_={};
    loggedContextFailure_=loggedSwapFailure_=loggedGlFailure_=false;
    if (!window) { if (error) *error = "Renderer requires a valid HWND"; return false; }
    window_ = window;
    dc_ = GetDC(window_);
    if (!dc_) { if (error) *error = "GetDC failed"; shutdown(); return false; }
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24;
    pfd.cStencilBits = 8; pfd.iLayerType = PFD_MAIN_PLANE;
    const int format = ChoosePixelFormat(dc_, &pfd);
    if (!format || !SetPixelFormat(dc_, format, &pfd)) {
        if (error) *error = "ChoosePixelFormat/SetPixelFormat failed"; shutdown(); return false;
    }
    context_ = wglCreateContext(dc_);
    if (!context_ || !wglMakeCurrent(dc_, context_)) {
        if (error) *error = "WGL context creation failed"; shutdown(); return false;
    }
    const auto glText=[](GLenum value) {
        const auto* text=reinterpret_cast<const char*>(glGetString(value));
        return text?std::string(text):std::string("unavailable");
    };
    stats_.glVendor=glText(GL_VENDOR);
    stats_.glRenderer=glText(GL_RENDERER);
    stats_.glVersion=glText(GL_VERSION);
    resourceRoot_ = std::move(sourceRoot);
    textureRoot_ = resourceRoot_ / L"assets" / L"minecraft" / L"textures";
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ownsComInitialization_ = SUCCEEDED(comResult);
    Image grassMap;
    if (!readPng(textureRoot_ / L"colormap" / L"grass.png", grassMap)) {
        lastError_="Could not decode supplied assets/minecraft/textures/colormap/grass.png";
        if (error) *error = lastError_;
        shutdown(); return false;
    }
    // Plains climate from the supplied biome JSON; this is the Java
    // ColorMapColorUtil lookup (rain *= temperature, then truncating indices).
    const UINT mapX=static_cast<UINT>((1.0-0.8)*255.0);
    const UINT mapY=static_cast<UINT>((1.0-0.4*0.8)*255.0);
    if (mapX>=grassMap.width || mapY>=grassMap.height) {
        lastError_="Supplied grass colormap is smaller than the Java 256 by 256 lookup";
        if (error) *error = lastError_;
        shutdown(); return false;
    }
    const auto grassAt=(static_cast<std::size_t>(mapY)*grassMap.width+mapX)*4;
    grassTintR_=grassMap.rgba[grassAt]/255.0f;
    grassTintG_=grassMap.rgba[grassAt+1]/255.0f;
    grassTintB_=grassMap.rgba[grassAt+2]/255.0f;
    Image asciiFont;
    if (!readPng(textureRoot_ / L"font" / L"ascii.png", asciiFont) || asciiFont.width<128 || asciiFont.height<128) {
        lastError_="Could not decode supplied 128 by 128 assets/minecraft/textures/font/ascii.png";
        if (error) *error=lastError_;
        shutdown(); return false;
    }
    glyphWidths_.fill(4);
    for (int code=32;code<128;++code) {
        const int cellX=(code%16)*8, cellY=(code/16)*8;
        int rightmost=-1;
        for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
            const auto at=(static_cast<std::size_t>(cellY+y)*asciiFont.width+cellX+x)*4;
            if (asciiFont.rgba[at+3]!=0) rightmost=std::max(rightmost,x);
        }
        if (rightmost>=0) glyphWidths_[code]=static_cast<std::uint8_t>(rightmost+1);
    }
    RECT clientRect{};
    if (GetClientRect(window_, &clientRect))
        resize(clientRect.right-clientRect.left, clientRect.bottom-clientRect.top);
    else
        resize(width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.48f, 0.70f, 0.93f, 1.0f);
    const std::array<std::pair<std::string,std::string>, 6> sourceStates{{
        {"minecraft:bedrock", ""}, {"minecraft:dirt", ""},
        {"minecraft:grass_block", "snowy=false"}, {"minecraft:stone", ""},
        {"minecraft:stone_slab", "type=bottom"}, {"minecraft:stone_slab", "type=top"}
    }};
    for (const auto& [state,selector]:sourceStates) {
        auto geometry=loadSourceBlockGeometry(resourceRoot_,state,selector);
        if (!geometry) {
            lastError_="Could not compile source block geometry: "+geometry.error;
            if (error) *error=lastError_;
            shutdown(); return false;
        }
        sourceModels_.emplace(geometryKey(state,selector),std::move(geometry));
    }
    // The source double slab state resolves to the original stone model.
    auto doubleSlab=loadSourceBlockGeometry(resourceRoot_,"minecraft:stone_slab","type=double");
    if (!doubleSlab) {
        lastError_="Could not compile source double-slab geometry: "+doubleSlab.error;
        if (error) *error=lastError_;
        shutdown(); return false;
    }
    sourceModels_.emplace(geometryKey("minecraft:stone_slab","type=double"),std::move(doubleSlab));
    return true;
}

void ClientRenderer::resize(int width, int height) {
    width_ = std::max(1, width); height_ = std::max(1, height);
    if (!context_ || !wglMakeCurrent(dc_, context_)) return;
    glViewport(0, 0, width_, height_);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    const double aspect = static_cast<double>(width_) / height_;
    const double nearPlane = 0.05, top = nearPlane * std::tan(70.0 * 3.141592653589793 / 360.0);
    glFrustum(-top * aspect, top * aspect, -top, top, nearPlane, 512.0);
    glMatrixMode(GL_MODELVIEW);
}

void ClientRenderer::render(std::span<const BlockInstance> blocks, const PlayerState& player,
                            const ClientHud& hud, std::uint64_t sceneRevision,
                            std::uint64_t snapshotToken) {
    const auto frameStart=std::chrono::steady_clock::now();
    if (!context_) return;
    if (!wglMakeCurrent(dc_, context_)) {
        if (!loggedContextFailure_) { lastError_="wglMakeCurrent failed during render"; OutputDebugStringW(L"Carbon renderer: wglMakeCurrent failed during render\n"); loggedContextFailure_=true; }
        return;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto view = cameraViewMatrix(player.yaw, player.pitch,
        {player.position.x, player.position.y+player.eyeHeight, player.position.z});
    glMatrixMode(GL_MODELVIEW); glLoadMatrixd(view.data());

    // Use original block texture PNGs. The mappings below are sourced from the
    // supplied blockstates and block models; unsupported model families are skipped.
    auto bind = [&](std::string_view subdir, std::string_view name) {
        const std::string key = std::string(subdir) + "/" + std::string(name);
        auto found = textures_.find(key);
        if (found != textures_.end()) { glBindTexture(GL_TEXTURE_2D, found->second); return true; }
        Image image;
        const auto path=textureRoot_ / std::filesystem::path(subdir) / (std::string(name) + ".png");
        if (!readPng(path, image)) { lastError_="Could not decode source texture: "+path.string(); return false; }
        GLuint id=0; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(image.width),static_cast<GLsizei>(image.height),0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
        textures_.emplace(key,id); return true;
    };

    auto bindResource = [&](std::string_view identifier) {
        const std::string id(identifier);
        const auto colon=id.find(':');
        if (colon==std::string::npos || colon==0 || colon+1>=id.size()) {
            lastError_="Invalid source texture identifier: "+id; return false;
        }
        auto found=textures_.find(id);
        if (found!=textures_.end()) { glBindTexture(GL_TEXTURE_2D,found->second); return true; }
        const std::string nameSpace=id.substr(0,colon), path=id.substr(colon+1);
        const auto file=resourceRoot_ / L"assets" / std::filesystem::path(nameSpace) /
            L"textures" / (std::filesystem::path(path).concat(L".png"));
        Image image;
        if (!readPng(file,image)) { lastError_="Could not decode source texture: "+file.string(); return false; }
        GLuint texture=0; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(image.width),static_cast<GLsizei>(image.height),0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
        textures_.emplace(id,texture); return true;
    };

    const auto blockHash=[](const BlockInstance& block) {
        std::uint64_t h=static_cast<std::uint64_t>(block.block)+1;
        h=mixHash(h,BlockPos::asLong(block.pos.x,block.pos.y,block.pos.z));
        h=mixHash(h,static_cast<std::uint64_t>(block.slabType));
        h=mixHash(h,static_cast<std::uint64_t>(block.stairFacing));
        h=mixHash(h,static_cast<std::uint64_t>(block.stairHalf));
        h=mixHash(h,static_cast<std::uint64_t>(block.stairShape));
        const std::uint64_t fenceBits=(block.fenceConnections.north?1ULL:0ULL)|(block.fenceConnections.east?2ULL:0ULL)|(block.fenceConnections.south?4ULL:0ULL)|(block.fenceConnections.west?8ULL:0ULL);
        h=mixHash(h,fenceBits);
        return h;
    };
    std::uint64_t sceneFingerprint=0x434152424f4eULL;
    if (snapshotToken) {
        sceneFingerprint=mixHash(sceneFingerprint,snapshotToken);
        sceneFingerprint=mixHash(sceneFingerprint,blocks.size());
    } else {
        for (const auto& block:blocks) sceneFingerprint=mixHash(sceneFingerprint,blockHash(block));
    }
    sceneFingerprint=mixHash(sceneFingerprint,sceneRevision);
    const bool sceneChanged=sceneFingerprint!=cachedSceneFingerprint_;
    const bool worldEdit=sceneRevision!=0 && cachedSceneRevision_!=0 && sceneRevision!=cachedSceneRevision_;
    if (worldEdit || (sceneChanged && !snapshotToken)) {
        for (const auto& [key,mesh]:sectionMeshes_) {
            (void)key; for (const auto& [texture,list]:mesh.textureLists) { (void)texture; glDeleteLists(list,1); }
        }
        sectionMeshes_.clear();
    }
    const auto lightIndexStart=std::chrono::steady_clock::now();
    if (!lightingNeighborhood_ || sceneFingerprint!=cachedSceneFingerprint_) {
        lightingNeighborhood_=std::make_unique<SmoothLightNeighborhood>(blocks);
        cachedSceneFingerprint_=sceneFingerprint;
        ++stats_.lightIndexBuilds;
    }
    stats_.lastLightIndexMs=sceneChanged?std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-lightIndexStart).count():0.0;
    if (sceneRevision) cachedSceneRevision_=sceneRevision;
    const auto& lightingNeighborhood=*lightingNeighborhood_;
    stats_.visibleBlocks=blocks.size(); stats_.drawCalls=0; stats_.meshCacheHits=0;
    if (sceneChanged) { stats_.visibleQuads=0; stats_.culledQuads=0; }
    if (sceneChanged) stats_.lastSectionHashMs=stats_.lastMeshPrepareMs=stats_.lastDisplayListBuildMs=0.0;
    auto stateSelector=[](const BlockInstance& block,std::string& state,std::string& selector) {
        switch (block.block) {
        case BlockId::Bedrock: state="minecraft:bedrock"; break;
        case BlockId::Dirt: state="minecraft:dirt"; break;
        case BlockId::GrassBlock: state="minecraft:grass_block"; selector="snowy=false"; break;
        case BlockId::Stone: state="minecraft:stone"; break;
        case BlockId::StoneSlab:
            state="minecraft:stone_slab";
            selector=block.slabType==SlabType::Top?"type=top":block.slabType==SlabType::Double?"type=double":"type=bottom"; break;
        case BlockId::StoneStairs: {
            state="minecraft:stone_stairs";
            const auto facing=directionInfo(block.stairFacing).name;
            const char* half=block.stairHalf==StairHalf::Top?"top":"bottom";
            const char* shape=block.stairShape==StairsShape::Straight?"straight":block.stairShape==StairsShape::InnerLeft?"inner_left":block.stairShape==StairsShape::InnerRight?"inner_right":block.stairShape==StairsShape::OuterLeft?"outer_left":"outer_right";
            selector="facing="+std::string(facing)+",half="+half+",shape="+shape; break;
        }
        case BlockId::OakFence:
            state="minecraft:oak_fence";
            selector="north="+std::string(block.fenceConnections.north?"true":"false")+",east="+(block.fenceConnections.east?"true":"false")+",south="+(block.fenceConnections.south?"true":"false")+",west="+(block.fenceConnections.west?"true":"false"); break;
        default: return false;
        }
        return true;
    };
    auto renderStateHash=[](const BlockInstance& block) {
        std::uint64_t h=static_cast<std::uint64_t>(block.block)+1;
        h=mixHash(h,static_cast<std::uint64_t>(block.slabType));
        h=mixHash(h,static_cast<std::uint64_t>(block.stairFacing));
        h=mixHash(h,static_cast<std::uint64_t>(block.stairHalf));
        h=mixHash(h,static_cast<std::uint64_t>(block.stairShape));
        const std::uint64_t fenceBits=(block.fenceConnections.north?1ULL:0ULL)|(block.fenceConnections.east?2ULL:0ULL)|(block.fenceConnections.south?4ULL:0ULL)|(block.fenceConnections.west?8ULL:0ULL);
        return mixHash(h,fenceBits);
    };
    std::unordered_map<std::uint64_t,bool> supportFaceCache;
    const auto camera=cameraBasis(player.yaw,player.pitch);
    const Vec3 eye{player.position.x,player.position.y+player.eyeHeight,player.position.z};
    const float tanV=std::tan(70.0f*3.14159265358979323846f/360.0f);
    const float tanH=tanV*static_cast<float>(width_)/static_cast<float>(height_);

    if (sceneChanged) {
    struct SectionBuild { int x{},y{},z{}; std::vector<const BlockInstance*> blocks; };
    std::map<std::int64_t,SectionBuild> sections;
    auto sectionCoord=[](int value) { return value>=0?value/16:-((15-value)/16); };
    struct BlockRecord { const BlockInstance* block{}; std::uint64_t fingerprint{}; };
    std::unordered_map<std::int64_t,BlockRecord> blockLookup;
    blockLookup.reserve(blocks.size());
    for (const auto& block:blocks)
        blockLookup.insert_or_assign(BlockPos::asLong(block.pos.x,block.pos.y,block.pos.z),BlockRecord{&block,blockHash(block)});
    std::unordered_map<std::int64_t,int> topOpaqueByColumn;
    topOpaqueByColumn.reserve(blocks.size());
    for (const auto& block:blocks) {
        if (block.block==BlockId::Air || block.block==BlockId::Water || block.block==BlockId::OakFence) continue;
        const auto column=BlockPos::asLong(block.pos.x,0,block.pos.z);
        auto [it,inserted]=topOpaqueByColumn.try_emplace(column,block.pos.y);
        if (!inserted) it->second=std::max(it->second,block.pos.y);
    }
    auto sectionFingerprint=[&](const SectionBuild& section) {
        std::uint64_t h=0x53454354494f4eULL;
        // Hash the exact one-block section halo sampled by cullface and AO
        // once per cell instead of repeating neighbors for every block.
        const int x0=section.x*16,y0=section.y*16,z0=section.z*16;
        for (int x=x0-1;x<=x0+16;++x)
            for (int y=y0-1;y<=y0+16;++y)
                for (int z=z0-1;z<=z0+16;++z) {
                    const auto found=blockLookup.find(BlockPos::asLong(x,y,z));
                    h=mixHash(h,found==blockLookup.end()?0xdeadbeefULL:found->second.fingerprint);
                }
        // Direct sky is a column lookup, including the same one-block XZ halo.
        for (int x=x0-1;x<=x0+16;++x) for (int z=z0-1;z<=z0+16;++z) {
            const auto top=topOpaqueByColumn.find(BlockPos::asLong(x,0,z));
            h=mixHash(h,top==topOpaqueByColumn.end()?0x51a7ULL:static_cast<std::uint64_t>(static_cast<std::int64_t>(top->second)));
        }
        return h;
    };
    for (const auto& block:blocks) {
        if (block.block==BlockId::Air) continue;
        const int sx=sectionCoord(block.pos.x),sy=sectionCoord(block.pos.y),sz=sectionCoord(block.pos.z);
        auto& section=sections[BlockPos::asLong(sx,sy,sz)];
        section.x=sx; section.y=sy; section.z=sz; section.blocks.push_back(&block);
    }

    std::unordered_map<std::int64_t,bool> activeSections;
    activeSections.reserve(sections.size());
    for (auto& [sectionKey,section]:sections) {
        activeSections.emplace(sectionKey,true);
        auto meshIt=sectionMeshes_.find(sectionKey);
        const auto hashStart=std::chrono::steady_clock::now();
        const auto fingerprint=sectionFingerprint(section);
        stats_.lastSectionHashMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-hashStart).count();
        if (meshIt!=sectionMeshes_.end() && meshIt->second.fingerprint==fingerprint) {
            ++stats_.meshCacheHits;
            continue;
        }
        if (meshIt!=sectionMeshes_.end()) {
            for (const auto& [texture,list]:meshIt->second.textureLists) { (void)texture; glDeleteLists(list,1); }
            sectionMeshes_.erase(meshIt);
        }
        const auto prepareStart=std::chrono::steady_clock::now();
        std::map<std::string,std::vector<std::array<std::array<float,9>,4>>,std::less<>> batches;
        for (const auto* block:section.blocks) {
        std::string state,selector;
        if (!stateSelector(*block,state,selector)) continue;
        std::shared_ptr<const SourceModelGeometry> cachedGeometry;
        auto generated=loadSourceBlockGeometry(resourceRoot_,state,selector,block->pos.x,block->pos.y,block->pos.z,&cachedGeometry);
        if (!cachedGeometry && !generated) { lastError_="Could not bake source model at block position: "+generated.error; continue; }
        const auto& geometry=cachedGeometry?*cachedGeometry:generated;
        const float ox=static_cast<float>(block->pos.x),oy=static_cast<float>(block->pos.y),oz=static_cast<float>(block->pos.z);
        for (const auto& q:geometry.quads) {
            ++stats_.visibleQuads;
            if (q.cullFace) {
                const auto face=directionFromName(*q.cullFace); const auto d=step(face);
                const auto neighbor=lightingNeighborhood.find({block->pos.x+d.x,block->pos.y+d.y,block->pos.z+d.z});
                if (neighbor) {
                    const auto supportFace=opposite(face);
                    const auto cacheKey=mixHash(renderStateHash(*neighbor),static_cast<std::uint64_t>(supportFace));
                    auto [support,inserted]=supportFaceCache.try_emplace(cacheKey,false);
                    if (inserted) support->second=fullRenderSupportFace(renderCollisionShape(*neighbor),supportFace);
                    if (support->second) { ++stats_.culledQuads; continue; }
                }
            }
            if (!bindResource(q.texture)) continue;
            const auto lighting=sampleSmoothLighting(q,*block,lightingNeighborhood);
            const float tintR=q.tintIndex>=0?grassTintR_:1.0f;
            const float tintG=q.tintIndex>=0?grassTintG_:1.0f;
            const float tintB=q.tintIndex>=0?grassTintB_:1.0f;
            auto& batch=batches[q.texture];
            std::array<std::array<float,9>,4> vertices{};
            for (std::size_t i=0;i<4;++i) {
                const float light=lighting.vertexBrightness[i];
                vertices[i]={ox+q.positions[i][0],oy+q.positions[i][1],oz+q.positions[i][2],q.uvs[i][0]/16.0f,q.uvs[i][1]/16.0f,tintR*light,tintG*light,tintB*light,1.0f};
            }
            batch.push_back(vertices);
        }
        }
        stats_.lastMeshPrepareMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-prepareStart).count();
        SectionMesh mesh; mesh.fingerprint=fingerprint; mesh.x=section.x; mesh.y=section.y; mesh.z=section.z;
        const auto listBuildStart=std::chrono::steady_clock::now();
        for (auto& [texture,quads]:batches) {
            if (quads.empty() || !bindResource(texture)) continue;
            const GLuint list=glGenLists(1);
            if (!list) { lastError_="OpenGL display-list allocation failed"; break; }
            glNewList(list,GL_COMPILE);
            glBegin(GL_QUADS);
            for (const auto& quad:quads) for (const auto& v:quad) {
                glColor4f(v[5],v[6],v[7],v[8]); glTexCoord2f(v[3],v[4]); glVertex3f(v[0],v[1],v[2]);
            }
            glEnd(); glEndList(); mesh.textureLists.emplace(texture,list);
        }
        stats_.lastDisplayListBuildMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-listBuildStart).count();
        sectionMeshes_.emplace(sectionKey,std::move(mesh)); ++stats_.meshBuilds;
    }
    for (auto it=sectionMeshes_.begin();it!=sectionMeshes_.end();) {
        if (!activeSections.contains(it->first)) {
            for (const auto& [texture,list]:it->second.textureLists) { (void)texture; glDeleteLists(list,1); }
            it=sectionMeshes_.erase(it);
        } else ++it;
    }
    }
    // Frustum-test section bounds conservatively with a bounding sphere. The
    // margin is the section half-diagonal, so no source face inside an
    // intersecting section is discarded.
    const auto worldDrawStart=std::chrono::steady_clock::now();
    for (const auto& [key,mesh]:sectionMeshes_) {
        (void)key;
        const Vec3 center{mesh.x*16.0+8.0,mesh.y*16.0+8.0,mesh.z*16.0+8.0};
        const Vec3 delta=center.subtract(eye); const double radius=13.86;
        const double depth=delta.x*camera.forward.x+delta.y*camera.forward.y+delta.z*camera.forward.z;
        const double horizontal=delta.x*camera.right.x+delta.y*camera.right.y+delta.z*camera.right.z;
        const double vertical=delta.x*camera.up.x+delta.y*camera.up.y+delta.z*camera.up.z;
        if (depth+radius<0.05 || depth-radius>512.0 || std::abs(horizontal)>std::max(0.0,depth)*tanH+radius*1.42 || std::abs(vertical)>std::max(0.0,depth)*tanV+radius*1.42) continue;
        ++stats_.meshCacheHits;
        for (const auto& [texture,list]:mesh.textureLists) {
            const auto tex=textures_.find(texture);
            if (tex!=textures_.end()) glBindTexture(GL_TEXTURE_2D,tex->second);
            glCallList(list); ++stats_.drawCalls;
        }
    }
    stats_.lastWorldDrawMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-worldDrawStart).count();

    // Original HUD sprite textures from the supplied resource pack.
    const GLboolean depthWasEnabled=glIsEnabled(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0,width_,height_,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity(); glDisable(GL_DEPTH_TEST);
    const auto hudTexture=[&](std::string_view name,float x,float y,float w,float h) {
        if (!bind("gui/sprites/hud",name)) return;
        glColor4f(1,1,1,1); glBegin(GL_QUADS);
        glTexCoord2f(0,0);glVertex2f(x,y); glTexCoord2f(1,0);glVertex2f(x+w,y);
        glTexCoord2f(1,1);glVertex2f(x+w,y+h); glTexCoord2f(0,1);glVertex2f(x,y+h); glEnd();
    };
    const int slot=std::clamp(hud.hotbar.selectedSlot,0,static_cast<int>(hud.hotbar.slots.size())-1);
    const int hotbarGuiScale=sourceAutoGuiScale(width_,height_);
    const auto hotbarLayout=sourceHotbarLayout(width_,height_,hotbarGuiScale,slot);
    const float itemScale=static_cast<float>(hotbarGuiScale);
    const float scale=std::max(1.0f,std::min(width_/320.0f,height_/180.0f));
    hudTexture("hotbar",static_cast<float>(hotbarLayout.background.x),
               static_cast<float>(hotbarLayout.background.y),
               static_cast<float>(hotbarLayout.background.width),
               static_cast<float>(hotbarLayout.background.height));
    hudTexture("hotbar_selection",static_cast<float>(hotbarLayout.selection.x),
               static_cast<float>(hotbarLayout.selection.y),
               static_cast<float>(hotbarLayout.selection.width),
               static_cast<float>(hotbarLayout.selection.height));
    for (std::size_t i=0;i<hud.hotbar.slots.size();++i) {
        const auto itemId=guiItemId(hud.hotbar.slots[i]);
        if (itemId.empty()) continue;
        auto itemIt=guiItemModels_.find(itemId);
        if (itemIt==guiItemModels_.end()) {
            itemIt=guiItemModels_.emplace(itemId,loadSourceGuiItemModel(resourceRoot_,itemId)).first;
            if (!itemIt->second) { lastError_="Could not load source GUI item model: "+itemIt->second.error; continue; }
        }
        const auto& item=itemIt->second;
        const std::string geometryKey="gui-model|"+item.model;
        auto modelIt=sourceModels_.find(geometryKey);
        if (modelIt==sourceModels_.end()) {
            auto geometry=loadSourceModelGeometry(resourceRoot_,item.model);
            if (!geometry) { lastError_="Could not bake source GUI item geometry: "+geometry.error; continue; }
            modelIt=sourceModels_.emplace(geometryKey,std::move(geometry)).first;
        }

        struct GuiQuad { const SourceModelQuad* quad; std::array<std::array<float,3>,4> points; float depth; float shade; };
        std::vector<GuiQuad> drawQuads;
        drawQuads.reserve(modelIt->second.quads.size());
        const std::array<float,3> light{0.25f,0.55f,0.80f};
        for (const auto& quad:modelIt->second.quads) {
            GuiQuad draw{&quad,{},0.0f,1.0f};
            const auto transformedNormal=transformGuiItemNormal(quad.normal,item.gui);
            const float nx=transformedNormal[0],ny=transformedNormal[1],nz=transformedNormal[2];
            const float normalLength=std::sqrt(nx*nx+ny*ny+nz*nz);
            const float facing=normalLength>0.0f?(nx*light[0]+ny*light[1]+nz*light[2])/normalLength:0.0f;
            // Java's side-lit item GUI uses directional face lighting. This
            // native slice applies a stable diffuse approximation per face.
            draw.shade=std::clamp(0.72f+0.28f*facing,0.44f,1.0f);
            float depth=0.0f;
            for (std::size_t corner=0;corner<4;++corner) {
                draw.points[corner]=transformGuiItemPoint(quad.positions[corner],item.gui);
                depth+=draw.points[corner][2]*0.25f;
            }
            draw.depth=depth;
            drawQuads.push_back(draw);
        }
        // Painter order keeps the 3D mesh legible while the HUD depth buffer is off.
        std::stable_sort(drawQuads.begin(),drawQuads.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
        const auto& itemCell=hotbarLayout.itemCells[i];
        const float cx=static_cast<float>(itemCell.x)+static_cast<float>(itemCell.width)*0.5f;
        const float cy=static_cast<float>(itemCell.y)+static_cast<float>(itemCell.height)*0.5f;
        for (const auto& draw:drawQuads) {
            if (!bindResource(draw.quad->texture)) continue;
            const float tintR=draw.quad->tintIndex>=0?grassTintR_:1.0f;
            const float tintG=draw.quad->tintIndex>=0?grassTintG_:1.0f;
            const float tintB=draw.quad->tintIndex>=0?grassTintB_:1.0f;
            glColor4f(tintR*draw.shade,tintG*draw.shade,tintB*draw.shade,1.0f);
            glBegin(GL_QUADS);
            for (std::size_t corner=0;corner<4;++corner) {
                glTexCoord2f(draw.quad->uvs[corner][0]/16.0f,draw.quad->uvs[corner][1]/16.0f);
                glVertex3f(cx+draw.points[corner][0]*16.0f*itemScale,
                           cy-draw.points[corner][1]*16.0f*itemScale,
                           draw.points[corner][2]*0.5f);
            }
            glEnd();
        }
    }
    hudTexture("crosshair",(width_-15*scale)*0.5f,(height_-15*scale)*0.5f,15*scale,15*scale);
    if (!hud.status.empty() && bind("font","ascii")) {
        glColor4f(1,1,1,1);
        float textX=8*scale;
        const float textY=8*scale, glyphHeight=8*scale;
        for (const unsigned char raw : hud.status) {
            const unsigned char code=(raw>=32 && raw<128) ? raw : static_cast<unsigned char>('?');
            const float glyphWidth=static_cast<float>(glyphWidths_[code])*scale;
            if (code!=' ' && glyphWidth>0) {
                const float u0=static_cast<float>((code%16)*8)/128.0f;
                const float v0=static_cast<float>((code/16)*8)/128.0f;
                const float u1=u0+static_cast<float>(glyphWidths_[code])/128.0f;
                const float v1=v0+8.0f/128.0f;
                glBegin(GL_QUADS);
                glTexCoord2f(u0,v0); glVertex2f(textX,textY);
                glTexCoord2f(u1,v0); glVertex2f(textX+glyphWidth,textY);
                glTexCoord2f(u1,v1); glVertex2f(textX+glyphWidth,textY+glyphHeight);
                glTexCoord2f(u0,v1); glVertex2f(textX,textY+glyphHeight);
                glEnd();
            }
            textX+=glyphWidth;
        }
    }
    if(depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    const GLenum glError=glGetError();
    if (glError!=GL_NO_ERROR && !loggedGlFailure_) {
        lastError_="OpenGL rendering error";
        wchar_t message[128]{}; wsprintfW(message,L"Carbon renderer: OpenGL error 0x%04X\n",static_cast<unsigned>(glError));
        OutputDebugStringW(message); loggedGlFailure_=true;
    }
    if (!SwapBuffers(dc_) && !loggedSwapFailure_) {
        lastError_="SwapBuffers failed";
        wchar_t message[128]{}; wsprintfW(message,L"Carbon renderer: SwapBuffers failed (%lu)\n",GetLastError());
        OutputDebugStringW(message); loggedSwapFailure_=true;
    } else if (!loggedSwapFailure_) ++presentedFrames_;
    ++stats_.frames;
    stats_.lastCpuFrameMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count();
}

void ClientRenderer::renderMenu(std::string_view title, std::span<const MenuButton> buttons,
                                bool titleScreen, std::string_view subtitle) {
    if (!context_ || !dc_) return;
    wglMakeCurrent(dc_, context_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, width_, height_, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    auto menuBind = [&](std::string_view identifier) {
        const std::string id(identifier);
        auto found=textures_.find(id);
        if(found==textures_.end() && id=="minecraft:font/ascii") found=textures_.find("font/ascii");
        if(found!=textures_.end()) { glBindTexture(GL_TEXTURE_2D,found->second); return true; }
        const auto colon=id.find(':');
        if(colon==std::string::npos) return false;
        const auto file=resourceRoot_ / L"assets" / std::filesystem::path(id.substr(0,colon)) / L"textures" /
            std::filesystem::path(id.substr(colon+1)).concat(L".png");
        Image image; if(!readPng(file,image)) { lastError_="Could not decode menu texture: "+file.string(); return false; }
        GLuint texture=0; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(image.width),static_cast<GLsizei>(image.height),0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
        textures_.emplace(id,texture); return true;
    };
    auto drawTexture = [&](std::string_view id, int x, int y, int w, int h, float alpha = 1.0f) {
        if (!menuBind(id)) return;
        glColor4f(1, 1, 1, alpha); glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(static_cast<float>(x),static_cast<float>(y));
        glTexCoord2f(1,0); glVertex2f(static_cast<float>(x+w),static_cast<float>(y));
        glTexCoord2f(1,1); glVertex2f(static_cast<float>(x+w),static_cast<float>(y+h));
        glTexCoord2f(0,1); glVertex2f(static_cast<float>(x),static_cast<float>(y+h));
        glEnd();
    };
    const int uiScale = std::max(1, std::min(width_ / 320, height_ / 180));
    if (titleScreen) {
        // The supplied CubeMapTexture uses CubeMap's face order and a view
        // looking into an animated six-sided panorama. Render the same six
        // original images inside a cube, with a slowly rotating title view.
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        const double nearPlane=0.05, top=nearPlane*std::tan(85.0*3.14159265358979323846/360.0), aspect=static_cast<double>(width_)/height_;
        glFrustum(-top*aspect,top*aspect,-top,top,nearPlane,10.0);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glRotatef(180.0f,1,0,0); glRotatef(panoramaRotation_,0,1,0);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
        struct Face { const char* image; std::array<std::array<float,3>,4> p; };
        const std::array<Face,6> faces{{
            {"panorama_1", {{{-1,-1,1},{-1,1,1},{1,1,1},{1,-1,1}}}},
            {"panorama_3", {{{1,-1,1},{1,1,1},{1,1,-1},{1,-1,-1}}}},
            {"panorama_5", {{{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,-1}}}},
            {"panorama_4", {{{-1,-1,-1},{-1,1,-1},{-1,1,1},{-1,-1,1}}}},
            {"panorama_0", {{{-1,-1,-1},{-1,-1,1},{1,-1,1},{1,-1,-1}}}},
            {"panorama_2", {{{-1,1,1},{-1,1,-1},{1,1,-1},{1,1,1}}}}
        }};
        for(const auto& face:faces) {
            if(!menuBind(std::string("minecraft:gui/title/background/")+face.image)) continue;
            glColor4f(1,1,1,1); glBegin(GL_QUADS);
            const std::array<std::array<float,2>,4> uv{{{{0,1}},{{0,0}},{{1,0}},{{1,1}}}};
            for(std::size_t i=0;i<4;++i) { glTexCoord2f(uv[i][0],uv[i][1]); glVertex3f(face.p[i][0],face.p[i][1],face.p[i][2]); }
            glEnd();
        }
        glLoadIdentity(); glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,width_,height_,0,-1,1); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        panoramaRotation_+=0.012f; if(panoramaRotation_>360.0f) panoramaRotation_-=360.0f;
        drawTexture("minecraft:gui/title/background/panorama_overlay", 0, 0, width_, height_, 0.72f);
        const int logoX=(width_-256*uiScale)/2, logoY=30*uiScale;
        if(menuBind("minecraft:gui/title/minecraft")) {
            glColor4f(1,1,1,1); glBegin(GL_QUADS);
            glTexCoord2f(0,0);glVertex2f(static_cast<float>(logoX),static_cast<float>(logoY));
            glTexCoord2f(1,0);glVertex2f(static_cast<float>(logoX+256*uiScale),static_cast<float>(logoY));
            glTexCoord2f(1,44.0f/64.0f);glVertex2f(static_cast<float>(logoX+256*uiScale),static_cast<float>(logoY+44*uiScale));
            glTexCoord2f(0,44.0f/64.0f);glVertex2f(static_cast<float>(logoX),static_cast<float>(logoY+44*uiScale)); glEnd();
        }
        const int editionX=(width_-128*uiScale)/2, editionY=logoY+37*uiScale;
        if(menuBind("minecraft:gui/title/edition")) {
            glColor4f(1,1,1,1); glBegin(GL_QUADS);
            glTexCoord2f(0,0);glVertex2f(static_cast<float>(editionX),static_cast<float>(editionY));
            glTexCoord2f(1,0);glVertex2f(static_cast<float>(editionX+128*uiScale),static_cast<float>(editionY));
            glTexCoord2f(1,14.0f/16.0f);glVertex2f(static_cast<float>(editionX+128*uiScale),static_cast<float>(editionY+14*uiScale));
            glTexCoord2f(0,14.0f/16.0f);glVertex2f(static_cast<float>(editionX),static_cast<float>(editionY+14*uiScale)); glEnd();
        }
    } else {
        drawTexture("minecraft:gui/menu_background", 0, 0, width_, height_);
        glDisable(GL_TEXTURE_2D);
        glColor4f(0,0,0,0.58f); glBegin(GL_QUADS);
        glVertex2f(0,0); glVertex2f(static_cast<float>(width_),0);
        glVertex2f(static_cast<float>(width_),static_cast<float>(height_)); glVertex2f(0,static_cast<float>(height_)); glEnd();
        glEnable(GL_TEXTURE_2D);
    }
    auto drawText = [&](std::string_view text, float x, float y, float scale, bool centered) {
        if (!menuBind("minecraft:font/ascii")) return;
        float total=0;
        for (const unsigned char c:text) total += glyphWidths_[(c>=32&&c<128)?c:'?']*scale;
        if (centered) x-=total*0.5f;
        glColor4f(1,1,1,1);
        for (const unsigned char raw:text) {
            const unsigned char c=(raw>=32&&raw<128)?raw:static_cast<unsigned char>('?');
            const float gw=static_cast<float>(glyphWidths_[c])*scale;
            if (c!=' '&&gw>0) {
                const float u0=static_cast<float>((c%16)*8)/128.0f, v0=static_cast<float>((c/16)*8)/128.0f;
                const float u1=u0+static_cast<float>(glyphWidths_[c])/128.0f, v1=v0+8.0f/128.0f;
                glBegin(GL_QUADS);
                glTexCoord2f(u0,v0); glVertex2f(x,y); glTexCoord2f(u1,v0); glVertex2f(x+gw,y);
                glTexCoord2f(u1,v1); glVertex2f(x+gw,y+8*scale); glTexCoord2f(u0,v1); glVertex2f(x,y+8*scale);
                glEnd();
            }
            x+=gw;
        }
    };
    if (!titleScreen) drawText(title, width_*0.5f, 12.0f*uiScale, 2.0f*uiScale, true);
    for (const auto& b:buttons) {
        const std::string_view sprite = b.disabled ? "minecraft:gui/sprites/widget/button_disabled" :
            (b.hovered ? "minecraft:gui/sprites/widget/button_highlighted" : "minecraft:gui/sprites/widget/button");
        drawTexture(sprite, b.x, b.y, b.width, b.height);
        drawText(b.label, b.x+b.width*0.5f, b.y+(b.height-8*uiScale)*0.5f, static_cast<float>(uiScale), true);
    }
    if (!subtitle.empty()) drawText(subtitle, width_*0.5f, height_-24.0f*uiScale, static_cast<float>(uiScale), true);
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
    const GLenum glError=glGetError();
    if(glError!=GL_NO_ERROR) lastError_="OpenGL menu rendering error";
    if (!SwapBuffers(dc_)) lastError_="SwapBuffers failed while rendering menu";
}

bool ClientRenderer::writeScreenshot(const std::filesystem::path& path, std::string* error) {
    if (!context_ || !wglMakeCurrent(dc_,context_)) {
        if (error) *error="Renderer has no current WGL context";
        return false;
    }
    GLint oldReadBuffer=GL_BACK;
    glGetIntegerv(GL_READ_BUFFER,&oldReadBuffer);
    glReadBuffer(GL_FRONT);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width_)*height_*3);
    glReadPixels(0,0,width_,height_,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    glReadBuffer(static_cast<GLenum>(oldReadBuffer));
    if (const auto glError=glGetError(); glError!=GL_NO_ERROR) {
        if (error) *error="glReadPixels failed with OpenGL error "+std::to_string(glError);
        return false;
    }
    std::ofstream output(path,std::ios::binary);
    if (!output) { if (error) *error="Could not open screenshot path"; return false; }
    output << "P6\n" << width_ << ' ' << height_ << "\n255\n";
    const std::size_t row=static_cast<std::size_t>(width_)*3;
    for (int y=height_-1;y>=0;--y)
        output.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*row),static_cast<std::streamsize>(row));
    if (!output) { if (error) *error="Failed while writing screenshot"; return false; }
    return true;
}

void ClientRenderer::shutdown() {
    if (context_) {
        wglMakeCurrent(dc_,context_);
        for (const auto& [key,mesh]:sectionMeshes_) {
            (void)key;
            for (const auto& [texture,list]:mesh.textureLists) { (void)texture; glDeleteLists(list,1); }
        }
        sectionMeshes_.clear();
        for (const auto& [key,id]:textures_) { (void)key; const GLuint tex=id; glDeleteTextures(1,&tex); }
        textures_.clear();
        wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context_); context_=nullptr;
    }
    if (dc_ && window_) { ReleaseDC(window_,dc_); dc_=nullptr; }
    if (ownsComInitialization_) { CoUninitialize(); ownsComInitialization_=false; }
    window_=nullptr;
    sourceModels_.clear();
    lightingNeighborhood_.reset();
    cachedSceneFingerprint_=0;
    cachedSceneRevision_=0;
    guiItemModels_.clear();
    resourceRoot_.clear();
    textureRoot_.clear();
}

} // namespace carbon


