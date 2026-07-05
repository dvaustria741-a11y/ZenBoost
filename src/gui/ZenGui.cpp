#include "ZenGui.h"
#include "../../include/ZenBoost.h"
#include "../features/FeatureManager.h"
#include "../hooks/InputHook.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>

// ── ZenBoost colour palette ────────────────────────────────────────────────────
//   Primary  : #7B2FFF  (electric violet)
//   Accent   : #C084FC  (soft lilac)
//   Surface0 : #0D0D1A  (deep navy-black)
//   Surface1 : #13132E  (panel bg)
//   Surface2 : #1E1E40  (card bg)
//   OnText   : #E2E8F0
//   SubText  : #94A3B8
//   Green    : #22C55E  (enabled)
//   Red      : #EF4444  (disabled)

namespace ZenGui {

static bool s_open = false;
static bool s_prevDown = false;
static int  s_screenW = 1280, s_screenH = 720;

static ImVec4 col(uint32_t hex) {
    return ImVec4(
        ((hex >> 16) & 0xFF) / 255.f,
        ((hex >> 8)  & 0xFF) / 255.f,
        ((hex)       & 0xFF) / 255.f,
        1.f);
}

// ── get screen size from EGL surface ─────────────────────────────────────────
static void refreshScreenSize() {
    EGLDisplay dpy = eglGetCurrentDisplay();
    EGLSurface surf = eglGetCurrentSurface(EGL_DRAW);
    if (dpy != EGL_NO_DISPLAY && surf != EGL_NO_SURFACE) {
        EGLint w = 0, h = 0;
        eglQuerySurface(dpy, surf, EGL_WIDTH,  &w);
        eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);
        if (w > 0 && h > 0) { s_screenW = w; s_screenH = h; }
    }
}

// ── apply ZenBoost ImGui theme ────────────────────────────────────────────────
static void applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4*     c     = style.Colors;

    style.WindowRounding    = 10.f;
    style.ChildRounding     = 8.f;
    style.FrameRounding     = 6.f;
    style.GrabRounding      = 6.f;
    style.PopupRounding     = 8.f;
    style.ScrollbarRounding = 6.f;
    style.TabRounding       = 6.f;
    style.WindowBorderSize  = 0.f;
    style.FrameBorderSize   = 0.f;
    style.ItemSpacing       = ImVec2(8, 8);
    style.FramePadding      = ImVec2(10, 6);
    style.WindowPadding     = ImVec2(14, 14);
    style.IndentSpacing     = 0.f;

    c[ImGuiCol_WindowBg]          = col(0x0D0D1A);
    c[ImGuiCol_ChildBg]           = col(0x13132E);
    c[ImGuiCol_PopupBg]           = col(0x13132E);
    c[ImGuiCol_TitleBg]           = col(0x0D0D1A);
    c[ImGuiCol_TitleBgActive]     = col(0x7B2FFF);
    c[ImGuiCol_TitleBgCollapsed]  = col(0x0D0D1A);
    c[ImGuiCol_Header]            = col(0x1E1E40);
    c[ImGuiCol_HeaderHovered]     = col(0x2D2D55);
    c[ImGuiCol_HeaderActive]      = col(0x7B2FFF);
    c[ImGuiCol_Button]            = col(0x1E1E40);
    c[ImGuiCol_ButtonHovered]     = col(0x7B2FFF);
    c[ImGuiCol_ButtonActive]      = col(0xC084FC);
    c[ImGuiCol_FrameBg]           = col(0x1E1E40);
    c[ImGuiCol_FrameBgHovered]    = col(0x2D2D55);
    c[ImGuiCol_FrameBgActive]     = col(0x7B2FFF);
    c[ImGuiCol_CheckMark]         = col(0xC084FC);
    c[ImGuiCol_SliderGrab]        = col(0x7B2FFF);
    c[ImGuiCol_SliderGrabActive]  = col(0xC084FC);
    c[ImGuiCol_Tab]               = col(0x1E1E40);
    c[ImGuiCol_TabHovered]        = col(0x7B2FFF);
    c[ImGuiCol_TabActive]         = col(0x7B2FFF);
    c[ImGuiCol_Separator]         = col(0x2D2D55);
    c[ImGuiCol_ScrollbarBg]       = col(0x0D0D1A);
    c[ImGuiCol_ScrollbarGrab]     = col(0x2D2D55);
    c[ImGuiCol_ScrollbarGrabHovered] = col(0x7B2FFF);
    c[ImGuiCol_ScrollbarGrabActive]  = col(0xC084FC);
    c[ImGuiCol_Text]              = col(0xE2E8F0);
    c[ImGuiCol_TextDisabled]      = col(0x94A3B8);
}

void init() {
    refreshScreenSize();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)s_screenW, (float)s_screenH);
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    applyTheme();
    ImGui_ImplOpenGL3_Init("#version 100");   // GLSL ES 1.00

    LOGI("[ZenGui] Initialized — screen %dx%d", s_screenW, s_screenH);
}

