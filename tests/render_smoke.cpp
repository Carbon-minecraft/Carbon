#include "carbon/ClientRenderer.hpp"
#include "carbon/PlayableWorld.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL/gl.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <vector>

namespace {
constexpr wchar_t kClassName[] = L"CarbonRendererSmokeWindow";
LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(hwnd,msg,wp,lp);
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct PpmImage { int width{}, height{}; std::vector<unsigned char> rgb; };
PpmImage readPpm(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    std::string magic;
    PpmImage image;
    int maximum{};
    input >> magic >> image.width >> image.height >> maximum;
    input.get();
    if (!input || magic!="P6" || maximum!=255 || image.width<=0 || image.height<=0)
        throw std::runtime_error("could not read smoke PPM: "+path.string());
    image.rgb.resize(static_cast<std::size_t>(image.width)*image.height*3);
    input.read(reinterpret_cast<char*>(image.rgb.data()),static_cast<std::streamsize>(image.rgb.size()));
    if (!input) throw std::runtime_error("incomplete smoke PPM: "+path.string());
    return image;
}
}

int wmain(int argc, wchar_t** argv) try {
    if (argc < 2 || argc > 3) {
        std::wcerr << L"Usage: render_smoke <JavaMinecraft/decompiled> [screenshot.ppm]\n";
        return 2;
    }
    const std::filesystem::path sourceRoot=argv[1];
    const std::filesystem::path screenshot=argc==3 ? argv[2] : L"build/render-smoke.ppm";
    WNDCLASSEXW klass{};
    klass.cbSize=sizeof(klass); klass.lpfnWndProc=windowProc;
    klass.hInstance=GetModuleHandleW(nullptr); klass.lpszClassName=kClassName;
    require(RegisterClassExW(&klass)!=0,"RegisterClassExW failed");
    HWND hwnd=CreateWindowExW(0,kClassName,L"Carbon renderer smoke",WS_POPUP,
        CW_USEDEFAULT,CW_USEDEFAULT,960,540,nullptr,nullptr,klass.hInstance,nullptr);
    require(hwnd!=nullptr,"CreateWindowExW failed");

    int result=0;
    {
        carbon::ClientRenderer renderer;
        std::string error;
        if (!renderer.initialize(hwnd,960,540,sourceRoot,&error))
            throw std::runtime_error("renderer.initialize: "+error);
        // Reproduce the normal menu-to-world transition: renderMenu leaves a
        // black clear color and 2D projection that the world pass must replace.
        renderer.renderMenu("Sky state transition",{},false);
        auto world=carbon::PlayableWorld::createDefaultFlat();
        // Add source-backed cubes, slabs, stairs, and connected fences to
        // exercise original blockstates plus CLASSIC_FLAT terrain.
        world.setBlock({2,-61,5},carbon::BlockId::Stone);
        // A stepped, placed-block cluster puts opaque neighbors above and
        // beside model corners so smooth world lighting has visible gradients.
        world.setBlock({0,-60,3},carbon::BlockId::Stone);
        world.setBlock({1,-60,3},carbon::BlockId::Stone);
        world.setBlock({1,-59,3},carbon::BlockId::Stone);
        world.setSlab({-1,-60,3},carbon::SlabType::Bottom);
        world.setSlab({3,-61,5},carbon::SlabType::Bottom);
        world.setSlab({4,-61,5},carbon::SlabType::Top);
        world.setSlab({5,-61,5},carbon::SlabType::Double);
        world.setStairs({-4,-61,7},carbon::Direction::North,carbon::StairHalf::Bottom,carbon::StairsShape::Straight);
        world.setStairs({-2,-61,7},carbon::Direction::East,carbon::StairHalf::Bottom,carbon::StairsShape::InnerLeft);
        world.setStairs({0,-61,7},carbon::Direction::South,carbon::StairHalf::Top,carbon::StairsShape::InnerRight);
        world.setStairs({2,-61,7},carbon::Direction::West,carbon::StairHalf::Bottom,carbon::StairsShape::OuterLeft);
        world.setStairs({4,-61,7},carbon::Direction::North,carbon::StairHalf::Top,carbon::StairsShape::OuterRight);
        world.setFence({-5,-61,3});
        world.setFence({-4,-61,3});
        world.setFence({-4,-61,4});
        const auto center=carbon::BlockPos(0,-60,0);
        const auto blocks=world.blocksInRange(center,24);
        carbon::ClientHud hud;
        hud.hotbar.slots[8]=carbon::BlockId::StoneStairs;
        hud.hotbar.slots[7]=carbon::BlockId::OakFence;
        hud.hotbar.selectedSlot=8;
        hud.status="Native WGL texture smoke";
        // The HUD must restore caller depth state after its 2D/3D item pass.
        glDisable(GL_DEPTH_TEST);
        const auto firstStart=std::chrono::steady_clock::now();
        const auto worldRevision=world.sceneRevision();
        renderer.render(blocks,world.player,hud,worldRevision,1);
        const double firstBuildMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-firstStart).count();
        require(glIsEnabled(GL_DEPTH_TEST)==GL_FALSE,"HUD did not restore disabled depth testing");
        glEnable(GL_DEPTH_TEST);
        for (int frame=0;frame<8;++frame) {
            MSG message{};
            while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            renderer.render(blocks,world.player,hud,worldRevision,1);
        }
        std::vector<double> frameMs; frameMs.reserve(40);
        for (int frame=0;frame<40;++frame) {
            MSG message{};
            while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            renderer.render(blocks,world.player,hud,worldRevision,1);
            frameMs.push_back(renderer.performanceStats().lastCpuFrameMs);
        }
        auto sortedMs=frameMs; std::sort(sortedMs.begin(),sortedMs.end());
        const double p50=sortedMs[sortedMs.size()/2];
        const double p95=sortedMs[static_cast<std::size_t>(std::ceil(sortedMs.size()*0.95))-1];
        std::vector<double> movingMs; movingMs.reserve(20);
        const float initialYaw=world.player.yaw;
        for (int frame=0;frame<20;++frame) {
            world.player.yaw=initialYaw+static_cast<float>(frame*3);
            renderer.render(blocks,world.player,hud,worldRevision,1);
            movingMs.push_back(renderer.performanceStats().lastCpuFrameMs);
        }
        world.player.yaw=initialYaw;
        renderer.render(blocks,world.player,hud,worldRevision,1);
        std::sort(movingMs.begin(),movingMs.end());
        const double movingP50=movingMs[movingMs.size()/2];
        const double movingP95=movingMs[static_cast<std::size_t>(std::ceil(movingMs.size()*0.95))-1];
        std::vector<double> sectionMovementMs; sectionMovementMs.reserve(15);
        const carbon::BlockPos firstWindowCenter(8,-56,8);
        const auto windowRangeStart=std::chrono::steady_clock::now();
        const auto windowBlocks=world.blocksInSectionWindow(firstWindowCenter,2,32);
        const double windowExtractMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-windowRangeStart).count();
        const auto windowBuildStart=std::chrono::steady_clock::now();
        renderer.render(windowBlocks,world.player,hud,worldRevision,100);
        const double windowColdBuildMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-windowBuildStart).count();
        for (int dx=1;dx<=15;++dx) {
            world.player.position.x=0.5+dx;
            const auto moveStart=std::chrono::steady_clock::now();
            renderer.render(windowBlocks,world.player,hud,worldRevision,100);
            sectionMovementMs.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-moveStart).count());
        }
        world.player.position.x=16.5;
        const carbon::BlockPos nextWindowCenter(24,-56,8);
        const auto boundaryRangeStart=std::chrono::steady_clock::now();
        const auto nextWindowBlocks=world.blocksInSectionWindow(nextWindowCenter,2,32);
        const double boundaryExtractMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-boundaryRangeStart).count();
        const auto boundaryBuildStart=std::chrono::steady_clock::now();
        renderer.render(nextWindowBlocks,world.player,hud,worldRevision,101);
        const double boundaryBuildMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-boundaryBuildStart).count();
        const auto boundaryStats=renderer.performanceStats();
        world.player.position.x=0.5;
        const auto restoredBlocks=world.blocksInRange(center,24);
        renderer.render(restoredBlocks,world.player,hud,worldRevision,1);
        std::sort(sectionMovementMs.begin(),sectionMovementMs.end());
        const double sectionMoveP50=sectionMovementMs[sectionMovementMs.size()/2];
        const double sectionMoveP95=sectionMovementMs[static_cast<std::size_t>(std::ceil(sectionMovementMs.size()*0.95))-1];
        require(renderer.presentedFrames()==88,"not all 88 benchmark SwapBuffers calls succeeded: "+std::to_string(renderer.presentedFrames()));
        require(renderer.loadedTextureCount()>=10,"source block/HUD/font textures did not all load: "+std::to_string(renderer.loadedTextureCount()));
        require(renderer.lastError().empty(),"renderer reported: "+renderer.lastError());
        require(renderer.writeScreenshot(screenshot,&error),"writeScreenshot: "+error);
        const auto bytes=std::filesystem::file_size(screenshot);
        require(bytes>960ULL*540ULL*3ULL,"framebuffer screenshot is incomplete: "+std::to_string(bytes)+" bytes");
        carbon::ClientHud emptyItems=hud;
        emptyItems.hotbar.slots.fill(carbon::BlockId::Air);
        renderer.render(restoredBlocks,world.player,emptyItems,worldRevision,1);
        auto noItemsPath=screenshot;
        noItemsPath.replace_filename(screenshot.stem().string()+"-empty"+screenshot.extension().string());
        require(renderer.writeScreenshot(noItemsPath,&error),"write empty-item screenshot: "+error);
        const auto withItems=readPpm(screenshot);
        const auto withoutItems=readPpm(noItemsPath);
        require(withItems.width==withoutItems.width && withItems.height==withoutItems.height,
                "HUD comparison screenshots have different dimensions");
        const auto skyPixel=(static_cast<std::size_t>(20)*withItems.width+480u)*3u;
        require(withItems.rgb[skyPixel+2]>withItems.rgb[skyPixel],
                "menu-to-world transition left the daytime sky black or failed to render the source-blue sky disc");
        const auto samePixel=[&](int x,int y) {
            const auto at=(static_cast<std::size_t>(y)*withItems.width+static_cast<std::size_t>(x))*3;
            return withItems.rgb[at]==withoutItems.rgb[at] && withItems.rgb[at+1]==withoutItems.rgb[at+1] &&
                   withItems.rgb[at+2]==withoutItems.rgb[at+2];
        };
        for(int y=105;y<215;++y) for(int x=100;x<300;++x)
            require(samePixel(x,y),"hotbar icon rendering changed pixels in the world view");
        int crosshairColors=0;
        unsigned char previous[3]{255,255,255};
        bool hasPrevious=false;
        for(int y=262;y<278;++y) for(int x=472;x<488;++x) {
            require(samePixel(x,y),"hotbar icon rendering changed the crosshair view");
            const auto at=(static_cast<std::size_t>(y)*withItems.width+static_cast<std::size_t>(x))*3;
            if(!hasPrevious || previous[0]!=withItems.rgb[at] || previous[1]!=withItems.rgb[at+1] || previous[2]!=withItems.rgb[at+2]) {
                ++crosshairColors;
                previous[0]=withItems.rgb[at]; previous[1]=withItems.rgb[at+1]; previous[2]=withItems.rgb[at+2];
                hasPrevious=true;
            }
        }
        require(crosshairColors>1,"crosshair did not appear in the WGL smoke frame");
        const auto oldLightBuilds=renderer.performanceStats().lightIndexBuilds;
        const auto oldMeshBuilds=renderer.performanceStats().meshBuilds;
        world.setBlock({6,-60,3},carbon::BlockId::Stone);
        auto roofed=world.blocksInRange(center,24);
        renderer.render(roofed,world.player,hud,world.sceneRevision(),2);
        require(renderer.performanceStats().lightIndexBuilds==oldLightBuilds+1,"world edit did not rebuild the cached lighting index");
        require(renderer.performanceStats().meshBuilds>oldMeshBuilds,"world edit did not rebuild section meshes");
        world.setBlock({6,-60,3},carbon::BlockId::Air);
        auto afterBreak=world.blocksInRange(center,24);
        renderer.render(afterBreak,world.player,hud,world.sceneRevision(),3);
        require(renderer.performanceStats().lightIndexBuilds==oldLightBuilds+2,"block removal did not rebuild the cached lighting index");
        const auto stats=renderer.performanceStats();
        std::cout << std::fixed << std::setprecision(3)
                  << "GL vendor: " << stats.glVendor << "\nGL renderer: " << stats.glRenderer
                  << "\nGL version: " << stats.glVersion
                  << "\nBenchmark: radius 24, 960x540, 8 warmup + 40 measured fixed-scene frames (CPU render through SwapBuffers)"
                  << "\nFirst scene build frame ms: " << firstBuildMs
                  << "\nCPU frame p50/p95 ms: " << p50 << " / " << p95
                  << "\nCamera-turn CPU frame p50/p95 ms: " << movingP50 << " / " << movingP95
                  << "\nChunk-window cold extraction/build ms (5x5 sections, aligned center): " << windowExtractMs << " / " << windowColdBuildMs
                  << "\nWithin-window camera movement CPU p50/p95 ms: " << sectionMoveP50 << " / " << sectionMoveP95
                  << "\nChunk-boundary extraction/build ms: " << boundaryExtractMs << " / " << boundaryBuildMs
                  << "\n  lighting index / section hash / mesh prepare / display-list build / world draw ms: "
                  << boundaryStats.lastLightIndexMs << " / " << boundaryStats.lastSectionHashMs << " / "
                  << boundaryStats.lastMeshPrepareMs << " / " << boundaryStats.lastDisplayListBuildMs << " / "
                  << boundaryStats.lastWorldDrawMs
                  << "\nVisible blocks/quads: " << stats.visibleBlocks << " / " << stats.visibleQuads
                  << "\nCulled quads: " << stats.culledQuads << "\nWorld draw calls: " << stats.drawCalls
                  << "\nSection builds/cache hits: " << stats.meshBuilds << " / " << stats.meshCacheHits
                  << "\nLighting index builds: " << stats.lightIndexBuilds
                  << "\nWGL presents: " << renderer.presentedFrames()
                  << "\nSource textures cached: " << renderer.loadedTextureCount()
                  << "\n3D icon/world/crosshair checks: passed"
                  << "\nFramebuffer PPM: " << std::filesystem::absolute(screenshot).string()
                  << " (" << bytes << " bytes)\n";
        renderer.shutdown();
    }
    DestroyWindow(hwnd);
    UnregisterClassW(kClassName,GetModuleHandleW(nullptr));
    return result;
} catch (const std::exception& error) {
    std::cerr << "render_smoke: " << error.what() << '\n';
    return 1;
}


