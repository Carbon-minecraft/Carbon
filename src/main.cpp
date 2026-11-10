#include "carbon/ClientRenderer.hpp"
#include "carbon/ClientControls.hpp"
#include "carbon/DirectoryResources.hpp"
#include "carbon/LegacyRandomSource.hpp"
#include "carbon/PlayableWorld.hpp"
#include "carbon/SeededOverworldChunkProvider.hpp"
#include "carbon/Multiplayer.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>
#include <stdexcept>

#ifndef CARBON_SOURCE_ROOT
#define CARBON_SOURCE_ROOT "../JavaMinecraft/decompiled"
#endif

namespace fs = std::filesystem;
namespace {
constexpr wchar_t kWindowClass[] = L"CarbonNativeClient26_3";
constexpr int kWidth = 1280, kHeight = 720;
constexpr int kPlayerViewRadius = 24;
constexpr int kSectionWindowRadius = kPlayerViewRadius + 8;
constexpr double kTickSeconds = 1.0 / 20.0;

int sectionAnchor(int coordinate) {
    const int section=coordinate>=0?coordinate/16:-((15-coordinate)/16);
    return section*16+8;
}

struct ClientApp {
    HWND window{};
    carbon::ClientRenderer renderer;
    enum class Screen { Title, Worlds, CreateWorld, Loading, Multiplayer, Paused, Playing } screen{Screen::Title};
    carbon::PlayableWorld world = carbon::PlayableWorld::createDefaultFlat();
    carbon::MultiplayerClient multiplayer;
    struct Session { std::string name, seedText; std::int64_t seed{}; bool numericSeed{}; };
    std::optional<Session> session;
    std::optional<Session> pendingSession;
    std::future<std::shared_ptr<const carbon::SeededOverworldChunkProvider>> terrainLoad;
    std::shared_ptr<std::atomic<std::size_t>> terrainProgress;
    std::shared_ptr<std::atomic<int>> terrainLoadStage;
    std::shared_ptr<std::atomic<bool>> terrainCancel;
    std::future<void> terrainStream;
    std::shared_ptr<std::atomic<std::size_t>> terrainStreamProgress;
    std::shared_ptr<std::atomic<bool>> terrainStreamCancel;
    std::string worldName{"New World"}, seedText, serverAddress{"localhost"}, username{"Player"}, menuMessage;
    int inputField{}; // Create World: 0 name / 1 seed; Multiplayer: 2 address / 3 username
    POINT mouse{};
    std::vector<carbon::BlockInstance> visibleBlocks;
    carbon::BlockPos visibleCenter{std::numeric_limits<int>::min(),0,0};
    int visibleWindowRadius{-1};
    std::uint64_t visibleRevision{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t visibleSnapshotToken{};
    carbon::ClientHud hud;
    std::string_view currentBiome{"minecraft:plains"};
    fs::path sourceRoot;
    bool focused{true};
    bool paused{};
    bool running{true};
    bool leftDown{}, rightDown{}, leftPressed{}, rightPressed{}, pickPressed{};
    carbon::CreativeBreakDelay destroyDelay;
    int rightClickDelay{};
    POINT rawDelta{};
    std::uint64_t dayTime{};
    std::string error;
};

ClientApp* appFrom(HWND hwnd) { return reinterpret_cast<ClientApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)); }
void setCursorCapture(ClientApp& app, bool capture);

int menuScale(int width,int height) { return carbon::sourceAutoGuiScale(width,height); }
std::vector<carbon::MenuButton> menuButtons(const ClientApp& app,int width,int height) {
    const int scale=menuScale(width,height);
    const int logicalW=width/scale+(width%scale!=0);
    const int logicalH=height/scale+(height%scale!=0);
    std::vector<carbon::MenuButton> result;
    auto add=[&](std::string label,int lx,int ly,int lw=200,int lh=20,bool disabled=false) {
        carbon::MenuButton button{std::move(label),lx*scale,ly*scale,lw*scale,lh*scale,false,disabled};
        result.push_back(std::move(button));
    };
    switch(app.screen) {
    case ClientApp::Screen::Title: {
        const int top=logicalH/4+48;
        add("Singleplayer",0,top); add("Multiplayer",0,top+24); add("Minecraft Realms",0,top+48,200,20,true);
        add("Options...",0,top+72,98,20,true); add("Quit Game",0,top+72,98);
        result[result.size()-2].x=(logicalW/2-102)*scale;
        result.back().x=(logicalW/2+2)*scale;
        break;
    }
    case ClientApp::Screen::Worlds: {
        if(app.session) add(app.session->name+" (current session)",0,logicalH/2-18,300,36);
        const int rowY=logicalH-54;
        add("Play Selected World",0,rowY,150,20,!app.session);
        add("Create New World",0,rowY,150);
        result[result.size()-2].x=(logicalW/2-154)*scale;
        result.back().x=(logicalW/2+4)*scale;
        add("Back",0,logicalH-30,150);
        break;
    }
    case ClientApp::Screen::CreateWorld: {
        const int top=48;
        add("World Name: "+app.worldName,0,top,320);
        add("Seed: "+(app.seedText.empty()?"(random seed)":app.seedText),0,top+24,320);
        add("World Type: Normal Overworld (multi-biome)",0,top+56,320,20,true);
        add("Game Mode: Creative",0,top+80,220,20,true);
        add("Generate Structures: Off",0,top+104,220,20,true);
        add("Create World",0,logicalH-30,150);
        add("Cancel",0,logicalH-30,150);
        result[result.size()-2].x=(logicalW/2-154)*scale;
        result.back().x=(logicalW/2+4)*scale;
        break;
    }
    case ClientApp::Screen::Multiplayer: {
        add("Server: "+app.serverAddress,0,48,320);
        add("Username: "+app.username,0,72,320);
        add("Query Server Status",0,logicalH-54,150);
        add("Connect (offline servers only)",0,logicalH-54,150);
        result[result.size()-2].x=(logicalW/2-154)*scale;
        result.back().x=(logicalW/2+4)*scale;
        add("Back",0,logicalH-30,150);
        break;
    }
    case ClientApp::Screen::Loading:
        add("Cancel Generation",0,logicalH-30,150);
        break;
    case ClientApp::Screen::Paused:
        add("Resume Game",0,logicalH/2-24); add("Return to Title (session only)",0,logicalH/2+4,260);
        break;
    case ClientApp::Screen::Playing: break;
    }
    for(auto& button:result) {
        if(button.x==0) button.x=((logicalW-button.width/scale)/2)*scale;
        if(button.y==0) button.y=((logicalH-button.height/scale)/2)*scale;
        button.hovered=app.mouse.x>=button.x && app.mouse.x<button.x+button.width &&
            app.mouse.y>=button.y && app.mouse.y<button.y+button.height;
    }
    return result;
}

