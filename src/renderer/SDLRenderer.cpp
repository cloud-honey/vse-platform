#include "renderer/SDLRenderer.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <string>
#include <unordered_set>

// Dear ImGui 헤더 (cpp 파일에만 include)
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

namespace vse {

SDLRenderer::SDLRenderer() 
    : lastFrameTime_(0.0f)
{
}

SDLRenderer::~SDLRenderer()
{
    shutdown();
}

bool SDLRenderer::init(int windowW, int windowH, const char* title, EventBus& bus)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }
    
    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        spdlog::warn("TTF_Init failed: {}", TTF_GetError());
        // Continue without font support - fallback rendering will be used
    }

    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowW, windowH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window_) {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(
        window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer_) {
        spdlog::error("SDL_CreateRenderer failed: {}", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // Load sprite sheet
    std::string spritePath = std::string(VSE_PROJECT_ROOT) + "/content/sprites/npc_sheet.png";
    npcSheet_ = std::make_unique<SpriteSheet>(renderer_, spritePath);
    
    if (!npcSheet_->isLoaded()) {
        spdlog::warn("Sprite sheet not loaded, using fallback rendering");
    }

    // Initialize tile renderer
    std::string spritesDir = std::string(VSE_PROJECT_ROOT) + "/content/sprites/";
    tileRenderer_ = std::make_unique<TileRenderer>(renderer_);
    tileRenderer_->loadSprites(spritesDir);
    
    // Initialize elevator renderer
    elevatorRenderer_ = std::make_unique<ElevatorRenderer>(renderer_);
    elevatorRenderer_->loadSprites(spritesDir);

    // Load font
    std::string fontPath = std::string(VSE_PROJECT_ROOT) + "/content/fonts/default.ttf";
    if (!fontManager_.load(fontPath, 16)) {
        spdlog::warn("Font not loaded, floor labels will use fallback rectangles");
    }

    // Dear ImGui 초기화
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer2_Init(renderer_);

    // AudioEngine 초기화 (비치명적 — 실패해도 계속 진행)
    if (!audioEngine_.init()) {
        spdlog::warn("AudioEngine initialization failed — continuing without audio");
    }

    // EventBus 참조 저장 및 DayNightCycle 초기화
    bus_ = &bus;
    dayNightCycle_ = std::make_unique<DayNightCycle>(bus);

    spdlog::info("SDLRenderer::init OK ({}x{})", windowW, windowH);
    return true;
}

void SDLRenderer::feedEvent(const SDL_Event& event)
{
    // Bootstrapper 이벤트 루프에서 SDL_PollEvent 직후 호출
    // ImGui가 이 이벤트를 NewFrame 전에 받아야 버튼 클릭/마우스 상태 정확히 반영됨
    ImGui_ImplSDL2_ProcessEvent(&event);
}

void SDLRenderer::shutdown()
{
    // AudioEngine 정리
    audioEngine_.shutdown();
    
    // Dear ImGui 종료 (초기화된 경우에만)
    if (renderer_) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    // 텍스처 먼저 해제 (SDL_DestroyRenderer 전에 반드시 호출)
    if (tileRenderer_)     { tileRenderer_->freeTextures();     tileRenderer_.reset(); }
    if (elevatorRenderer_) { elevatorRenderer_->freeTextures(); elevatorRenderer_.reset(); }

    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow(window_);     window_   = nullptr; }
    
    // Clear label cache before TTF_Quit (textures must be freed before renderer destroyed)
    clearLabelCache();
    // Quit SDL_ttf
    TTF_Quit();
}

void SDLRenderer::render(const RenderFrame& frame, const Camera& camera)
{
    if (!renderer_) return;

    // Calculate delta time for animation
    float currentTime = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    float dt = 0.0f;
    if (lastFrameTime_ > 0.0f) {
        dt = currentTime - lastFrameTime_;
        // Clamp delta time to avoid large jumps
        if (dt > 0.1f) dt = 0.1f;
    }
    lastFrameTime_ = currentTime;

    // 배경 클리어 — 어두운 블루-그레이 (건물 대비 강화)
    SDL_SetRenderDrawColor(renderer_, 40, 44, 52, 255);
    SDL_RenderClear(renderer_);

    // 건물 배경 (건설된 층)
    drawGridBackground(frame, camera);

    // 주야간 오버레이 — 배경(하늘) 위에만, 타일보다 먼저 그려야 건물 내부에 영향 없음
    if (dayNightCycle_) {
        SDL_Color overlay = dayNightCycle_->getOverlayColor();
        if (overlay.a > 0) {
            SDL_SetRenderDrawColor(renderer_, overlay.r, overlay.g, overlay.b, overlay.a);
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer_, nullptr);
        }
    }

    // 타일 렌더링 (테넌트 등)
    drawTiles(frame, camera);

    // 엘리베이터 렌더링
    drawElevators(frame, camera);

    // 그리드 선
    if (frame.drawGrid) {
        drawGridLines(frame, camera);
    }

    // 에이전트 렌더링 (TASK-03-003: Sprite Sheet 시스템)
    drawAgents(frame, camera, dt);

    // 건설 모드 커서 SDL 오버레이 (TASK-05-001: SDL-only, ImGui tooltip은 NewFrame 이후에)
    buildCursor_.drawOverlay(renderer_, camera, frame.mouseX, frame.mouseY,
                             frame.buildMode, frame.tileSize);

    // 층 번호 라벨
    drawFloorLabels(frame, camera);

    // ImGui 프레임 시작 — feedEvent()로 이미 이벤트 전달됨
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // TASK-05-001: BuildCursor ImGui tooltip (must be inside NewFrame/Render)
    buildCursor_.drawImGui(frame.mouseX, frame.mouseY, frame.buildMode);

    // TASK-05-001: Tenant selection popup
    if (shouldOpenTenantPopup_) {
        buildCursor_.openTenantSelectPopup();
        shouldOpenTenantPopup_ = false;
    }
    
    int selectedTenantType = -1;
    if (buildCursor_.drawTenantSelectPopup(selectedTenantType)) {
        pendingTenantSelection_ = selectedTenantType;
    }

    // DebugPanel::draw() — 별도 구성요소에 위임 (Design Spec 구조)
    // drawDebugInfo=false 또는 debugPanel_.isVisible()=false 시 패널 숨김
    if (frame.drawDebugInfo) {
        debugPanel_.draw(frame);
    }

    // HUDPanel::draw() — Playing 상태에서만 렌더링 (MainMenu/Paused 등에서는 숨김)
    if (frame.gameState == GameState::Playing) {
        hudPanel_.draw(frame);
        
        // Handle HUD interactions (TASK-05-004)
        int newSpeed = hudPanel_.drawSpeedButtons(frame.gameSpeed);
        if (newSpeed != -1) {
            pendingSpeedChange_ = newSpeed;
        }
        
        int buildAction = hudPanel_.drawToolbar(frame.viewportH);
        if (buildAction != 0) {
            pendingBuildAction_ = buildAction;
        }
    }
    
    // Handle pending toast from Bootstrapper
    if (!frame.pendingToast.empty()) {
        hudPanel_.pushToast(frame.pendingToast, ToastMessage::Type::Info);
        // Note: Toast is cleared by Bootstrapper after being added to frame
    }

    // SaveLoadPanel::draw() — Save/Load UI 렌더링
    if (frame.showSaveLoadPanel) {
        // Set up callbacks
        saveLoadPanel_.onSaveRequested = [this](int slotIndex) {
            pendingSaveSlot_ = slotIndex;
        };
        saveLoadPanel_.onLoadRequested = [this](int slotIndex) {
            pendingLoadSlot_ = slotIndex;
        };
        
        // Open panel only on the first frame (not every frame) to preserve overwrite state.
        // If panel is already open, do not call open again — it resets pendingOverwriteSlot_.
        if (!saveLoadPanel_.isOpen()) {
            if (frame.saveLoadPanelSave) {
                saveLoadPanel_.openSave();
            } else {
                saveLoadPanel_.openLoad();
            }
        }
        
        // Draw panel
        saveLoadPanel_.draw(frame.saveSlotInfos);
    } else {
        saveLoadPanel_.close();
    }

    // Game state UI menus (TASK-04-005)
    drawGameStateUI(frame);

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);

    SDL_RenderPresent(renderer_);
}

