#include "carbon/ClientRenderer.hpp"
#include "carbon/ClientControls.hpp"
#include "carbon/DirectoryResources.hpp"
#include "carbon/LegacyRandomSource.hpp"
#include "carbon/PlayableWorld.hpp"
#include "carbon/ClientRenderer.hpp"
#include "carbon/Multiplayer.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <filesystem>
#include <fstream>
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
    enum class Screen { Title, Worlds, CreateWorld, Multiplayer, Paused, Playing } screen{Screen::Title};
    carbon::PlayableWorld world = carbon::PlayableWorld::createDefaultFlat();
    carbon::MultiplayerClient multiplayer;
    struct Session { std::string name, seedText; std::int64_t seed{}; bool numericSeed{}; };
    std::optional<Session> session;
    bool flatPresetSelected{};
    std::string worldName{"New World"}, seedText, serverAddress{"localhost"}, username{"Player"}, menuMessage;
    int inputField{}; // Create World: 0 name / 1 seed; Multiplayer: 2 address / 3 username
    POINT mouse{};
    std::vector<carbon::BlockInstance> visibleBlocks;
    carbon::BlockPos visibleCenter{std::numeric_limits<int>::min(),0,0};
    std::uint64_t visibleRevision{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t visibleSnapshotToken{};
    carbon::ClientHud hud;
    fs::path sourceRoot;
    bool focused{true};
    bool paused{};
    bool running{true};
    bool leftDown{}, rightDown{}, leftPressed{}, rightPressed{}, pickPressed{};
    carbon::CreativeBreakDelay destroyDelay;
    int rightClickDelay{};
    POINT rawDelta{};
    std::string error;
};

ClientApp* appFrom(HWND hwnd) { return reinterpret_cast<ClientApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)); }
void setCursorCapture(ClientApp& app, bool capture);

int menuScale(int width,int height) { return std::max(1,std::min(width/320,height/180)); }
std::vector<carbon::MenuButton> menuButtons(const ClientApp& app,int width,int height) {
    const int scale=menuScale(width,height), w=200*scale, h=20*scale;
    const int logicalH=height/scale;
    std::vector<carbon::MenuButton> result;
    auto add=[&](std::string label,int lx,int ly,int lw=200,int lh=20,bool disabled=false) {
        carbon::MenuButton button{std::move(label),lx*scale,ly*scale,lw*scale,lh*scale,false,disabled};
        button.hovered=app.mouse.x>=button.x && app.mouse.x<button.x+button.width && app.mouse.y>=button.y && app.mouse.y<button.y+button.height;
        result.push_back(std::move(button));
    };
    switch(app.screen) {
    case ClientApp::Screen::Title: {
        const int top=logicalH/4+48;
        add("Singleplayer",0,top); add("Multiplayer",0,top+24);
        add("Options",-102,top+72,98); add("Quit",4,top+72,98); // replaced with centered pair below
        result[result.size()-2].x=(width/2-102*scale);
        result.back().x=(width/2+4*scale);
        break;
    }
    case ClientApp::Screen::Worlds: {
        add(app.session ? "Resume: "+app.session->name+" [session only]" : "No saved worlds",0,logicalH/2-40,320,20,!app.session);
        add("Create New World",0,logicalH/2-12);
        add("Back",0,logicalH/2+16,200);
        break;
    }
    case ClientApp::Screen::CreateWorld: {
        add("World Name: "+app.worldName,0,logicalH/2-76,320);
        add("Seed: "+(app.seedText.empty()?"(random seed)":app.seedText),0,logicalH/2-48,320);
        add(std::string("World Type: ")+(app.flatPresetSelected?"Classic Flat (seed does not change terrain)":"Normal (generation unavailable)"),0,logicalH/2-20,320);
        add("Game Mode: Creative",0,logicalH/2+8,220,20,true);
        add("Generate Structures: Off",0,logicalH/2+36,220,20,true);
        add("Create World",0,logicalH/2+64,200,20,!app.flatPresetSelected);
        add("Cancel",0,logicalH/2+92);
        break;
    }
    case ClientApp::Screen::Multiplayer: {
        add("Server: "+app.serverAddress,0,logicalH/2-64,320);
        add("Username: "+app.username,0,logicalH/2-36,320);
        add("Query Server Status",0,logicalH/2-4);
        add("Connect (offline servers only)",0,logicalH/2+24,240);
        add("Back",0,logicalH/2+52);
        break;
    }
    case ClientApp::Screen::Paused:
        add("Resume Game",0,logicalH/2-24); add("Return to Title (session only)",0,logicalH/2+4,260);
        break;
    case ClientApp::Screen::Playing: break;
    }
    for(auto& button:result) {
        if(button.x==0) button.x=(width-button.width)/2;
        if(button.y==0) button.y=(height-button.height)/2;
    }
    (void)w;(void)h;
    return result;
}

