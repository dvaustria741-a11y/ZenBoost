#include "ZenGui.h"
#include "../../include/ZenBoost.h"
#include "../features/FeatureManager.h"
#include "../hooks/InputHook.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Palette (matches ChatGPT spec exactly)
// ─────────────────────────────────────────────────────────────────────────────
#define C32A(r,g,b,a) IM_COL32(r,g,b,a)
#define HEX(h,a)      IM_COL32(((h)>>16)&0xFF, ((h)>>8)&0xFF, (h)&0xFF, a)

static constexpr ImU32
    BG         = HEX(0x0D0D1A, 255),
    SURFACE    = HEX(0x141428, 255),
    SURFACE2   = HEX(0x1A0F33, 255),
    BORDER     = HEX(0x2A2350, 255),
    VIOLET     = HEX(0x7B2FFF, 255),
    LILAC      = HEX(0xC084FC, 255),
    TEXT_PRI   = HEX(0xE2E8F0, 255),
    TEXT_SEC   = HEX(0x94A3B8, 255),
    TEXT_DIM   = HEX(0x6B7280, 255),
    CYAN_      = HEX(0x22D3EE, 255),
    GREEN_     = HEX(0x00E6A8, 255),
    RED_       = HEX(0xEF4444, 255),
    CARD_DIS   = HEX(0x0F0F1E, 255),

    GLOW_40    = HEX(0x7B2FFF, 102),
    GLOW_20    = HEX(0x7B2FFF,  51),
    GLOW_10    = HEX(0x7B2FFF,  25),
    LIL_20     = HEX(0xC084FC,  51),
    VIOLET_BDR = HEX(0x7B2FFF, 128);

namespace ZenGui {

// ── state ─────────────────────────────────────────────────────────────────────
static bool  s_open     = false;
static bool  s_prevDown = false;
static int   s_sw = 1280, s_sh = 720;
static float s_fabAnim  = 0.f;   // 0..1 open/close anim progress (unused, placeholder)

// FPS history for graph
static constexpr int FPS_BUF = 120;
static float  s_fpsBuf[FPS_BUF] = {};
static int    s_fpsIdx  = 0;
static float  s_avgFps  = 0.f, s_peakFps = 0.f;

// ── helpers ───────────────────────────────────────────────────────────────────
static void refreshScreen() {
    EGLDisplay dpy  = eglGetCurrentDisplay();
    EGLSurface surf = eglGetCurrentSurface(EGL_DRAW);
    if (dpy != EGL_NO_DISPLAY && surf != EGL_NO_SURFACE) {
        EGLint w = 0, h = 0;
        eglQuerySurface(dpy, surf, EGL_WIDTH, &w);
        eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);
        if (w > 0 && h > 0) { s_sw = w; s_sh = h; }
    }
}

// Simulate glow: draw N expanding transparent rects behind a card
static void drawGlow(ImDrawList* dl, ImVec2 p1, ImVec2 p2, float r, float maxSize = 16.f) {
    for (int i = 3; i >= 1; --i) {
        float s = (maxSize / 3.f) * i;
        ImU32 c = HEX(0x7B2FFF, (int)(30 / i));
        dl->AddRectFilled(
            ImVec2(p1.x - s, p1.y - s),
            ImVec2(p2.x + s, p2.y + s),
            c, r + s);
    }
}