void SDLRenderer::drawGridBackground(const RenderFrame& frame, const Camera& camera)
{
    // 건설된 층 영역을 어두운 배경으로
    // RenderFrame의 tiles에 포함된 층 범위를 기반으로 배경 그리기
    // Phase 1: floor 0은 항상 존재 → 전체 maxFloors 영역에 옅은 배경

    int ts = frame.tileSize;
    float zoom = camera.zoomLevel();
    
    // Draw building exterior frame (only when zoomed in enough)
    if (zoom >= 0.5f) {
        // Calculate building bounds
        float buildingLeft = camera.tileToScreenX(0);
        float buildingRight = camera.tileToScreenX(frame.floorWidth);
        float buildingTop = camera.tileToScreenY(frame.maxFloors);
        float buildingBottom = camera.tileToScreenY(0);
        float buildingWidth = buildingRight - buildingLeft;
        float buildingHeight = buildingBottom - buildingTop;
        
        // Draw outer frame (3px border)
        SDL_SetRenderDrawColor(renderer_, 120, 120, 140, 255);
        SDL_FRect outerFrame = {
            buildingLeft - 3, buildingTop - 3,
            buildingWidth + 6, buildingHeight + 6
        };
        SDL_RenderDrawRectF(renderer_, &outerFrame);
        
        // Draw inner frame (1px border)
        SDL_SetRenderDrawColor(renderer_, 180, 180, 200, 255);
        SDL_FRect innerFrame = {
            buildingLeft - 1, buildingTop - 1,
            buildingWidth + 2, buildingHeight + 2
        };
        SDL_RenderDrawRectF(renderer_, &innerFrame);
    }

    // Draw floor backgrounds with zebra pattern
    for (int f = 0; f < frame.maxFloors; ++f) {
        float sx = camera.tileToScreenX(0);
        float sy = camera.tileToScreenY(f);
        float w  = frame.floorWidth * ts * zoom;
        float h  = ts * zoom;

        // 화면 밖이면 스킵
        if (sy + h < 0 || sy > camera.viewportH()) continue;
        if (sx + w < 0 || sx > camera.viewportW()) continue;

        // Zebra pattern: alternate floor colors for better readability
        if (f % 2 == 0) {
            // Even floors (including lobby)
            if (f == 0) {
                SDL_SetRenderDrawColor(renderer_, 60, 60, 80, 200); // Lobby
            } else {
                SDL_SetRenderDrawColor(renderer_, 40, 42, 54, 180); // Darker even floors
            }
        } else {
            // Odd floors
            SDL_SetRenderDrawColor(renderer_, 45, 47, 60, 180); // Lighter odd floors
        }
        
        SDL_FRect rect = {sx, sy, w, h};
        SDL_RenderFillRectF(renderer_, &rect);
    }
}