std::string menuTitle(ClientApp::Screen screen) {
    switch(screen) {
    case ClientApp::Screen::Title:return "";
    case ClientApp::Screen::Worlds:return "Select World";
    case ClientApp::Screen::CreateWorld:return "Create New World";
    case ClientApp::Screen::Multiplayer:return "Multiplayer";
    case ClientApp::Screen::Paused:return "Game Menu";
    case ClientApp::Screen::Playing:return "";
    }
    return "";
}

std::string menuSubtitle(const ClientApp& app) {
    if(!app.menuMessage.empty()) return app.menuMessage;
    if(app.screen==ClientApp::Screen::Worlds) return app.session ? "World data exists only in this running session; disk saves are not implemented." : "No session worlds available.";
    if(app.screen==ClientApp::Screen::CreateWorld) return app.inputField==0 ? "Type world name; Tab selects seed. Normal terrain is still under implementation." : "Signed decimal or Java String.hashCode; blank uses a random seed.";
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
    return "Java Edition 26.3 native client slice";
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

void clickMenu(ClientApp& app,int x,int y) {
    RECT r{}; GetClientRect(app.window,&r);
    for(const auto& button:menuButtons(app,r.right,r.bottom)) {
        if(x<button.x||x>=button.x+button.width||y<button.y||y>=button.y+button.height) continue;
        if(button.disabled) return;
        switch(app.screen) {
        case ClientApp::Screen::Title:
            if(button.label=="Singleplayer") openMenu(app,ClientApp::Screen::Worlds);
            else if(button.label=="Multiplayer") openMenu(app,ClientApp::Screen::Multiplayer);
            else if(button.label=="Quit") PostMessageW(app.window,WM_CLOSE,0,0);
            else if(button.label=="Options") app.menuMessage="Options are not implemented in this native slice.";
            break;
        case ClientApp::Screen::Worlds:
            if(button.label.rfind("Resume:",0)==0 && app.session) {
                openMenu(app,ClientApp::Screen::Playing);
                app.hud.status="Session resumed (memory only)";
            } else if(button.label=="Create New World") { app.worldName="New World"; app.seedText.clear(); openMenu(app,ClientApp::Screen::CreateWorld); }
            else if(button.label=="Back") openMenu(app,ClientApp::Screen::Title);
            break;
        case ClientApp::Screen::CreateWorld:
            if(button.label.rfind("World Name:",0)==0) app.inputField=0;
            else if(button.label.rfind("Seed:",0)==0) app.inputField=1;
            else if(button.label.rfind("World Type:",0)==0) app.flatPresetSelected=!app.flatPresetSelected;
            else if(button.label=="Cancel") openMenu(app,ClientApp::Screen::Worlds);
            else if(button.label=="Create World") {
                bool numeric=false; const auto seed=parseJavaSeed(app.seedText,numeric);
                if(!app.flatPresetSelected) app.menuMessage="Normal Overworld generation is not yet available. Choose Classic Flat to play.";
                else {
                    app.session=ClientApp::Session{app.worldName.empty()?"New World":app.worldName,app.seedText,seed,numeric};
                    app.world=carbon::PlayableWorld::createDefaultFlat();
                    app.visibleCenter={std::numeric_limits<int>::min(),0,0}; app.visibleRevision=std::numeric_limits<std::uint64_t>::max();
                    openMenu(app,ClientApp::Screen::Playing);
                    app.hud.status="Classic Flat | seed "+std::to_string(seed)+" | memory only";
                }
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
    app.hud.status = capture ? "Classic Flat | WASD walk | Space jump | Double Space flight | Shift sneak | Ctrl sprint | Esc pause"
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
                else if(app->screen==ClientApp::Screen::CreateWorld) openMenu(*app,ClientApp::Screen::Worlds);
                else if(app->screen==ClientApp::Screen::Worlds) openMenu(*app,ClientApp::Screen::Title);
                else if(app->screen==ClientApp::Screen::Multiplayer) { app->multiplayer.cancel(); openMenu(*app,ClientApp::Screen::Title); }
            }
            if(app->screen==ClientApp::Screen::CreateWorld && wParam==VK_TAB) app->inputField=app->inputField==0?1:0;
            if(app->screen==ClientApp::Screen::Multiplayer && wParam==VK_TAB) app->inputField=app->inputField==2?3:2;
            if(app->screen!=ClientApp::Screen::Playing && wParam==VK_BACK) {
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
        if(app && app->screen!=ClientApp::Screen::Playing && wParam>=32 && wParam<127) {
            auto& value=app->screen==ClientApp::Screen::CreateWorld?(app->inputField==0?app->worldName:app->seedText):(app->inputField==3?app->username:app->serverAddress);
            if(value.size()<128) value.push_back(static_cast<char>(wParam));
        }
        return 0;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
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

int runClient(fs::path sourceRoot) {
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

    RECT rect{0, 0, kWidth, kHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    app.window = CreateWindowExW(0, kWindowClass, L"Carbon | Java Edition 26.3 native slice", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, &app);
    if (!app.window) throw std::runtime_error("Could not create Carbon window");

    RAWINPUTDEVICE mouse{0x01, 0x02, 0, app.window};
    RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
    std::string error;
    if (!app.renderer.initialize(app.window, kWidth, kHeight, app.sourceRoot, &error)) {
        DestroyWindow(app.window);
        throw std::runtime_error("Renderer startup failed: " + error);
    }
    ShowWindow(app.window, SW_SHOW);
    UpdateWindow(app.window);
    setCursorCapture(app, false);

    using clock = std::chrono::steady_clock;
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
        accumulator += std::chrono::duration<double>(now - previous).count();
        previous = now;
        while (accumulator >= kTickSeconds) { if(app.screen==ClientApp::Screen::Playing) tick(app); accumulator -= kTickSeconds; }
        RECT client{};
        GetClientRect(app.window, &client);
        if(app.screen==ClientApp::Screen::Playing) {
            const carbon::BlockPos center(sectionAnchor(static_cast<int>(std::floor(app.world.player.position.x))),
                                          sectionAnchor(static_cast<int>(std::floor(app.world.player.position.y))),
                                          sectionAnchor(static_cast<int>(std::floor(app.world.player.position.z))));
            const auto revision=app.world.sceneRevision();
            if (center!=app.visibleCenter || revision!=app.visibleRevision) {
                app.visibleBlocks=app.world.blocksInSectionWindow(center,2,kSectionWindowRadius);
                app.visibleCenter=center; app.visibleRevision=revision; ++app.visibleSnapshotToken;
            }
            app.renderer.render(app.visibleBlocks, app.world.player, app.hud, revision, app.visibleSnapshotToken);
        } else {
            const auto buttons=menuButtons(app,client.right,client.bottom);
            app.renderer.renderMenu(menuTitle(app.screen),buttons,app.screen==ClientApp::Screen::Title,menuSubtitle(app));
        }
        const auto spent = clock::now() - frameStart;
        if (spent < frameDuration) std::this_thread::sleep_for(frameDuration - spent);
    }
    setCursorCapture(app, false);
    app.renderer.shutdown();
    UnregisterClassW(kWindowClass, instance);
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
              << "Implemented client slice: Java classic superflat preset, creative flight, nine-slot supported-block hotbar with source-backed 3D item icons, pick-block, break/place, supported source block models, dry stone slab/stair states and oak-fence states.\n"
              << "Native UI: title, world selection/flat creation, multiplayer pregame and pause screens; multiplayer supports server status ping and an offline-mode login start only, with no authentication or configuration/gameplay handoff.\n"
              << "Omitted: complete registries/resource reload, full light propagation, entities, survival, full inventory/codecs, disk saves, integrated server, complete Java screens and full Java parity.\n";
    return 0;
}
}

int main(int argc, char** argv) try {
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
    if (argc != 1) throw std::invalid_argument("Usage: carbon [--diagnostics [--source-root <directory>]] | --random-vectors <seed>");
    return runClient(defaultSourceRoot());
} catch (const std::exception& error) {
    std::cerr << "Carbon: " << error.what() << '\n';
    return 1;
}