std::string menuTitle(ClientApp::Screen screen) {
    switch(screen) {
    case ClientApp::Screen::Title:return "";
    case ClientApp::Screen::Worlds:return "Select World";
    case ClientApp::Screen::CreateWorld:return "Create New World";
    case ClientApp::Screen::Loading:return "Preparing Normal Overworld";
    case ClientApp::Screen::Multiplayer:return "Multiplayer";
    case ClientApp::Screen::Paused:return "Game Menu";
    case ClientApp::Screen::Playing:return "";
    }
    return "";
}

std::string menuSubtitle(const ClientApp& app) {
    if(app.screen==ClientApp::Screen::Title) return {};
    if(!app.menuMessage.empty()) return app.menuMessage;
    if(app.screen==ClientApp::Screen::Worlds) return app.session ? "World data exists only in this running session; disk saves are not implemented." : "No saved worlds. Create a world to begin.";
    if(app.screen==ClientApp::Screen::CreateWorld) return app.inputField==0
        ? "Type world name; Tab selects seed. Normal multi-biome Overworld base terrain."
        : "26.3 Normal base terrain only; carvers, features and structures are not generated. Seed: decimal or Java String.hashCode.";
    if(app.screen==ClientApp::Screen::Loading) {
        const auto complete=app.terrainProgress?app.terrainProgress->load():0;
        const auto stage=app.terrainLoadStage?app.terrainLoadStage->load():0;
        if(stage==1) return "Finding safe spawn; "+std::to_string(complete)+" terrain chunks generated";
        return "Preparing initial view: "+std::to_string(std::min<std::size_t>(complete,9))+" / 9 chunks";
    }
    if(app.screen==ClientApp::Screen::Multiplayer) {
        const auto state=app.multiplayer.poll();
        switch(state.state) {
        case carbon::MultiplayerState::Idle:return "Enter address. Query status, or try login on an offline-mode server.";
        case carbon::MultiplayerState::Resolving: case carbon::MultiplayerState::Connecting: case carbon::MultiplayerState::Status: case carbon::MultiplayerState::Login:return state.message.empty()?"Connecting…":state.message;
        case carbon::MultiplayerState::StatusComplete:return state.motd+" | "+state.version+" protocol "+std::to_string(state.protocol)+" | "+std::to_string(state.playersOnline)+"/"+std::to_string(state.playersMax)+" players | "+std::to_string(state.latencyMs)+" ms";
        case carbon::MultiplayerState::EncryptionRequired:return "Server requires encryption; online authentication is not implemented.";
        case carbon::MultiplayerState::OnlineAuthenticationRequired:return "Server requires online authentication; sign-in is not implemented.";
        case carbon::MultiplayerState::LoginAcceptedConfigurationUnsupported:return "Login accepted; configuration/gameplay handoff is not implemented.";
        case carbon::MultiplayerState::Disconnected: case carbon::MultiplayerState::Failed: case carbon::MultiplayerState::Cancelled:return state.message.empty()?"Connection ended.":state.message;
        }
    }
    if(app.screen==ClientApp::Screen::Paused) return "Return to title keeps the current world only in memory.";
    return {};
}

std::int64_t javaStringHash(const std::string& utf8) {
    const auto decoded=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),nullptr,0);
    if(decoded<=0) return 0;
    std::wstring utf16(static_cast<std::size_t>(decoded),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),utf16.data(),decoded);
    std::uint32_t hash=0;
    for(wchar_t c:utf16) hash=hash*31u+static_cast<std::uint16_t>(c);
    return static_cast<std::int32_t>(hash);
}

std::int64_t parseJavaSeed(const std::string& text, bool& numeric) {
    auto begin=text.find_first_not_of(" \t\r\n");
    if(begin==std::string::npos) { numeric=false; return static_cast<std::int64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()); }
    auto end=text.find_last_not_of(" \t\r\n");
    const std::string trimmed=text.substr(begin,end-begin+1);
    try { std::size_t used=0; const auto value=std::stoll(trimmed,&used,10); if(used==trimmed.size()){numeric=true;return value;} } catch(...) {}
    numeric=false; return javaStringHash(trimmed);
}

void openMenu(ClientApp& app,ClientApp::Screen screen) {
    app.screen=screen; app.paused=screen!=ClientApp::Screen::Playing;
    app.menuMessage.clear(); app.mouse={}; app.inputField=screen==ClientApp::Screen::CreateWorld?0:(screen==ClientApp::Screen::Multiplayer?2:-1);
    if(screen==ClientApp::Screen::Playing) setCursorCapture(app,true);
    else setCursorCapture(app,false);
}

int chunkCoordinate(int blockCoordinate) {
    return blockCoordinate>=0?blockCoordinate/16:-((15-blockCoordinate)/16);
}

void cancelSeededWorldLoad(ClientApp& app) {
    if(app.terrainCancel) app.terrainCancel->store(true);
    app.pendingSession.reset();
    if(app.screen==ClientApp::Screen::Loading) openMenu(app,ClientApp::Screen::CreateWorld);
    app.menuMessage="World generation cancelled.";
}