void SDLRenderer::drawGridLines(const RenderFrame& frame, const Camera& camera)
{
    int ts = frame.tileSize;
    float zoom = camera.zoomLevel();

    // 줌이 작으면 그리드 선 생략 (0.5 이하)
    if (zoom < 0.5f) return;

    SDL_SetRenderDrawColor(renderer_, 80, 85, 100, 40);

    // 수평선 (층 경계)
    for (int f = 0; f <= frame.maxFloors; ++f) {
        float y = camera.tileToScreenY(f);
        if (y < -1 || y > camera.viewportH() + 1) continue;
        float x1 = camera.tileToScreenX(0);
        float x2 = camera.tileToScreenX(frame.floorWidth);
        SDL_RenderDrawLineF(renderer_, x1, y, x2, y);
    }

    // 수직선 (타일 경계)
    for (int x = 0; x <= frame.floorWidth; ++x) {
        float sx = camera.tileToScreenX(x);
        if (sx < -1 || sx > camera.viewportW() + 1) continue;
        float y1 = camera.tileToScreenY(0);
        float y2 = camera.tileToScreenY(frame.maxFloors - 1);
        SDL_RenderDrawLineF(renderer_, sx, y2, sx, y1);
    }
}

void SDLRenderer::drawTiles(const RenderFrame& frame, const Camera& camera)
{
    float zoom = camera.zoomLevel();
    int ts = frame.tileSize;
    float tilePx = ts * zoom;

    for (const auto& tile : frame.tiles) {
        float sx = camera.tileToScreenX(tile.x);
        float sy = camera.tileToScreenY(tile.floor);

        // 화면 밖 컬링
        if (sx + tilePx < 0 || sx > camera.viewportW()) continue;
        if (sy + tilePx < 0 || sy > camera.viewportH()) continue;

        if (tileRenderer_) tileRenderer_->drawTile(tile, sx, sy, tilePx);
    }
}