// ── helper: coloured feature card with toggle button ─────────────────────────
static void featureCard(const std::shared_ptr<Feature>& feat) {
    ImGui::PushID(feat->id.c_str());

    bool on = feat->enabled;

    // card background
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        on ? ImVec4(0.11f, 0.25f, 0.13f, 1.f)   // green tint
           : ImVec4(0.11f, 0.11f, 0.22f, 1.f));  // neutral

    ImGui::BeginChild("##card", ImVec2(0, 64), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // left accent bar
    ImVec2 p  = ImGui::GetCursorScreenPos();
    ImVec2 p2 = ImVec2(p.x, p.y + 64);
    ImU32 barCol = on ? IM_COL32(34, 197, 94, 255) : IM_COL32(123, 47, 255, 255);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(p.x - 14, p.y - 4),
        ImVec2(p.x - 10, p.y + 60), barCol);

    // label + description
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
    ImGui::TextColored(col(0xE2E8F0), "%s", feat->label.c_str());
    ImGui::TextColored(col(0x94A3B8), "%s", feat->description.c_str());

    // toggle button — right-aligned
    float btnW = 68.f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - btnW + ImGui::GetCursorPosX());
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing() - 2);

    if (on)
        ImGui::PushStyleColor(ImGuiCol_Button, col(0x16A34A));
    else
        ImGui::PushStyleColor(ImGuiCol_Button, col(0x3F1F7F));

    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        on ? col(0x22C55E) : col(0x7B2FFF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        on ? col(0x4ADE80) : col(0xC084FC));

    if (ImGui::Button(on ? "  ON  " : "  OFF ", ImVec2(btnW, 30))) {
        FeatureManager::toggle(feat->id);
    }
    ImGui::PopStyleColor(3);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PopID();
}

void render(float fps, float frameTimeMs) {
    // ── update ImGui IO ───────────────────────────────────────────────────────
    refreshScreenSize();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)s_screenW, (float)s_screenH);

    float tx   = InputHook::getLastTouchX();
    float ty   = InputHook::getLastTouchY();
    bool  down = InputHook::getTouchDown();

    io.MousePos     = ImVec2(tx, ty);
    io.MouseDown[0] = down;

    // ── open/close toggle via FAB button (top-right corner) ──────────────────
    bool curDown = down;
    // FAB region: top-right 80x80
    float fabX1 = s_screenW - 88.f, fabX2 = (float)s_screenW - 8.f;
    float fabY1 = 8.f,              fabY2 = 88.f;
    if (!s_prevDown && curDown && tx >= fabX1 && tx <= fabX2 && ty >= fabY1 && ty <= fabY2) {
        s_open = !s_open;
    }
    s_prevDown = curDown;

    // ── new ImGui frame ───────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // ── FAB (floating action button) — always visible ─────────────────────────
    ImGui::SetNextWindowPos(ImVec2(fabX1, fabY1), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(80, 80),       ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 40.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0,0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col(s_open ? 0x7B2FFF : 0x1E1E40));
    ImGui::Begin("##fab", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs);

    ImGui::GetWindowDrawList()->AddText(
        ImVec2(fabX1 + 20, fabY1 + 24),
        IM_COL32(192, 132, 252, 255),
        "ZB");
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // ── HUD overlay (fps + mem) ───────────────────────────────────────────────
    if (FeatureManager::isEnabled("hud")) {
        ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::Begin("##hud", nullptr,
            ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize   |
            ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoInputs   |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

        ImVec4 fpsCol = fps >= 100 ? ImVec4(0.13f,0.76f,0.37f,1) :
                        fps >=  60 ? ImVec4(1,0.84f,0,1)         :
                                     ImVec4(0.94f,0.27f,0.27f,1);
        ImGui::TextColored(fpsCol, "FPS  %.0f", fps);
        ImGui::TextColored(col(0x94A3B8), "ft   %.2f ms", frameTimeMs);

        // memory from /proc
        std::ifstream st("/proc/self/status");
        std::string ln;
        while (std::getline(st, ln)) {
            if (ln.rfind("VmRSS:", 0) == 0) {
                unsigned long kb = 0;
                sscanf(ln.c_str(), "VmRSS: %lu kB", &kb);
                ImGui::TextColored(col(0x94A3B8), "RAM  %lu MB", kb / 1024);
                break;
            }
        }
        ImGui::End();
    }

    // ── main GUI panel ────────────────────────────────────────────────────────
    if (s_open) {
        float winW = 360.f, winH = 540.f;
        ImGui::SetNextWindowPos(
            ImVec2(s_screenW - winW - 8, fabY2 + 8), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);

        ImGui::Begin("##zenboost_panel", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar);

        // ── header ──────────────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, col(0xC084FC));
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("  ZenBoost v" ZB_VERSION);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::TextColored(col(0x94A3B8), "  Minecraft Bedrock 26.30.5");
        ImGui::Separator();
        ImGui::Spacing();

        // ── FPS graph ────────────────────────────────────────────────────────
        static float fpsBuf[90] = {};
        static int   fpsIdx = 0;
        fpsBuf[fpsIdx % 90] = fps;
        fpsIdx++;
        char graphLabel[32];
        snprintf(graphLabel, sizeof(graphLabel), "%.0f FPS", fps);
        ImGui::PlotLines("##fpsgraph", fpsBuf, 90, fpsIdx % 90,
            graphLabel, 0.f, 300.f, ImVec2(winW - 28, 50));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── feature cards ────────────────────────────────────────────────────
        ImGui::BeginChild("##features", ImVec2(0, 0), false);
        for (auto& feat : FeatureManager::getAll()) {
            featureCard(feat);
        }
        ImGui::EndChild();

        ImGui::End();
    }

    // ── render ────────────────────────────────────────────────────────────────
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace ZenGui