void beginSeededWorldLoad(ClientApp& app, ClientApp::Session session) {
    if(app.terrainLoad.valid()) {
        if(app.terrainLoad.wait_for(std::chrono::seconds(0))!=std::future_status::ready) {
            app.menuMessage="The previous generation task is still stopping. Try again in a moment.";
            return;
        }
        try { (void)app.terrainLoad.get(); } catch(...) {}
        app.terrainCancel.reset();
    }
    app.pendingSession=std::move(session);
    app.terrainProgress=std::make_shared<std::atomic<std::size_t>>(0);
    app.terrainLoadStage=std::make_shared<std::atomic<int>>(1);
    app.terrainCancel=std::make_shared<std::atomic<bool>>(false);
    const auto progress=app.terrainProgress;
    const auto stage=app.terrainLoadStage;
    const auto cancel=app.terrainCancel;
    const auto root=app.sourceRoot;
    const auto seed=app.pendingSession->seed;
    app.terrainLoad=std::async(std::launch::async,[root,seed,progress,stage,cancel]() -> std::shared_ptr<const carbon::SeededOverworldChunkProvider> {
        auto provider=std::make_shared<carbon::SeededOverworldChunkProvider>(seed,root);
        const auto spawn=provider->safeSpawn(
            [progress](std::size_t generated) { progress->store(generated); },
            [cancel] { return cancel->load(); });
        if(cancel->load()) return {};
        const int centerX=chunkCoordinate(spawn.x),centerZ=chunkCoordinate(spawn.z);
        stage->store(2);
        std::size_t ready=0;
        for(int cx=centerX-1;cx<=centerX+1;++cx) for(int cz=centerZ-1;cz<=centerZ+1;++cz)
            if(provider->hasChunk(cx,cz)) ++ready;
        progress->store(ready);
        for(int cx=centerX-1;cx<=centerX+1;++cx) for(int cz=centerZ-1;cz<=centerZ+1;++cz) {
            if(cancel->load()) return {};
            const bool wasCached=provider->hasChunk(cx,cz);
            (void)provider->chunk(cx,cz);
            if(!wasCached) progress->fetch_add(1);
        }
        return provider;
    });
    openMenu(app,ClientApp::Screen::Loading);
}