void SDLRenderer::drawElevators(const RenderFrame& frame, const Camera& camera)
{
    float zoom = camera.zoomLevel();
    int ts = frame.tileSize;
    float tilePx = ts * zoom;

    for (const auto& elev : frame.elevators) {
        // 보간 위치 사용 (Phase 1: currentFloor와 동일)
        float worldY = (elev.interpolatedFloor + 1) * ts;
        float sx = camera.worldToScreenX(static_cast<float>(elev.shaftX * ts));
        float sy = camera.worldToScreenY(worldY);

        if (elevatorRenderer_) elevatorRenderer_->drawElevator(elev, sx, sy, tilePx);
    }
}

void SDLRenderer::drawAgents(const RenderFrame& frame, const Camera& camera, float dt)
{
    // TASK-03-003: Sprite Sheet 렌더링 시스템
    // NPC 렌더링: 16×32px 스프라이트 시트 또는 폴백 컬러 박스
    //
    // 좌표 계약:
    //   agent.pos = NPC 발바닥 월드 픽셀 (좌하단 원점, Y↑)
    //   sy = worldToScreenY(pos.y) → 발바닥의 화면 y
    //   drawY = sy - npcH → 스프라이트 상단 화면 y (SDL2 좌상단 원점 기준)
    //
    // agents는 Y-sort(pos.y 오름차순) 완료 상태로 전달됨

    // 현재 프레임에 없는 에이전트 animStates_ 정리 (메모리 누수 방지)
    {
        std::unordered_set<EntityId> currentIds;
        for (const auto& agent : frame.agents) currentIds.insert(agent.id);
        animationSystem_.pruneStale(currentIds);
    }

    float zoom = camera.zoomLevel();
    int ts = frame.tileSize;

    // NPC 전체 크기: 16×32px (tileSize 기준 — 타일 절반 너비, 한 층 높이)
    float npcW = (ts * 0.5f) * zoom;
    float npcH = ts * zoom;

    for (const auto& agent : frame.agents) {
        // pos(발바닥 월드 픽셀) → 화면 좌표
        float sx    = camera.worldToScreenX(agent.pos.x);
        float sy    = camera.worldToScreenY(agent.pos.y);  // 발바닥 화면 y
        float drawY = sy - npcH;                            // 스프라이트 상단 (16×32 영역 시작)

        // 화면 밖 컬링 (전체 외곽 기준)
        if (sx + npcW < 0 || sx > camera.viewportW()) continue;
        if (drawY + npcH < 0 || drawY > camera.viewportH()) continue;

        // Get animation frame from AnimationSystem
        int frameIndex = animationSystem_.getFrame(agent.id, agent.state, dt);
        
        // Draw using sprite sheet (or fallback)
        if (npcSheet_) {
            npcSheet_->drawFrame(frameIndex, sx, drawY, npcW, npcH);
        } else {
            // Fallback to colored rectangles (should not happen if sprite sheet loaded)
            // 상태별 색상 (TASK-02-008: 고채도 색상으로 NPC 가시성 향상)
            uint8_t r, g, b;
            switch (agent.state) {
            case AgentState::Working:
                r = 0;   g = 220; b = 100;  // 밝은 초록
                break;
            case AgentState::Resting:
                r = 255; g = 220; b = 50;   // 밝은 노랑
                break;
            case AgentState::WaitingElevator:
                r = 180; g = 100; b = 255;  // 밝은 보라
                break;
            case AgentState::InElevator:
                r = 120; g = 70;  b = 180;  // 어두운 보라
                break;
            case AgentState::UsingStairs:
                r = 255; g = 165; b = 0;    // 주황 (계단 이동)
                break;
            case AgentState::Idle:
            default:
                r = 0;   g = 200; b = 220;  // 밝은 시안
                break;
            }

            // 머리 크기: 전체 높이의 상위 30% (npcH * 0.3), 16×32 규격 안에 포함
            float headH    = npcH * 0.30f;
            float bodyH    = npcH - headH;           // 몸통 = 나머지 70%
            float headW    = npcW * 0.75f;           // 머리 너비 (약간 좁게)

            // ── 머리 (상단 30%) ─────────────────────────────
            // 16×32 박스 안에 배치 — 규격 위반 없음
            float headX = sx + (npcW - headW) * 0.5f;
            SDL_SetRenderDrawColor(renderer_,
                static_cast<uint8_t>(std::min(255, static_cast<int>(r) + 50)),
                static_cast<uint8_t>(std::min(255, static_cast<int>(g) + 35)),
                static_cast<uint8_t>(std::min(255, static_cast<int>(b) + 25)),
                230);
            SDL_FRect head = {headX, drawY, headW, headH};
            SDL_RenderFillRectF(renderer_, &head);

            // ── 몸통 (하단 70%) ─────────────────────────────
            float bodyY = drawY + headH;
            SDL_SetRenderDrawColor(renderer_, r, g, b, 230);
            SDL_FRect body = {sx, bodyY, npcW, bodyH};
            SDL_RenderFillRectF(renderer_, &body);

            // ── 전체 외곽 테두리 (1px 흰색 아웃라인, NPC 가시성 향상) ────────
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 180);
            SDL_FRect outline = {sx, drawY, npcW, npcH};
            SDL_RenderDrawRectF(renderer_, &outline);
        }
    }
}