// Section label: "● TITLE" + right-aligned extra text
static void sectionHeader(ImDrawList* dl, const char* label,
                           const char* right = nullptr, float winX = 0, float winW = 0) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    // dot indicator
    dl->AddCircleFilled(ImVec2(pos.x + 5, pos.y + 7), 4.f, VIOLET);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16);
    ImGui::TextColored(ImVec4(0.88f,0.91f,0.94f,1), "%s", label);

    if (right && winW > 0) {
        auto* font = ImGui::GetFont();
        float tw = ImGui::CalcTextSize(right).x;
        ImVec2 rp = ImVec2(winX + winW - tw - 4, pos.y);
        dl->AddText(rp, TEXT_SEC, right);
    }

    ImGui::Spacing();

    // Divider: left half dim, right half dim, with violet centre
    pos = ImGui::GetCursorScreenPos();
    float x1 = pos.x, x2 = x1 + ImGui::GetContentRegionAvail().x;
    float mid = (x1 + x2) * 0.5f, y = pos.y + 2;
    dl->AddLine(ImVec2(x1, y), ImVec2(mid - 16, y), BORDER, 1.f);
    dl->AddLine(ImVec2(mid - 16, y), ImVec2(mid + 16, y), VIOLET, 1.5f);
    dl->AddLine(ImVec2(mid + 16, y), ImVec2(x2, y), BORDER, 1.f);
    // subtle glow on centre pulse
    dl->AddLine(ImVec2(mid - 8, y), ImVec2(mid + 8, y), HEX(0x7B2FFF, 80), 3.f);
    ImGui::Dummy(ImVec2(0, 6));
}

// Stat box: icon char + value + unit label
static void statBox(ImDrawList* dl, ImVec2 p, float w, float h,
                    const char* icon, const char* value, const char* unit,
                    ImU32 valCol = TEXT_PRI) {
    // card bg
    dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), SURFACE, 10.f);
    dl->AddRect(      p, ImVec2(p.x+w, p.y+h), BORDER,  10.f, 0, 1.f);

    float cx = p.x + w * 0.5f;
    // icon
    dl->AddText(ImVec2(cx - ImGui::CalcTextSize(icon).x * 0.5f, p.y + 5),
                LILAC, icon);
    // value
    float vw = ImGui::CalcTextSize(value).x;
    dl->AddText(ImVec2(cx - vw * 0.5f, p.y + h * 0.38f), valCol, value);
    // unit
    float uw = ImGui::CalcTextSize(unit).x;
    dl->AddText(ImVec2(cx - uw * 0.5f, p.y + h - 16.f), TEXT_DIM, unit);
}

// System bar: icon + label + percentage + progress bar
static void sysBar(ImDrawList* dl, ImVec2 p, float w,
                   const char* icon, const char* label,
                   const char* pct, float fill, ImU32 barCol) {
    float h = 44.f;
    dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), SURFACE, 8.f);
    dl->AddRect(      p, ImVec2(p.x+w, p.y+h), BORDER,  8.f, 0, 1.f);

    // icon
    dl->AddText(ImVec2(p.x + 8, p.y + 6), LILAC, icon);
    // label
    float lx = p.x + 8 + ImGui::CalcTextSize(icon).x + 6;
    dl->AddText(ImVec2(lx, p.y + 7), TEXT_SEC, label);
    // percent
    float pw = ImGui::CalcTextSize(pct).x;
    dl->AddText(ImVec2(p.x + w - pw - 8, p.y + 7), TEXT_PRI, pct);
    // progress bar
    float bx1 = lx, bx2 = p.x + w - 8, by = p.y + h - 10.f;
    dl->AddRectFilled(ImVec2(bx1, by), ImVec2(bx2, by + 4), BORDER, 2.f);
    dl->AddRectFilled(ImVec2(bx1, by), ImVec2(bx1 + (bx2-bx1)*fill, by + 4), barCol, 2.f);
}