void finishSeededWorldLoad(ClientApp& app) {
    if(!app.terrainLoad.valid() ||
       app.terrainLoad.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
    const bool cancelled=app.terrainCancel && app.terrainCancel->load();
    try {
        auto provider=app.terrainLoad.get();
        app.terrainCancel.reset();
        if(cancelled || app.screen!=ClientApp::Screen::Loading || !provider) {
            app.pendingSession.reset();
            return;
        }
        app.world=carbon::PlayableWorld::createSeededOverworld(std::move(provider));
        app.session=std::move(app.pendingSession);
        app.pendingSession.reset();
        const auto spawnX=static_cast<int>(std::floor(app.world.player.position.x));
        const auto spawnY=static_cast<int>(std::floor(app.world.player.position.y));
        const auto spawnZ=static_cast<int>(std::floor(app.world.player.position.z));
        app.currentBiome=app.world.seededTerrainProvider()->biomeAt(spawnX,spawnY,spawnZ);
        app.visibleCenter={std::numeric_limits<int>::min(),0,0};
        app.visibleWindowRadius=-1;
        app.visibleRevision=std::numeric_limits<std::uint64_t>::max();
        app.visibleSnapshotToken=0;
        openMenu(app,ClientApp::Screen::Playing);
        app.hud.status="Normal Overworld base terrain | seed "+std::to_string(app.session->seed)+" | session only";
    } catch(const std::exception& error) {
        app.terrainCancel.reset();
        app.pendingSession.reset();
        if(app.screen!=ClientApp::Screen::Loading) return;
        openMenu(app,ClientApp::Screen::CreateWorld);
        app.menuMessage="Seeded world generation failed: "+std::string(error.what());
    }
}

bool seededWindowCached(const std::shared_ptr<const carbon::SeededOverworldChunkProvider>& provider,
                        const carbon::BlockPos& center, int sectionRadius=2) {
    if(!provider) return false;
    const int cx=chunkCoordinate(center.x),cz=chunkCoordinate(center.z);
    for(int x=cx-sectionRadius;x<=cx+sectionRadius;++x) for(int z=cz-sectionRadius;z<=cz+sectionRadius;++z)
        if(!provider->hasChunk(x,z)) return false;
    return true;
}

void beginTerrainStream(ClientApp& app, const carbon::BlockPos& center) {
    if(app.terrainStream.valid()) return;
    auto provider=app.world.seededTerrainProvider();
    if(!provider) return;
    const int cx=chunkCoordinate(center.x),cz=chunkCoordinate(center.z);
    app.terrainStreamProgress=std::make_shared<std::atomic<std::size_t>>(0);
    const auto progress=app.terrainStreamProgress;
    app.terrainStreamCancel=std::make_shared<std::atomic<bool>>(false);
    const auto cancel=app.terrainStreamCancel;
    app.terrainStream=std::async(std::launch::async,[provider=std::move(provider),cx,cz,progress,cancel] {
        for(int x=cx-2;x<=cx+2;++x) for(int z=cz-2;z<=cz+2;++z) {
            if(cancel->load()) return;
            (void)provider->chunk(x,z);
            progress->fetch_add(1);
        }
    });
}

void finishTerrainStream(ClientApp& app) {
    if(!app.terrainStream.valid() ||
       app.terrainStream.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
    try { app.terrainStream.get(); app.terrainStreamCancel.reset(); }
    catch(const std::exception& error) { app.terrainStreamCancel.reset(); app.hud.status="Terrain chunk generation failed: "+std::string(error.what()); }
}

void clickMenu(ClientApp& app,int x,int y) {
    RECT r{}; GetClientRect(app.window,&r);
    for(const auto& button:menuButtons(app,r.right,r.bottom)) {
        if(x<button.x||x>=button.x+button.width||y<button.y||y>=button.y+button.height) continue;
        if(button.disabled) return;
        switch(app.screen) {
        case ClientApp::Screen::Title:
            if(button.label=="Singleplayer") openMenu(app,ClientApp::Screen::Worlds);
            else if(button.label=="Multiplayer") openMenu(app,ClientApp::Screen::Multiplayer);
            else if(button.label=="Quit Game") PostMessageW(app.window,WM_CLOSE,0,0);
            break;
        case ClientApp::Screen::Worlds:
            if((button.label=="Play Selected World" || (app.session && button.label==app.session->name+" (current session)")) && app.session) {
                openMenu(app,ClientApp::Screen::Playing);
                app.hud.status="Session resumed (memory only)";
            } else if(button.label=="Create New World") { app.worldName="New World"; app.seedText.clear(); openMenu(app,ClientApp::Screen::CreateWorld); }
            else if(button.label=="Back") openMenu(app,ClientApp::Screen::Title);
            break;
        case ClientApp::Screen::CreateWorld:
            if(button.label.rfind("World Name:",0)==0) app.inputField=0;
            else if(button.label.rfind("Seed:",0)==0) app.inputField=1;
            else if(button.label=="Cancel") openMenu(app,ClientApp::Screen::Worlds);
            else if(button.label=="Create World") {
                bool numeric=false; const auto seed=parseJavaSeed(app.seedText,numeric);
                beginSeededWorldLoad(app,ClientApp::Session{app.worldName.empty()?"New World":app.worldName,app.seedText,seed,numeric});
            }
            break;
        case ClientApp::Screen::Multiplayer:
            if(button.label.rfind("Server:",0)==0) app.inputField=2;
            else if(button.label.rfind("Username:",0)==0) app.inputField=3;
            else if(button.label=="Back") { app.multiplayer.cancel(); openMenu(app,ClientApp::Screen::Title); }
            else if(button.label=="Query Server Status"||button.label.rfind("Connect",0)==0) {
                std::string error; const auto address=carbon::ServerAddress::parse(app.serverAddress,&error);
                if(!address) app.menuMessage="Invalid server address: "+error;
                else {
                    const bool started=button.label=="Query Server Status" ? app.multiplayer.startStatus(*address) : app.multiplayer.startLogin(*address,app.username);
                    app.menuMessage=started ? "" : "A network request is already running.";
                }
            }
            break;
        case ClientApp::Screen::Loading:
            if(button.label=="Cancel Generation") cancelSeededWorldLoad(app);
            break;
        case ClientApp::Screen::Paused:
            if(button.label=="Resume Game") openMenu(app,ClientApp::Screen::Playing);
            else if(button.label.rfind("Return to Title",0)==0) openMenu(app,ClientApp::Screen::Title);
            break;
        case ClientApp::Screen::Playing:break;
        }
        return;
    }
}

void setCursorCapture(ClientApp& app, bool capture) {
    app.paused = !capture;
    app.hud.status = capture ? "Seeded terrain | WASD walk | Space jump | Double Space flight | Shift sneak | Ctrl sprint | Esc pause"
                            : "Paused | Esc to resume";
    if (capture) {
        RECT rect{};
        GetClientRect(app.window, &rect);
        POINT tl{rect.left, rect.top}, br{rect.right, rect.bottom};
        ClientToScreen(app.window, &tl); ClientToScreen(app.window, &br);
        RECT screenRect{tl.x, tl.y, br.x, br.y};
        ClipCursor(&screenRect);
        while (ShowCursor(FALSE) >= 0) {}
        SetCapture(app.window);
    } else {
        app.leftDown = app.rightDown = false;
        app.leftPressed = app.rightPressed = false;
        app.pickPressed = false;
        ClipCursor(nullptr);
        ReleaseCapture();
        while (ShowCursor(TRUE) < 0) {}
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = appFrom(hwnd);
    switch (msg) {
    case WM_NCCREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_SIZE:
        if (app) app->renderer.resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_SETFOCUS:
        if (app) { app->focused = true; if(app->screen==ClientApp::Screen::Playing) setCursorCapture(*app,true); }
        return 0;
    case WM_KILLFOCUS:
        if (app) { app->focused = false; setCursorCapture(*app, false); }
        return 0;
    case WM_INPUT: {
        if (!app || app->screen!=ClientApp::Screen::Playing) break;
        UINT bytes = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &bytes, sizeof(RAWINPUTHEADER));
        if (!bytes) break;
        std::vector<std::byte> data(bytes);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, data.data(), &bytes, sizeof(RAWINPUTHEADER)) == bytes) {
            const auto* input = reinterpret_cast<const RAWINPUT*>(data.data());
            if (input->header.dwType == RIM_TYPEMOUSE) {
                app->rawDelta.x += input->data.mouse.lLastX;
                app->rawDelta.y += input->data.mouse.lLastY;
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE: if(app) { app->mouse={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)}; } return 0;
    case WM_LBUTTONDOWN:
        if(app && app->screen!=ClientApp::Screen::Playing) clickMenu(*app,GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
        else if(app && !app->leftDown) { app->leftDown=true; app->leftPressed=true; }
        return 0;
    case WM_LBUTTONUP: if (app) app->leftDown = false; return 0;
    case WM_RBUTTONDOWN: if (app && app->screen==ClientApp::Screen::Playing && !app->rightDown) { app->rightDown = true; app->rightPressed = true; } return 0;
    case WM_RBUTTONUP: if (app) app->rightDown = false; return 0;
    case WM_MBUTTONDOWN: if (app && app->screen==ClientApp::Screen::Playing) app->pickPressed = true; return 0;
    case WM_MOUSEWHEEL:
        if (app && app->screen==ClientApp::Screen::Playing) {
            app->hud.hotbar.scroll(static_cast<double>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
        }
        return 0;
    case WM_KEYDOWN:
        if (app && !(lParam & (1L << 30))) {
            if (wParam == VK_ESCAPE) {
                if(app->screen==ClientApp::Screen::Playing) openMenu(*app,ClientApp::Screen::Paused);
                else if(app->screen==ClientApp::Screen::Paused) openMenu(*app,ClientApp::Screen::Playing);
                else if(app->screen==ClientApp::Screen::Loading) cancelSeededWorldLoad(*app);
                else if(app->screen==ClientApp::Screen::CreateWorld) openMenu(*app,ClientApp::Screen::Worlds);
                else if(app->screen==ClientApp::Screen::Worlds) openMenu(*app,ClientApp::Screen::Title);
                else if(app->screen==ClientApp::Screen::Multiplayer) { app->multiplayer.cancel(); openMenu(*app,ClientApp::Screen::Title); }
            }
            if(app->screen==ClientApp::Screen::CreateWorld && wParam==VK_TAB) app->inputField=app->inputField==0?1:0;
            if(app->screen==ClientApp::Screen::Multiplayer && wParam==VK_TAB) app->inputField=app->inputField==2?3:2;
            if((app->screen==ClientApp::Screen::CreateWorld || app->screen==ClientApp::Screen::Multiplayer) && wParam==VK_BACK) {
                auto& value=app->screen==ClientApp::Screen::CreateWorld?(app->inputField==0?app->worldName:app->seedText):(app->inputField==3?app->username:app->serverAddress);
                if(!value.empty()) value.pop_back();
            }
            if(app->screen!=ClientApp::Screen::Playing && wParam==VK_RETURN) {
                RECT r{}; GetClientRect(hwnd,&r); const auto buttons=menuButtons(*app,r.right,r.bottom);
                for(const auto& button:buttons) if(button.label=="Create World"||button.label=="Query Server Status") { clickMenu(*app,button.x+button.width/2,button.y+button.height/2); break; }
            }
            if (app->screen==ClientApp::Screen::Playing && wParam >= '1' && wParam <= '9') app->hud.hotbar.selectSlot(static_cast<int>(wParam - '1'));
        }
        return 0;
    case WM_CHAR:
        if(app && (app->screen==ClientApp::Screen::CreateWorld || app->screen==ClientApp::Screen::Multiplayer) && wParam>=32 && wParam<127) {
            auto& value=app->screen==ClientApp::Screen::CreateWorld?(app->inputField==0?app->worldName:app->seedText):(app->inputField==3?app->username:app->serverAddress);
            if(value.size()<128) value.push_back(static_cast<char>(wParam));
        }
        return 0;
    case WM_CLOSE:
        if(app && app->terrainCancel) app->terrainCancel->store(true);
        if(app && app->terrainStreamCancel) app->terrainStreamCancel->store(true);
        DestroyWindow(hwnd); return 0;
    case WM_DESTROY: if (app) app->running = false; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool keyDown(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }

fs::path defaultSourceRoot() {
    const fs::path configured{CARBON_SOURCE_ROOT};
    if (configured.is_absolute()) return configured;
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length > 0 && static_cast<std::size_t>(length) < executable.size()) {
        executable.resize(length);
        // Search upward so both build/carbon.exe and alternate artifacts such
        // as build/port-integration/carbon.exe resolve the sibling resources.
        for (auto directory = fs::path(executable).parent_path(); !directory.empty(); directory = directory.parent_path()) {
            const auto candidate = directory.parent_path().parent_path() / L"JavaMinecraft" / L"decompiled";
            if (fs::is_regular_file(candidate / L"version.json")) return candidate;
            if (directory == directory.root_path()) break;
        }
    }
    return configured;
}

void tick(ClientApp& app) {
    if (app.paused || !app.focused) return;
    app.dayTime=(app.dayTime+1)%24000;
    // Java Options defaults to sensitivity 0.5. MouseHandler applies
    // (sensitivity*0.6+0.2)^3*8 then the player turn multiplier 0.15.
    constexpr float degreesPerPixel = 0.15F;
    app.world.player.yaw += static_cast<float>(app.rawDelta.x) * degreesPerPixel;
    app.world.player.pitch = std::clamp(app.world.player.pitch + static_cast<float>(app.rawDelta.y) * degreesPerPixel, -90.0F, 90.0F);
    app.rawDelta = {};

    carbon::MoveInput input;
    input.forward = static_cast<float>(keyDown('W')) - static_cast<float>(keyDown('S'));
    input.strafe = static_cast<float>(keyDown('D')) - static_cast<float>(keyDown('A'));
    input.jump = keyDown(VK_SPACE);
    input.descend = keyDown(VK_SHIFT);
    input.sprint = keyDown(VK_LCONTROL) || keyDown(VK_RCONTROL);
    bool nearbyTerrainReady=true;
    if(app.world.hasSeededTerrain()) {
        const auto terrain=app.world.seededTerrainProvider();
        const int cx=chunkCoordinate(static_cast<int>(std::floor(app.world.player.position.x)));
        const int cz=chunkCoordinate(static_cast<int>(std::floor(app.world.player.position.z)));
        for(int x=cx-1;x<=cx+1 && nearbyTerrainReady;++x)
            for(int z=cz-1;z<=cz+1;++z)
                if(!terrain->hasChunk(x,z)) { nearbyTerrainReady=false; break; }
    }
    if(!nearbyTerrainReady) {
        input={};
        app.leftPressed=false;
        app.rightPressed=false;
        app.pickPressed=false;
        app.hud.status="Waiting for nearby terrain chunks...";
    }
    const bool wasFlying = app.world.player.flying;
    app.world.tickCreativeMove(input);
    if (wasFlying != app.world.player.flying)
        app.hud.status = app.world.player.flying ? "Creative flight enabled" : "Creative flight disabled";

    // Minecraft.keyPickItem is bound to mouse button 3. The client asks the
    // server for the hit block's clone item; this offline slice applies the
    // same selection/add-to-hotbar rule for its supported block item subset.
    if (app.pickPressed) {
        const auto hit = app.world.lookAtBlock();
        if (hit) {
            const auto block = app.world.getBlock(hit->block);
            if (app.hud.hotbar.pickBlock(block)) app.hud.status = "Block picked";
            else app.hud.status = "No supported block item";
        } else {
            app.hud.status = "No block in reach";
        }
        app.pickPressed = false;
    }

    // Minecraft decrements these mode delays each client tick. Creative block
    // breaking repeats at a five-tick cadence; use starts with a four-tick delay.
    if (app.rightClickDelay > 0) --app.rightClickDelay;
    if (app.leftPressed) {
        if (app.world.breakTarget()) app.hud.status = "Block broken";
        else app.hud.status = "No block in reach";
        app.destroyDelay.beginFreshPress();
        app.leftPressed = false;
    } else if (app.leftDown && app.destroyDelay.beginHeldTick()) {
        if (app.world.breakTarget()) app.hud.status = "Block broken";
        else app.hud.status = "No block in reach";
    }
    if (app.rightPressed || (app.rightDown && app.rightClickDelay == 0)) {
        const auto block = app.hud.hotbar.slots[static_cast<std::size_t>(app.hud.hotbar.selectedSlot)];
        if (app.world.placeTarget(block)) app.hud.status = "Block placed";
        else app.hud.status = "Cannot place here";
        app.rightClickDelay = 4;
        app.rightPressed = false;
    }
}

int runClient(fs::path sourceRoot, fs::path smokeMenuImage = {}, fs::path smokeWorldImage = {},
              fs::path smokeStreamImage = {}, fs::path smokeTracePath = {},
              int windowWidth = kWidth, int windowHeight = kHeight, std::int64_t smokeSeed = 12345) {
    ClientApp app;
    app.sourceRoot = std::move(sourceRoot);
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Could not register Carbon window class");

    RECT rect{0, 0, windowWidth, windowHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    app.window = CreateWindowExW(0, kWindowClass, L"Carbon | Java Edition 26.3 native slice", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, &app);
    if (!app.window) throw std::runtime_error("Could not create Carbon window");

    RAWINPUTDEVICE mouse{0x01, 0x02, 0, app.window};
    RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
    std::string error;
    if (!app.renderer.initialize(app.window, windowWidth, windowHeight, app.sourceRoot, &error)) {
        DestroyWindow(app.window);
        throw std::runtime_error("Renderer startup failed: " + error);
    }
    ShowWindow(app.window, SW_SHOW);
    UpdateWindow(app.window);
    setCursorCapture(app, false);

    using clock = std::chrono::steady_clock;
    const bool smokeMode=!smokeTracePath.empty();
    if(smokeMode) app.dayTime=6000;
    std::ofstream smokeTrace;
    if(smokeMode) {
        if(!smokeTracePath.parent_path().empty()) fs::create_directories(smokeTracePath.parent_path());
        smokeTrace.open(smokeTracePath,std::ios::binary|std::ios::trunc);
        if(!smokeTrace) throw std::runtime_error("Could not create client smoke trace: "+smokeTracePath.string());
        smokeTrace<<"elapsed_ms\tscreen\tload_stage\tload_chunks\tstream_chunks\tcached_chunks\tvisible_blocks\tmesh_builds\tpresents\tframe_ms\tworld_ms\n";
        smokeTrace.flush();
    }
    int smokeClickStage=0;
    bool smokeMenuCaptured=false,smokeWorldCaptured=false,smokeFirstWorldFrameDone=false,smokeRelocated=false,smokeStreamCaptured=false,smokeFailed=false;
    std::size_t smokeTicksInPlay=0;
    std::uint64_t smokeLoopFrames=0,smokeLoadingFrames=0;
    double smokeMaxLoopGapMs=0.0,smokeLoadMs=0.0,smokeFirstWorldFrameMs=0.0,smokeStreamMs=0.0;
    std::optional<clock::time_point> smokeLoadStarted,smokeWorldReadyAt,smokePlayingStarted,smokeStreamStarted;
    auto smokeStart=clock::now(),smokePreviousLoop=smokeStart,smokeLastTrace=smokeStart;
    auto smokePostButton=[&](std::string_view label) {
        RECT r{}; GetClientRect(app.window,&r);
        for(const auto& button:menuButtons(app,r.right,r.bottom)) if(button.label==label) {
            return PostMessageW(app.window,WM_LBUTTONDOWN,MK_LBUTTON,
                MAKELPARAM(button.x+button.width/2,button.y+button.height/2))!=FALSE;
        }
        return false;
    };

    auto previous = clock::now();
    double accumulator = 0;
    constexpr auto frameDuration = std::chrono::milliseconds(16);
    MSG message{};
    while (app.running) {
        const auto frameStart = clock::now();
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) app.running = false;
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if (!app.running) break;
        const auto now = clock::now();
        if(smokeMode) {
            ++smokeLoopFrames;
            smokeMaxLoopGapMs=std::max(smokeMaxLoopGapMs,std::chrono::duration<double,std::milli>(now-smokePreviousLoop).count());
            smokePreviousLoop=now;
            if(app.screen==ClientApp::Screen::Loading) ++smokeLoadingFrames;
            if(app.screen==ClientApp::Screen::Playing && !smokePlayingStarted) smokePlayingStarted=now;
            if(smokeClickStage==0 && app.screen==ClientApp::Screen::Title) {
                if(smokePostButton("Singleplayer")) smokeClickStage=1;
                else { app.error="Smoke runner could not find the Singleplayer button"; smokeFailed=true; app.running=false; }
            } else if(smokeClickStage==1 && app.screen==ClientApp::Screen::Worlds) {
                if(smokePostButton("Create New World")) smokeClickStage=2;
                else { app.error="Smoke runner could not find the Create New World button"; smokeFailed=true; app.running=false; }
            } else if(smokeClickStage==2 && app.screen==ClientApp::Screen::CreateWorld) {
                app.seedText=std::to_string(smokeSeed);
                if(smokePostButton("Create World")) {
                    smokeClickStage=3;
                    smokeLoadStarted=now;
                } else { app.error="Smoke runner could not find the Create World button"; smokeFailed=true; app.running=false; }
            }
            if(smokeClickStage>=3 && app.screen==ClientApp::Screen::CreateWorld && !app.menuMessage.empty()) {
                app.error="World join did not complete: "+app.menuMessage;
                smokeFailed=true; app.running=false;
            }
            if(now-smokeStart>std::chrono::minutes(8) && !smokeStreamCaptured) {
                app.error="Smoke run exceeded its 8 minute deadline";
                smokeFailed=true; app.running=false;
            }
        }
        if(!app.running) break;
        finishSeededWorldLoad(app);
        if(smokeMode && app.screen==ClientApp::Screen::Playing && !smokeWorldReadyAt) {
            smokeWorldReadyAt=clock::now();
            if(smokeLoadStarted) smokeLoadMs=std::chrono::duration<double,std::milli>(*smokeWorldReadyAt-*smokeLoadStarted).count();
        }
        if(smokeMode && app.screen==ClientApp::Screen::Playing && !smokePlayingStarted) smokePlayingStarted=clock::now();
        finishTerrainStream(app);
        accumulator += std::chrono::duration<double>(now - previous).count();
        previous = now;
        while (accumulator >= kTickSeconds) {
            if(app.screen==ClientApp::Screen::Playing) {
                tick(app);
                if(smokeMode) ++smokeTicksInPlay;
            }
            accumulator -= kTickSeconds;
        }
        RECT client{};
        GetClientRect(app.window, &client);
        if(app.screen==ClientApp::Screen::Playing) {
            const carbon::BlockPos center(sectionAnchor(static_cast<int>(std::floor(app.world.player.position.x))),
                                          sectionAnchor(static_cast<int>(std::floor(app.world.player.position.y))),
                                          sectionAnchor(static_cast<int>(std::floor(app.world.player.position.z))));
            const auto revision=app.world.sceneRevision();
            if (center!=app.visibleCenter || revision!=app.visibleRevision || app.world.hasSeededTerrain()) {
                int readyRadius=2;
                if(app.world.hasSeededTerrain()) {
                    const auto provider=app.world.seededTerrainProvider();
                    readyRadius=seededWindowCached(provider,center,2)?2:
                                seededWindowCached(provider,center,1)?1:-1;
                }
                if(!app.world.hasSeededTerrain() || readyRadius>=1) {
                    if(center!=app.visibleCenter || revision!=app.visibleRevision || readyRadius!=app.visibleWindowRadius) {
                        app.visibleBlocks=app.world.blocksInSectionWindow(center,readyRadius,kSectionWindowRadius);
                        app.visibleCenter=center; app.visibleWindowRadius=readyRadius;
                        app.visibleRevision=revision; ++app.visibleSnapshotToken;
                    }
                    if(app.world.hasSeededTerrain() && readyRadius<2) {
                        beginTerrainStream(app,center);
                        const auto ready=app.terrainStreamProgress?app.terrainStreamProgress->load():0;
                        app.hud.status="Loading terrain around you "+std::to_string(std::min<std::size_t>(ready,25))+" / 25 chunks";
                    }
                    if(app.world.hasSeededTerrain() && app.visibleSnapshotToken==1 && app.visibleBlocks.empty()) {
                        openMenu(app,ClientApp::Screen::CreateWorld);
                        app.menuMessage="World loading failed: the generated spawn window contains no renderable blocks.";
                    }
                } else {
                    beginTerrainStream(app,center);
                    const auto ready=app.terrainStreamProgress?app.terrainStreamProgress->load():0;
                    app.hud.status="Loading terrain around you "+std::to_string(std::min<std::size_t>(ready,25))+" / 25 chunks";
                }
            }
            const carbon::RenderBlockBounds fluidBounds{center.x-40,center.x+39,
                center.y-kSectionWindowRadius,center.y+kSectionWindowRadius,
                center.z-40,center.z+39};
            const int playerX=static_cast<int>(std::floor(app.world.player.position.x));
            const int playerY=static_cast<int>(std::floor(app.world.player.position.y));
            const int playerZ=static_cast<int>(std::floor(app.world.player.position.z));
            const auto visualTerrain=app.world.seededTerrainProvider();
            if(visualTerrain && visualTerrain->hasChunk(chunkCoordinate(playerX),chunkCoordinate(playerZ)))
                app.currentBiome=visualTerrain->biomeAt(playerX,playerY,playerZ);
            const carbon::WorldVisualState visualState{app.currentBiome,app.dayTime,kPlayerViewRadius/12};
            app.renderer.render(app.visibleBlocks, app.world.player, app.hud, revision,
                                app.visibleSnapshotToken,&fluidBounds,&visualState);
            if(smokeMode && app.world.hasSeededTerrain() && smokePlayingStarted && !smokeFirstWorldFrameDone) {
                smokeFirstWorldFrameMs=std::chrono::duration<double,std::milli>(clock::now()-*smokePlayingStarted).count();
                smokeFirstWorldFrameDone=true;
            }
        } else {
            const auto buttons=menuButtons(app,client.right,client.bottom);
            app.renderer.renderMenu(menuTitle(app.screen),buttons,app.screen==ClientApp::Screen::Title,menuSubtitle(app));
        }
        if(smokeMode && app.screen==ClientApp::Screen::Title && !smokeMenuCaptured) {
            if(!app.renderer.writeScreenshot(smokeMenuImage,&app.error)) { smokeFailed=true; app.running=false; }
            else smokeMenuCaptured=true;
        }
        if(smokeMode && app.screen==ClientApp::Screen::Playing && app.world.hasSeededTerrain()) {
            if(!smokeWorldCaptured && smokeTicksInPlay>=10) {
                if(!app.renderer.writeScreenshot(smokeWorldImage,&app.error)) { smokeFailed=true; app.running=false; }
                else {
                    smokeWorldCaptured=true;
                    app.world.player.flying=true;
                    app.world.player.position.x+=48.0;
                    smokeRelocated=true;
                    smokeStreamStarted=clock::now();
                }
            } else if(smokeRelocated && !smokeStreamCaptured) {
                const carbon::BlockPos smokeCenter(sectionAnchor(static_cast<int>(std::floor(app.world.player.position.x))),
                    sectionAnchor(static_cast<int>(std::floor(app.world.player.position.y))),
                    sectionAnchor(static_cast<int>(std::floor(app.world.player.position.z))));
                if(smokeCenter==app.visibleCenter && seededWindowCached(app.world.seededTerrainProvider(),smokeCenter)) {
                    if(!app.renderer.writeScreenshot(smokeStreamImage,&app.error)) { smokeFailed=true; app.running=false; }
                    else {
                        smokeStreamCaptured=true;
                        if(smokeStreamStarted) smokeStreamMs=std::chrono::duration<double,std::milli>(clock::now()-*smokeStreamStarted).count();
                        app.running=false;
                    }
                }
            }
        }
        if(smokeMode) {
            const auto traceNow=clock::now();
            if(traceNow-smokeLastTrace>=std::chrono::milliseconds(500)) {
                const auto terrain=app.world.seededTerrainProvider();
                const auto cached=terrain?terrain->cachedChunkCount():0;
                const auto loadStage=app.terrainLoadStage?app.terrainLoadStage->load():0;
                const auto loadChunks=app.terrainProgress?app.terrainProgress->load():0;
                const auto streamChunks=app.terrainStreamProgress?app.terrainStreamProgress->load():0;
                const auto& stats=app.renderer.performanceStats();
                smokeTrace<<std::chrono::duration_cast<std::chrono::milliseconds>(traceNow-smokeStart).count()<<'\t'
                    <<static_cast<int>(app.screen)<<'\t'<<loadStage<<'\t'<<loadChunks<<'\t'<<streamChunks<<'\t'<<cached<<'\t'
                    <<app.visibleBlocks.size()<<'\t'<<stats.meshBuilds<<'\t'<<app.renderer.presentedFrames()<<'\t'
                    <<stats.lastCpuFrameMs<<'\t'<<stats.lastWorldDrawMs<<'\n';
                smokeTrace.flush();
                smokeLastTrace=traceNow;
            }
        }
        const auto spent = clock::now() - frameStart;
        if(smokeMode) smokeMaxLoopGapMs=std::max(smokeMaxLoopGapMs,std::chrono::duration<double,std::milli>(spent).count());
        if (spent < frameDuration) std::this_thread::sleep_for(frameDuration - spent);
    }
    setCursorCapture(app, false);
    app.renderer.shutdown();
    UnregisterClassW(kWindowClass, instance);
    if(smokeMode) {
        if(!smokeMenuCaptured||!smokeWorldCaptured||!smokeStreamCaptured) smokeFailed=true;
        smokeTrace<<"# result: "<<(smokeFailed?"FAIL":"PASS")<<"\n# menu_capture="<<smokeMenuCaptured
            <<" world_capture="<<smokeWorldCaptured<<" stream_capture="<<smokeStreamCaptured
            <<" load_ms="<<smokeLoadMs<<" first_world_frame_ms="<<smokeFirstWorldFrameMs
            <<" stream_ms="<<smokeStreamMs<<" loading_frames="<<smokeLoadingFrames
            <<" loop_frames="<<smokeLoopFrames<<" max_loop_gap_ms="<<smokeMaxLoopGapMs
            <<" visible_blocks="<<app.visibleBlocks.size()<<" mesh_builds="<<app.renderer.performanceStats().meshBuilds
            <<" error="<<app.error<<"\n";
        smokeTrace.flush();
        std::cout<<"Native client join smoke: "<<(smokeFailed?"FAIL":"PASS")
            <<" load_ms="<<smokeLoadMs<<" first_world_frame_ms="<<smokeFirstWorldFrameMs
            <<" stream_ms="<<smokeStreamMs<<" loading_frames="<<smokeLoadingFrames
            <<" loop_frames="<<smokeLoopFrames<<" max_loop_gap_ms="<<smokeMaxLoopGapMs
            <<" menu="<<smokeMenuCaptured<<" world="<<smokeWorldCaptured<<" stream="<<smokeStreamCaptured
            <<" error="<<app.error<<'\n';
        return smokeFailed?2:0;
    }
    return 0;
}

int runDiagnostics(const fs::path& root) {
    std::ifstream version(root / "version.json", std::ios::binary);
    if (!version || !fs::is_directory(root / "assets/minecraft") || !fs::is_directory(root / "data/minecraft"))
        throw std::runtime_error("Expected version.json, assets/minecraft and data/minecraft in source root");
    carbon::DirectoryResources resources(root);
    const auto sample = resources.read(carbon::ResourceTree::assets,
        *carbon::Identifier::tryParse("minecraft:waypoint_style/default.json"));
    if (!sample) throw std::runtime_error("Unable to read original waypoint_style/default.json asset");
    std::cout << "Carbon: native C++ Java Edition 26.3 playable slice\nSource: " << fs::absolute(root).string()
              << "\nOriginal version metadata:\n" << version.rdbuf();
    std::uintmax_t files = 0, bytes = 0;
    for (const auto& directory : {"assets", "data"})
        for (const auto& entry : fs::recursive_directory_iterator(root / directory))
            if (entry.is_regular_file()) { ++files; bytes += entry.file_size(); }
    std::cout << "\nOriginal resource/data files: " << files << " (" << bytes << " bytes)\n"
              << "Original waypoint style loaded: " << sample->size() << " bytes\n"
              << "Implemented client slice: source-backed 26.3 Normal multi-biome overworld base terrain, creative flight, nine-slot supported-block hotbar, pick-block, break/place, supported source block models and stationary water/lava source rendering. Carvers, features and save files are not implemented.\n"
              << "Native UI: title, world selection/Normal Overworld creation, loading progress/cancellation, multiplayer pregame and pause screens; multiplayer supports server status ping and an offline-mode login start only, with no authentication or configuration/gameplay handoff.\n"
              << "Omitted: complete registries/resource reload, full light propagation, entities, survival, full inventory/codecs, disk saves, integrated server, complete Java screens and full Java parity.\n";
    return 0;
}
}

int main(int argc, char** argv) try {
    if (argc == 8 && std::string_view(argv[1]) == "--smoke-world-load")
        return runClient(defaultSourceRoot(),argv[4],argv[5],argv[6],argv[7],std::stoi(argv[2]),std::stoi(argv[3]));
    if (argc == 9 && std::string_view(argv[1]) == "--smoke-world-load")
        return runClient(defaultSourceRoot(),argv[4],argv[5],argv[6],argv[7],std::stoi(argv[2]),std::stoi(argv[3]),std::stoll(argv[8]));
    if (argc == 3 && std::string_view(argv[1]) == "--random-vectors") {
        carbon::LegacyRandomSource random(std::stoll(argv[2]));
        std::cout << std::setprecision(17);
        for (int i = 0; i < 256; ++i)
            std::cout << random.nextInt() << ' ' << random.nextInt(1073741825) << ' ' << random.nextLong() << ' '
                      << random.nextBoolean() << ' ' << random.nextFloat() << ' ' << random.nextDouble() << ' '
                      << random.nextGaussian() << '\n';
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--source-root") return runClient(argv[2]);
    if (argc == 2 && std::string_view(argv[1]) == "--diagnostics") return runDiagnostics(defaultSourceRoot());
    if (argc == 3 && std::string_view(argv[1]) == "--diagnostics-source-root") return runDiagnostics(argv[2]);
    if (argc != 1) throw std::invalid_argument("Usage: carbon [--diagnostics [--source-root <directory>]] | --random-vectors <seed> | --smoke-world-load <width> <height> <menu.ppm> <world.ppm> <stream.ppm> <trace.tsv> [seed]");
    return runClient(defaultSourceRoot());
} catch (const std::exception& error) {
    std::cerr << "Carbon: " << error.what() << '\n';
    return 1;
}