void SDLRenderer::clearLabelCache()
{
    for (auto& [k, lt] : labelCache_) {
        if (lt.tex) SDL_DestroyTexture(lt.tex);
    }
    labelCache_.clear();
}

void SDLRenderer::drawFloorLabels(const RenderFrame& frame, const Camera& camera)
{
    // Only draw labels when zoomed in enough (hysteresis: hide < 0.48, show >= 0.52)
    static bool labelsVisible = false;
    float zoom = camera.zoomLevel();
    if (!labelsVisible && zoom >= 0.52f) labelsVisible = true;
    if (labelsVisible  && zoom <  0.48f) { labelsVisible = false; clearLabelCache(); }
    if (!labelsVisible) return;

    // Label X: left edge of building (tile x=0) minus a small offset
    float buildingLeftX = camera.tileToScreenX(0);
    float labelOffsetX  = 8.0f * zoom; // 8 world-px gap, scales with zoom
    float screenX = buildingLeftX - labelOffsetX - 24.0f * zoom;

    for (int floor = 0; floor < frame.maxFloors; ++floor) {
        float floorWorldY = static_cast<float>(floor * frame.tileSize);
        float screenY = camera.worldToScreenY(floorWorldY + frame.tileSize * 0.5f);

        if (screenY < -20 || screenY > camera.viewportH() + 20) continue;

        if (fontManager_.isLoaded()) {
            // Cache miss → create texture
            if (labelCache_.find(floor) == labelCache_.end()) {
                TTF_Font* font = fontManager_.get();
                std::string label = "L" + std::to_string(floor);
                SDL_Color col = {255, 255, 255, 255};
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font, label.c_str(), col);
                if (!surf) continue;
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
                int w = surf->w, h = surf->h;
                SDL_FreeSurface(surf);
                if (!tex) continue;
                labelCache_[floor] = {tex, w, h};
            }
            auto& lt = labelCache_[floor];
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 160);
            SDL_FRect bg = {screenX - 2, screenY - lt.h * 0.5f - 2,
                            static_cast<float>(lt.w + 4), static_cast<float>(lt.h + 4)};
            SDL_RenderFillRectF(renderer_, &bg);
            SDL_FRect dst = {screenX, screenY - lt.h * 0.5f,
                             static_cast<float>(lt.w), static_cast<float>(lt.h)};
            SDL_RenderCopyF(renderer_, lt.tex, nullptr, &dst);
        } else {
            // Fallback colored rect
            SDL_SetRenderDrawColor(renderer_, 100, 100, 255, 255);
            SDL_FRect rect = {screenX - 5, screenY - 5, 10, 10};
            SDL_RenderFillRectF(renderer_, &rect);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_RenderDrawRectF(renderer_, &rect);
        }
    }
}