// Feature card with glow toggle
static void featureCard(ImDrawList* dl, ImVec2 p, float cw, float ch,
                         const std::shared_ptr<Feature>& feat, int& toggleID) {
    bool on = feat->enabled;
    ImVec2 p2 = ImVec2(p.x + cw, p.y + ch);

    // glow behind card when enabled
    if (on) drawGlow(dl, p, p2, 14.f, 12.f);

    // card background
    dl->AddRectFilled(p, p2, on ? SURFACE : CARD_DIS, 14.f);

    // border: violet when on, dim when off
    ImU32 bdr = on ? VIOLET_BDR : BORDER;
    dl->AddRect(p, p2, bdr, 14.f, 0, 1.f);

    // left accent strip when enabled
    if (on) {
        dl->AddRectFilled(
            ImVec2(p.x, p.y + 10),
            ImVec2(p.x + 3, p.y + ch - 10),
            VIOLET, 2.f);
    }

    // abbrev label (2-3 chars, large) — icon stand-in
    // abbrev from first letters of words
    std::string abbr;
    bool nextUpper = true;
    for (char c : feat->label) {
        if (c == ' ') { nextUpper = true; }
        else if (nextUpper) { abbr += c; nextUpper = false; }
        if (abbr.size() >= 2) break;
    }
    if (abbr.empty()) abbr = feat->label.substr(0, 2);

    float scale = 1.6f;
    ImGui::SetWindowFontScale(scale);
    float aw = ImGui::CalcTextSize(abbr.c_str()).x;
    ImGui::SetWindowFontScale(1.0f);
    // draw large abbr centered in top portion
    ImU32 labelCol = on ? LILAC : HEX(0x94A3B8, 100);
    ImVec2 lpos = ImVec2(p.x + (cw - aw) * 0.5f, p.y + 12);
    // manual scale via font size isn't easy — use two-line label instead
    dl->AddText(ImVec2(p.x + (cw - ImGui::CalcTextSize(abbr.c_str()).x)*0.5f,
                       p.y + 10),
                labelCol, abbr.c_str());

    // short desc (first word only)
    std::string shortDesc = feat->label;
    if (shortDesc.length() > 10) shortDesc = shortDesc.substr(0,9) + "..";
    float dw = ImGui::CalcTextSize(shortDesc.c_str()).x;
    dl->AddText(ImVec2(p.x + (cw - dw)*0.5f, p.y + 28),
                on ? TEXT_PRI : TEXT_SEC, shortDesc.c_str());

    // circle toggle indicator at bottom
    float cx = p.x + cw * 0.5f, cy = p.y + ch - 14.f;
    float r = 5.f;
    if (on) {
        dl->AddCircleFilled(ImVec2(cx, cy), r, VIOLET);
        dl->AddCircle(      ImVec2(cx, cy), r + 2.f, HEX(0x7B2FFF, 80), 16, 1.f);
    } else {
        dl->AddCircle(ImVec2(cx, cy), r, BORDER, 16, 1.f);
    }

    // invisible ImGui button for interaction
    ImGui::SetCursorScreenPos(p);
    ImGui::PushID(toggleID++);
    if (ImGui::InvisibleButton("##card", ImVec2(cw, ch))) {
        FeatureManager::toggle(feat->id);
    }
    ImGui::PopID();
}

// ── ImGui theme ───────────────────────────────────────────────────────────────
static void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 14.f;
    s.ChildRounding     = 10.f;
    s.FrameRounding     = 8.f;
    s.GrabRounding      = 8.f;
    s.WindowBorderSize  = 0.f;
    s.FrameBorderSize   = 0.f;
    s.ItemSpacing       = ImVec2(6, 6);
    s.FramePadding      = ImVec2(8, 5);
    s.WindowPadding     = ImVec2(14, 12);

    ImVec4* c = s.Colors;
    auto hf = [](uint32_t h) {
        return ImVec4(((h>>16)&0xFF)/255.f, ((h>>8)&0xFF)/255.f, (h&0xFF)/255.f, 1.f);
    };
    c[ImGuiCol_WindowBg]         = hf(0x0D0D1A);
    c[ImGuiCol_ChildBg]          = hf(0x0D0D1A);
    c[ImGuiCol_Text]             = hf(0xE2E8F0);
    c[ImGuiCol_TextDisabled]     = hf(0x6B7280);
    c[ImGuiCol_Border]           = hf(0x2A2350);
    c[ImGuiCol_FrameBg]          = hf(0x141428);
    c[ImGuiCol_PlotLines]        = hf(0xC084FC);
    c[ImGuiCol_PlotLinesHovered] = hf(0x7B2FFF);
    c[ImGuiCol_ScrollbarBg]      = hf(0x0D0D1A);
    c[ImGuiCol_ScrollbarGrab]    = hf(0x2A2350);
    c[ImGuiCol_ScrollbarGrabHovered] = hf(0x7B2FFF);
    c[ImGuiCol_Separator]        = hf(0x2A2350);
}