void SDLRenderer::drawGameStateUI(const RenderFrame& frame) {
    // Set up fullscreen window flags
    ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoDecoration | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground;
    
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    
    ImGui::Begin("Game State UI", nullptr, window_flags);
    
    switch (frame.gameState) {
        case GameState::MainMenu: {
            // Title
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.3f);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("TOWER TYCOON").x) * 0.5f);
            ImGui::Text("TOWER TYCOON");
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            // Center buttons
            float buttonWidth = 200.0f;
            float buttonHeight = 40.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("New Game", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 1;
            }
            
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("Load Game", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 2;
            }
            
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("Quit", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 3;
            }
            break;
        }
            
        case GameState::Paused: {
            // Semi-transparent background
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetWindowPos(),
                ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(),
                       ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
                IM_COL32(0, 0, 0, 128));
            
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.3f);
            
            // Title
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("PAUSED").x) * 0.5f);
            ImGui::Text("PAUSED");
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            // Center buttons
            float buttonWidth = 200.0f;
            float buttonHeight = 40.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("Resume", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 4;  // Resume
            }
            
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("Save", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 5;  // Save
            }
            
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("Main Menu", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 6;  // MainMenu
            }
            
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("Quit", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 3;  // Quit
            }
            break;
        }
            
        case GameState::GameOver: {
            // Dark background
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetWindowPos(),
                ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(),
                       ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
                IM_COL32(40, 0, 0, 200));
            
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.3f);
            
            // Title
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("GAME OVER").x) * 0.5f);
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "GAME OVER");
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            // Center buttons
            float buttonWidth = 200.0f;
            float buttonHeight = 40.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("New Game", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 1;
            }
            
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("Quit", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 3;
            }
            break;
        }
            
        case GameState::Victory: {
            // Victory background
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetWindowPos(),
                ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(),
                       ImGui::GetWindowPos().y + ImGui::GetWindowHeight()),
                IM_COL32(0, 40, 0, 200));
            
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.3f);
            
            // Title
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("VICTORY!").x) * 0.5f);
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "VICTORY!");
            
            ImGui::Spacing();
            
            // Message
            const char* message = "You've built a successful tower!";
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(message).x) * 0.5f);
            ImGui::Text("%s", message);
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            // Center buttons
            float buttonWidth = 200.0f;
            float buttonHeight = 40.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("New Game", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 1;
            }
            
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            
            if (ImGui::Button("Quit", ImVec2(buttonWidth, buttonHeight))) {
                pendingMenuAction_ = 3;
            }
            break;
        }
            
        case GameState::Playing:
            // No UI overlay in playing state
            break;
    }
    
    ImGui::End();
}

// TASK-05-001: Tenant selection handling methods
bool SDLRenderer::checkTenantSelection(int& outTenantType)
{
    if (pendingTenantSelection_ >= 0) {
        outTenantType = pendingTenantSelection_;
        pendingTenantSelection_ = -1;
        return true;
    }
    return false;
}

void SDLRenderer::setShouldOpenTenantPopup(bool open)
{
    shouldOpenTenantPopup_ = open;
}

bool SDLRenderer::checkPendingSave(int& outSlotIndex)
{
    if (pendingSaveSlot_ >= 0) {
        outSlotIndex = pendingSaveSlot_;
        pendingSaveSlot_ = -1;
        return true;
    }
    return false;
}

bool SDLRenderer::checkPendingLoad(int& outSlotIndex)
{
    if (pendingLoadSlot_ >= 0) {
        outSlotIndex = pendingLoadSlot_;
        pendingLoadSlot_ = -1;
        return true;
    }
    return false;
}

bool SDLRenderer::checkPendingMenuAction(int& outAction)
{
    if (pendingMenuAction_ != 0) {
        outAction = pendingMenuAction_;
        pendingMenuAction_ = 0;
        return true;
    }
    return false;
}

bool SDLRenderer::checkPendingBuildAction(int& outBuildAction, int& outTenantType)
{
    if (pendingBuildAction_ != 0) {
        outBuildAction = pendingBuildAction_;
        outTenantType = 0; // Default, will be set based on action
        
        // Map toolbar action to tenant type
        if (pendingBuildAction_ == 1) {
            // Floor build
            outTenantType = 0; // Not used for floor
        } else if (pendingBuildAction_ >= 2 && pendingBuildAction_ <= 4) {
            // Tenant types: 2=office, 3=residential, 4=commercial
            // Map to tenant type: 0=office, 1=residential, 2=commercial
            outTenantType = pendingBuildAction_ - 2;
        }
        
        pendingBuildAction_ = 0;
        return true;
    }
    return false;
}

bool SDLRenderer::checkPendingSpeedChange(int& outSpeedMultiplier)
{
    if (pendingSpeedChange_ != -1) {
        outSpeedMultiplier = pendingSpeedChange_;
        pendingSpeedChange_ = -1;
        return true;
    }
    return false;
}

} // namespace vse