// ── public API ────────────────────────────────────────────────────────────────
void init() {
    refreshScreen();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)s_sw, (float)s_sh);
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    applyTheme();
    ImGui_ImplOpenGL3_Init("#version 100");
    LOGI("[ZenGui] Init %dx%d", s_sw, s_sh);
}

void render(float fps, float frameTimeMs) {
    refreshScreen();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)s_sw, (float)s_sh);

    // ── touch passthrough ─────────────────────────────────────────────────
    float tx = InputHook::getLastTouchX();
    float ty = InputHook::getLastTouchY();
    bool  dn = InputHook::getTouchDown();
    io.MousePos    = ImVec2(tx, ty);
    io.MouseDown[0] = dn;

    // ── FPS history ───────────────────────────────────────────────────────
    s_fpsBuf[s_fpsIdx % FPS_BUF] = fps;
    s_fpsIdx++;
    s_avgFps = s_peakFps = 0.f;
    for (float v : s_fpsBuf) {
        s_avgFps += v / FPS_BUF;
        if (v > s_peakFps) s_peakFps = v;
    }

    // ── FAB hit test (top-right, 64x64) ──────────────────────────────────
    float fabSz = 64.f;
    float fabX  = s_sw - fabSz - 12.f;
    float fabY  = 12.f;
    if (!s_prevDown && dn &&
        tx >= fabX && tx <= fabX + fabSz &&
        ty >= fabY && ty <= fabY + fabSz)
        s_open = !s_open;
    s_prevDown = dn;

    // ── new frame ─────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    auto* bg_dl = ImGui::GetBackgroundDrawList();

    // ════════════════════════════════════════════════════════════════════════
    //  FAB — superellipse squircle button (top-right)
    // ════════════════════════════════════════════════════════════════════════
    {
        ImVec2 fc = ImVec2(fabX + fabSz*0.5f, fabY + fabSz*0.5f);
        float  fr = fabSz * 0.5f;

        // glow ring
        bg_dl->AddCircleFilled(fc, fr + 10.f, HEX(0x7B2FFF, 40), 64);
        bg_dl->AddCircleFilled(fc, fr + 5.f,  HEX(0x7B2FFF, 60), 64);
        // gradient-ish fill: two overlapping circles
        bg_dl->AddCircleFilled(fc, fr, HEX(0x1A0F33, 255), 64);
        // violet overlay (top portion lighter)
        bg_dl->AddCircleFilled(
            ImVec2(fc.x, fc.y - fr*0.3f),
            fr * 0.85f, HEX(0x7B2FFF, 120), 64);
        // border
        bg_dl->AddCircle(fc, fr, VIOLET_BDR, 64, 1.5f);

        // "ZB" label centered
        const char* lbl = s_open ? "X" : "ZB";
        float lw = ImGui::CalcTextSize(lbl).x;
        bg_dl->AddText(ImVec2(fc.x - lw*0.5f, fc.y - 7.f), LILAC, lbl);
    }

    // ════════════════════════════════════════════════════════════════════════
    //  HUD (top-left) — always visible
    // ════════════════════════════════════════════════════════════════════════
    if (FeatureManager::isEnabled("hud")) {
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::Begin("##hud", nullptr,
            ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoScrollbar  | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings);

        auto* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float  w = 140.f, h = 56.f;

        // frosted card
        dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), HEX(0x0D0D1A, 180), 10.f);
        dl->AddRect(      p, ImVec2(p.x+w, p.y+h), BORDER,             10.f, 0, 1.f);
        // left violet strip
        dl->AddRectFilled(p, ImVec2(p.x+3, p.y+h), VIOLET, 2.f);

        // FPS colour: green ≥100, yellow ≥60, red <60
        ImU32 fpsCol = fps >= 100 ? GREEN_ : fps >= 60 ? HEX(0xFBBF24,255) : RED_;

        char fpsStr[16], ftStr[16];
        snprintf(fpsStr, sizeof(fpsStr), "%.0f FPS", fps);
        snprintf(ftStr,  sizeof(ftStr),  "%.1f ms",  frameTimeMs);

        dl->AddText(ImVec2(p.x+10, p.y+8),  fpsCol,   fpsStr);
        dl->AddText(ImVec2(p.x+10, p.y+26), TEXT_SEC, ftStr);

        // RAM
        unsigned long ramKB = 0;
        { std::ifstream f("/proc/self/status"); std::string ln;
          while (std::getline(f,ln)) if (ln.rfind("VmRSS:",0)==0)
              { sscanf(ln.c_str(),"VmRSS: %lu kB",&ramKB); break; } }
        char ramStr[16]; snprintf(ramStr,sizeof(ramStr),"%lu MB",ramKB/1024);
        dl->AddText(ImVec2(p.x+80, p.y+8), TEXT_DIM, ramStr);

        ImGui::Dummy(ImVec2(w, h));
        ImGui::End();
    }

    if (!s_open) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }

    // ════════════════════════════════════════════════════════════════════════
    //  Main panel
    // ════════════════════════════════════════════════════════════════════════
    float panW = 360.f;
    float panH = (float)s_sh - fabY - fabSz - 20.f;
    if (panH > 640.f) panH = 640.f;
    float panX = (float)s_sw - panW - 12.f;
    float panY = fabY + fabSz + 8.f;

    ImGui::SetNextWindowPos( ImVec2(panX, panY));
    ImGui::SetNextWindowSize(ImVec2(panW, panH));
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##zenboost", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    auto* dl = ImGui::GetWindowDrawList();
    ImVec2 panPos = ImGui::GetWindowPos();

    // ── panel background + border ─────────────────────────────────────────
    dl->AddRectFilled(panPos, ImVec2(panPos.x+panW, panPos.y+panH),
                      HEX(0x0D0D1A, 230), 16.f);
    dl->AddRect(panPos, ImVec2(panPos.x+panW, panPos.y+panH),
                BORDER, 16.f, 0, 1.f);
    // top violet accent line
    dl->AddRectFilled(ImVec2(panPos.x+16, panPos.y),
                      ImVec2(panPos.x+panW-16, panPos.y+2), VIOLET, 1.f);

    ImGui::BeginChild("##scroll", ImVec2(0, 0), false,
        ImGuiWindowFlags_NoScrollbar);

    // ── header ────────────────────────────────────────────────────────────
    {
        ImVec2 hp = ImGui::GetCursorScreenPos();
        float  hh = 52.f;
        dl->AddRectFilled(hp, ImVec2(hp.x + panW, hp.y + hh),
                          HEX(0x0D0D1A, 0), 0.f);  // transparent, use panel bg

        // logo dot + ZENBOOST
        dl->AddCircleFilled(ImVec2(hp.x + 14, hp.y + 26), 5.f, VIOLET);
        dl->AddCircle(      ImVec2(hp.x + 14, hp.y + 26), 8.f, HEX(0x7B2FFF, 60), 16, 1.f);

        dl->AddText(ImVec2(hp.x + 28, hp.y + 10), LILAC,    "ZEN");
        float zenW = ImGui::CalcTextSize("ZEN").x;
        dl->AddText(ImVec2(hp.x + 28 + zenW, hp.y + 10), TEXT_PRI, "BOOST");
        dl->AddText(ImVec2(hp.x + 28, hp.y + 28), TEXT_DIM, "v" ZB_VERSION "  Bedrock 26.30.5");

        // settings icon placeholder (right side)
        float sx = hp.x + panW - 36.f;
        dl->AddRectFilled(ImVec2(sx, hp.y+14), ImVec2(sx+24, hp.y+38), SURFACE, 6.f);
        dl->AddRect(      ImVec2(sx, hp.y+14), ImVec2(sx+24, hp.y+38), BORDER,  6.f, 0, 1.f);
        dl->AddText(ImVec2(sx+4, hp.y+18), TEXT_DIM, "cfg");

        ImGui::Dummy(ImVec2(panW, hh));

        // header divider
        ImVec2 dp = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(dp.x, dp.y), ImVec2(dp.x+panW-28, dp.y), BORDER, 1.f);
        ImGui::Dummy(ImVec2(0, 6));
    }

    // ── PERFORMANCE section ───────────────────────────────────────────────
    {
        char rightLabel[48];
        snprintf(rightLabel, sizeof(rightLabel), "AVG %.0f  PEAK %.0f",
                 s_avgFps, s_peakFps);
        sectionHeader(dl, "PERFORMANCE", rightLabel, panPos.x, panW - 14);

        // FPS graph with violet fill effect
        ImVec2 gp = ImGui::GetCursorScreenPos();
        float  gw = panW - 28.f, gh = 60.f;

        // graph bg
        dl->AddRectFilled(gp, ImVec2(gp.x+gw, gp.y+gh), SURFACE, 8.f);

        // axis labels
        dl->AddText(ImVec2(gp.x, gp.y-12),       TEXT_DIM, "FPS");
        dl->AddText(ImVec2(gp.x+gw-14, gp.y+gh+2), TEXT_DIM, "0s");
        dl->AddText(ImVec2(gp.x, gp.y+gh+2),     TEXT_DIM, "60s");

        // PlotLines with custom config
        ImGui::SetNextItemWidth(gw);
        ImGui::PlotLines("##fps", s_fpsBuf, FPS_BUF, s_fpsIdx % FPS_BUF,
                         nullptr, 0.f, 300.f, ImVec2(gw, gh));

        // peak marker — find highest point
        int peakIdx = 0; float peakVal = 0;
        for (int i = 0; i < FPS_BUF; ++i) if (s_fpsBuf[i] > peakVal) { peakVal=s_fpsBuf[i]; peakIdx=i; }
        if (peakVal > 0) {
            float px = gp.x + gw * (float)((peakIdx - s_fpsIdx%FPS_BUF + FPS_BUF) % FPS_BUF) / FPS_BUF;
            float py = gp.y + gh * (1.f - peakVal / 300.f);
            dl->AddCircleFilled(ImVec2(px, py), 4.f, VIOLET);
            char pk[12]; snprintf(pk, sizeof(pk), "%.0f", peakVal);
            dl->AddText(ImVec2(px - 8, py - 14), LILAC, pk);
        }

        ImGui::Dummy(ImVec2(0, gh));  // account for PlotLines consuming space
        ImGui::Spacing();

        // ── stat boxes: FPS | MS | TEMP | CPU ─────────────────────────────
        float bw = (panW - 28.f - 9.f) / 4.f, bh = 54.f;
        ImVec2 bp = ImGui::GetCursorScreenPos();

        // get cpu/temp from /proc
        unsigned long cpuIdle=0,cpuTotal=0;
        { std::ifstream f("/proc/stat"); std::string ln;
          if (std::getline(f,ln)) {
              unsigned long u,n,s,i,iow,irq,sirq;
              sscanf(ln.c_str(),"cpu %lu %lu %lu %lu %lu %lu %lu",&u,&n,&s,&i,&iow,&irq,&sirq);
              cpuIdle = i; cpuTotal = u+n+s+i+iow+irq+sirq;
          }}
        static unsigned long lastIdle=0, lastTotal=0;
        float cpuPct = 0.f;
        if (lastTotal > 0 && cpuTotal > lastTotal) {
            cpuPct = 100.f * (1.f - (float)(cpuIdle-lastIdle)/(cpuTotal-lastTotal));
        }
        lastIdle = cpuIdle; lastTotal = cpuTotal;

        char fStr[12],msStr[12],cpuStr[12];
        snprintf(fStr,  sizeof(fStr),  "%.0f",   fps);
        snprintf(msStr, sizeof(msStr), "%.1f",   frameTimeMs);
        snprintf(cpuStr,sizeof(cpuStr),"%.0f%%", cpuPct);

        ImU32 fCol = fps >= 100 ? GREEN_ : fps >= 60 ? HEX(0xFBBF24,255) : RED_;
        statBox(dl, bp,                          bw, bh, "~",   fStr,   "FPS",  fCol);
        statBox(dl, ImVec2(bp.x+bw+3, bp.y),    bw, bh, "t",   msStr,  "MS",   TEXT_PRI);
        statBox(dl, ImVec2(bp.x+2*(bw+3), bp.y),bw, bh, "T",   "40",   "C",    HEX(0xFBBF24,255));
        statBox(dl, ImVec2(bp.x+3*(bw+3), bp.y),bw, bh, "#",   cpuStr, "CPU",  TEXT_PRI);
        ImGui::Dummy(ImVec2(panW-28, bh));
        ImGui::Spacing();
    }

    // ── FEATURES section ──────────────────────────────────────────────────
    {
        sectionHeader(dl, "FEATURES");

        auto& feats = FeatureManager::getAll();
        int cols = 4;
        float gap = 6.f;
        float cw  = (panW - 28.f - gap * (cols - 1)) / cols;
        float ch  = 68.f;
        int   tid = 200;

        for (int i = 0; i < (int)feats.size(); ++i) {
            int col = i % cols, row = i / cols;
            if (col == 0 && row > 0) ImGui::Dummy(ImVec2(0, ch + gap));

            ImVec2 base = ImGui::GetCursorScreenPos();
            ImVec2 cp = ImVec2(base.x + col * (cw + gap),
                               (row == 0 ? base.y : base.y)); // simplified
            if (col > 0) {
                // Position manually via SetCursorScreenPos for multi-col
                // Actually just let same-row items be placed by accumulating
            }

            // For a 4-column grid we manage layout manually:
            if (col == 0) {
                // start of new row: use Dummy+SetCursorScreenPos
                ImVec2 rowPos = ImGui::GetCursorScreenPos();
                for (int c = 0; c < cols && i+c < (int)feats.size(); ++c) {
                    ImVec2 cp2 = ImVec2(rowPos.x + c*(cw+gap), rowPos.y);
                    featureCard(dl, cp2, cw, ch, feats[i+c], tid);
                }
                i += (cols - 1); // skip the rest of this row (loop increments +1)
                ImGui::Dummy(ImVec2(panW - 28.f, ch));
                ImGui::Spacing();
            }
        }
        ImGui::Spacing();
    }

    // ── SYSTEM section ────────────────────────────────────────────────────
    {
        sectionHeader(dl, "SYSTEM");

        unsigned long ramKB = 0, ramTotalKB = 1;
        { std::ifstream f("/proc/self/status"); std::string ln;
          while (std::getline(f,ln)) if (ln.rfind("VmRSS:",0)==0)
              { sscanf(ln.c_str(),"VmRSS: %lu kB",&ramKB); break; } }
        { std::ifstream f("/proc/meminfo"); std::string ln;
          while (std::getline(f,ln)) if (ln.rfind("MemTotal:",0)==0)
              { sscanf(ln.c_str(),"MemTotal: %lu kB",&ramTotalKB); break; } }

        float ramFill  = (float)ramKB / ramTotalKB;

        // battery from sysfs
        int   batPct   = 80;
        { std::ifstream f("/sys/class/power_supply/battery/capacity");
          f >> batPct; }

        float sw2 = (panW - 28.f - 6.f) * 0.5f;
        ImVec2 sp = ImGui::GetCursorScreenPos();

        char ramStr[16], batStr[16];
        snprintf(ramStr, sizeof(ramStr), "%lu%%", (ramKB*100)/ramTotalKB);
        snprintf(batStr, sizeof(batStr), "%d%%",   batPct);

        sysBar(dl, sp,                          sw2, "M", "MEM",  ramStr, ramFill, CYAN_);
        sysBar(dl, ImVec2(sp.x+sw2+6, sp.y),   sw2, "G", "GPU",  "N/A",  0.58f,   VIOLET);
        ImGui::Dummy(ImVec2(panW-28, 44)); ImGui::Spacing();
        sp = ImGui::GetCursorScreenPos();
        sysBar(dl, sp,                          sw2, "B", "BAT",  batStr, batPct/100.f, GREEN_);
        sysBar(dl, ImVec2(sp.x+sw2+6, sp.y),   sw2, "T", "TEMP", "40C",  0.40f,   HEX(0xFBBF24,255));
        ImGui::Dummy(ImVec2(panW-28, 44)); ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace ZenGui