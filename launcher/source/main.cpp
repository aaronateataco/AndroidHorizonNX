#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <sys/stat.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include "apk.h"
#include "avatar.h"
#include "build_number.h"

static const char* APK_DIR = "sdmc:/AndroidHorizonNX/apks";

// Translation Core NROs live in a subfolder next to this launcher — same
// convention as any other homebrew "app + resources" layout. x64 is real;
// x32 is a placeholder (32-bit binaries aren't supported yet — see below).
static const char* CORE_X64_PATH = "sdmc:/switch/AndroidHorizonNX/AHNX-Translation-Core-x64.nro";
static const char* CORE_X32_PATH = "sdmc:/switch/AndroidHorizonNX/AHNX-Translation-Core-x32.nro";

// ---------------------------------------------------------------------------
// Layout (1280×720) — matches the Translation Core's own UI exactly so the
// chain-load between the two feels like a single continuous app.
// ---------------------------------------------------------------------------
static const int SW       = 1280;
static const int SH       = 720;
static const int HEADER_H = 72;
static const int FOOTER_H = 48;
static const int LIST_Y   = HEADER_H;
static const int LIST_H   = SH - HEADER_H - FOOTER_H;
static const int ITEM_H   = 108;
static const int ICON_SZ  = 84;
static const int VISIBLE  = LIST_H / ITEM_H;

static const SDL_Color C_BG     = {13,  12,  46,  255};
static const SDL_Color C_HEADER = {24,  22,  86,  255};
static const SDL_Color C_FOOTER = {9,   8,   34,  255};
static const SDL_Color C_SEL    = {41,  37,  128, 255};
static const SDL_Color C_DIV    = {40,  37,  108, 255};
static const SDL_Color C_WHITE  = {255, 255, 255, 255};
static const SDL_Color C_GRAY   = {168, 170, 205, 255};
static const SDL_Color C_DIM    = {116, 116, 168, 255};
static const SDL_Color C_OK     = {52,  230, 134, 255};
static const SDL_Color C_ERR    = {235, 90,  90,  255};
static const SDL_Color C_WARN   = {230, 190, 70,  255};
static const SDL_Color C_INST   = {47,  223, 124, 255};
static const SDL_Color C_RIM    = {52,  230, 134, 255};

// ---------------------------------------------------------------------------
static FILE* g_log = nullptr;
static void logOpen()  { g_log = fopen("sdmc:/AndroidHorizonNX/launcher_log.txt", "w"); }
static void logClose() { if (g_log) { fclose(g_log); g_log = nullptr; } }
static void logMsg(const char* msg) {
    if (g_log) { fputs(msg, g_log); fputc('\n', g_log); fflush(g_log); }
}
static void logSDL(const char* prefix) {
    if (!g_log) return;
    fputs(prefix, g_log); fputs(": ", g_log);
    fputs(SDL_GetError(), g_log); fputc('\n', g_log); fflush(g_log);
}

static const int BTN_A     = 0;
static const int BTN_B     = 1;
static const int BTN_X     = 2;
static const int BTN_Y     = 3;
static const int BTN_PLUS  = 10;
static const int BTN_MINUS = 11;

// ---------------------------------------------------------------------------
struct App {
    SDL_Window*    win  = nullptr;
    SDL_Renderer*  rdr  = nullptr;
    TTF_Font*      fLg  = nullptr;
    TTF_Font*      fMd  = nullptr;
    TTF_Font*      fSm  = nullptr;
    SDL_Joystick*  joy  = nullptr;

    std::vector<ApkInfo>      apks;
    std::vector<SDL_Texture*> icons;
    int selected = 0;
    int scroll   = 0;

    SDL_Texture* avatarTex = nullptr;

    SDL_Texture* bgTex = nullptr;
    TTF_Font*    fBtn  = nullptr;
    struct Star { float x, y; int sz; float phase, speed; };
    std::vector<Star> stars;
    float selAnimY = -1.0f;

    Uint32 noticeUntil = 0;
    std::string noticeText;

    // ------------------------------------------------------------------
    TTF_Font* openFont(int ptsize) {
        plInitialize(PlServiceType_User);
        PlFontData fd = {};
        if (plGetSharedFontByType(&fd, PlSharedFontType_Standard) == 0 && fd.size > 8) {
            SDL_RWops* rw = SDL_RWFromConstMem(
                (const uint8_t*)fd.address + 8, (int)fd.size - 8);
            TTF_Font* f = TTF_OpenFontRW(rw, 1, ptsize);
            if (f) { logMsg("  font: system BFTTF"); return f; }
            logSDL("  BFTTF open failed");
        }
        romfsInit();
        TTF_Font* f = TTF_OpenFont("romfs:/fonts/DejaVuSans.ttf", ptsize);
        if (f) { logMsg("  font: romfs DejaVuSans"); return f; }
        logSDL("  romfs font open failed");
        return nullptr;
    }

    TTF_Font* openExtFont(int ptsize) {
        PlFontData fd = {};
        if (plGetSharedFontByType(&fd, PlSharedFontType_NintendoExt) == 0 && fd.size > 8) {
            SDL_RWops* rw = SDL_RWFromConstMem(
                (const uint8_t*)fd.address + 8, (int)fd.size - 8);
            TTF_Font* f = TTF_OpenFontRW(rw, 1, ptsize);
            if (f) { logMsg("  font: NintendoExt glyphs"); return f; }
        }
        logMsg("  NintendoExt font unavailable — text hints");
        return nullptr;
    }

    // ------------------------------------------------------------------
    bool init() {
        mkdir("sdmc:/AndroidHorizonNX", 0777);
        logOpen();
        logMsg("Android Horizon launcher starting");

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
            logSDL("SDL_Init failed"); logClose(); return false;
        }
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP) == 0)
            logSDL("IMG_Init warning");
        if (TTF_Init() != 0) {
            logSDL("TTF_Init failed"); logClose(); return false;
        }

        win = SDL_CreateWindow("Android Horizon",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SW, SH, SDL_WINDOW_SHOWN);
        if (!win) { logSDL("CreateWindow failed"); logClose(); return false; }

        rdr = SDL_CreateRenderer(win, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!rdr) {
            logSDL("Accelerated renderer failed, trying software");
            rdr = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        }
        if (!rdr) { logSDL("CreateRenderer failed"); logClose(); return false; }

        SDL_SetRenderDrawBlendMode(rdr, SDL_BLENDMODE_BLEND);

        fLg = openFont(28);
        fMd = openFont(20);
        fSm = openFont(17);
        if (!fLg || !fSm) { logMsg("Font load failed"); logClose(); return false; }
        if (!fMd) fMd = fSm;
        fBtn = openExtFont(22);

        buildBackground();

        if (SDL_NumJoysticks() > 0) {
            joy = SDL_JoystickOpen(0);
            if (!joy) logSDL("JoystickOpen warning");
        }
        avatarStart();
        logMsg("init complete");
        return true;
    }

    void cleanup() {
        avatarStop();
        if (avatarTex) SDL_DestroyTexture(avatarTex);
        if (bgTex) SDL_DestroyTexture(bgTex);
        for (auto* t : icons) if (t) SDL_DestroyTexture(t);
        if (fBtn) TTF_CloseFont(fBtn);
        if (fLg)  TTF_CloseFont(fLg);
        if (fMd && fMd != fSm) TTF_CloseFont(fMd);
        if (fSm)  TTF_CloseFont(fSm);
        if (joy)  SDL_JoystickClose(joy);
        if (rdr)  SDL_DestroyRenderer(rdr);
        if (win)  SDL_DestroyWindow(win);
        romfsExit(); plExit();
        TTF_Quit(); IMG_Quit(); SDL_Quit();
        logMsg("cleanup done");
        logClose();
    }

    // ------------------------------------------------------------------
    void fill(int x, int y, int w, int h, SDL_Color c) {
        SDL_SetRenderDrawColor(rdr, c.r, c.g, c.b, c.a);
        SDL_Rect r = {x, y, w, h};
        SDL_RenderFillRect(rdr, &r);
    }

    int drawText(TTF_Font* f, const std::string& s, SDL_Color col, int x, int y) {
        if (s.empty() || !f) return 0;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s.c_str(), col);
        if (!surf) return 0;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(rdr, surf);
        int w = surf->w;
        SDL_FreeSurface(surf);
        if (!tex) return 0;
        int tw, th;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {x, y, tw, th};
        SDL_RenderCopy(rdr, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
        return w;
    }

    static SDL_Color lerpCol(SDL_Color a, SDL_Color b, float t) {
        return { (Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t),
                 (Uint8)(a.b + (b.b - a.b) * t), 255 };
    }

    void fillCircle(int cx, int cy, int r, SDL_Color c) {
        SDL_SetRenderDrawColor(rdr, c.r, c.g, c.b, c.a);
        for (int dy = -r; dy <= r; dy++) {
            int hw = (int)sqrtf((float)(r * r - dy * dy));
            SDL_Rect row = {cx - hw, cy + dy, hw * 2, 1};
            SDL_RenderFillRect(rdr, &row);
        }
    }

    static constexpr float PLANET_R    = 2200.0f;
    static constexpr int   PLANET_BUMP = 130;

    void buildBackground() {
        stars.clear();
        uint32_t rng = 0x5EED5EED;
        auto rnd = [&rng]() { rng = rng * 1664525u + 1013904223u; return rng >> 8; };
        for (int i = 0; i < 110; i++) {
            Star s;
            s.x     = (float)(rnd() % SW);
            s.y     = (float)(rnd() % (SH - PLANET_BUMP - 80));
            s.sz    = (rnd() % 100 < 16) ? 2 : 1;
            s.phase = (rnd() % 628) / 100.0f;
            s.speed = 0.35f + (rnd() % 100) / 90.0f;
            stars.push_back(s);
        }

        // Same romfs:/background.svg the Translation Core uses — rasterized
        // via SDL2_image's bundled nanosvg support (see Changelog 0.1.105).
        SDL_Surface* svgSurf = IMG_Load("romfs:/background.svg");
        if (!svgSurf) { logSDL("background.svg load failed — flat background"); return; }
        bgTex = SDL_CreateTextureFromSurface(rdr, svgSurf);
        SDL_FreeSurface(svgSurf);
        if (!bgTex) logSDL("bg texture failed — flat background");
    }

    void drawBackground() {
        Uint32 now = SDL_GetTicks();
        if (bgTex) SDL_RenderCopy(rdr, bgTex, nullptr, nullptr);
        else       fill(0, 0, SW, SH, C_BG);
        for (auto& s : stars) {
            s.x -= 0.02f * s.speed;
            if (s.x < 0) s.x += SW;
            float tw = 0.5f + 0.5f * sinf(now / 1000.0f * s.speed * 6.2832f + s.phase);
            Uint8 a  = (Uint8)(70 + 170 * tw);
            fill((int)s.x, (int)s.y, s.sz, s.sz, {230, 235, 255, a});
            if (s.sz > 1 && tw > 0.75f) {
                fill((int)s.x - 2, (int)s.y, 6, 1, {230, 235, 255, (Uint8)(a / 3)});
                fill((int)s.x, (int)s.y - 2, 1, 6, {230, 235, 255, (Uint8)(a / 3)});
            }
        }
    }

    void drawHeaderBar(const std::string& rightText = "") {
        fill(0, 0, SW, HEADER_H, {24, 22, 86, 205});
        fill(0, HEADER_H - 3, SW, 3, C_RIM);
        int w = drawText(fLg, "Android ", C_WHITE, 30, (HEADER_H - 28) / 2);
        w += drawText(fLg, "Horizon", C_OK, 30 + w, (HEADER_H - 28) / 2);
        drawText(fSm, BUILD_VERSION, C_DIM, 30 + w + 14, (HEADER_H + 4) / 2);
        if (!rightText.empty()) {
            int tw = 0, th = 0;
            TTF_SizeUTF8(fSm, rightText.c_str(), &tw, &th);
            drawText(fSm, rightText, C_DIM, SW - tw - 30, (HEADER_H - 18) / 2);
        }
    }

    void drawFooterBar(const std::vector<std::pair<std::string, std::string>>& hints,
                       const std::string& leftText = "") {
        fill(0, SH - FOOTER_H, SW, FOOTER_H, {9, 8, 34, 225});
        fill(0, SH - FOOTER_H, SW, 2, C_RIM);
        int cy = SH - FOOTER_H / 2;
        if (!leftText.empty())
            drawText(fSm, leftText, C_WARN, 30, cy - 9);
        int x = SW - 30;
        for (auto it = hints.rbegin(); it != hints.rend(); ++it) {
            int lw = 0, lh = 0;
            TTF_SizeUTF8(fSm, it->second.c_str(), &lw, &lh);
            x -= lw;
            drawText(fSm, it->second, C_GRAY, x, cy - lh / 2);
            x -= 8;
            if (fBtn && it->first.size() > 1) {
                int gw = 0, gh = 0;
                TTF_SizeUTF8(fBtn, it->first.c_str(), &gw, &gh);
                x -= gw;
                drawText(fBtn, it->first, C_WHITE, x, cy - gh / 2);
            } else {
                x -= 26;
                fillCircle(x + 13, cy, 13, {41, 37, 128, 255});
                std::string letter = it->first.size() > 1 ? "?" : it->first;
                int gw = 0, gh = 0;
                TTF_SizeUTF8(fSm, letter.c_str(), &gw, &gh);
                drawText(fSm, letter, C_WHITE, x + 13 - gw / 2, cy - gh / 2);
            }
            x -= 34;
        }
    }

    static constexpr const char* GLYPH_A     = "\xEE\x83\xA0";
    static constexpr const char* GLYPH_B     = "\xEE\x83\xA1";
    static constexpr const char* GLYPH_X     = "\xEE\x83\xA2";
    static constexpr const char* GLYPH_Y     = "\xEE\x83\xA3";
    static constexpr const char* GLYPH_PLUS  = "\xEE\x83\xAF";
    static constexpr const char* GLYPH_MINUS = "\xEE\x83\xB0";

    std::string BG(const char* glyph, const char* letter) const {
        return fBtn ? glyph : letter;
    }

    static std::string formatSize(uint64_t bytes) {
        char buf[32];
        if (bytes >= 1024ull * 1024 * 1024)
            snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
        else if (bytes >= 1024ull * 1024)
            snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
        else if (bytes >= 1024ull)
            snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1024.0);
        else
            snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
        return buf;
    }

    void drawMonogram(const std::string& name, int x, int y, int sz) {
        static const SDL_Color PALETTE[] = {
            {239, 83,  80,  255}, {171, 71,  188, 255}, {66,  165, 245, 255},
            {38,  166, 154, 255}, {255, 167, 38,  255}, {126, 87,  194, 255},
            {92,  107, 192, 255}, {255, 112, 67,  255},
        };
        uint32_t h = 2166136261u;
        for (char c : name) h = (h ^ (uint8_t)c) * 16777619u;
        SDL_Color bg = PALETTE[h % (sizeof(PALETTE) / sizeof(PALETTE[0]))];
        fill(x, y, sz, sz, bg);
        char letter = name.empty() ? '?' : (char)toupper((unsigned char)name[0]);
        std::string s(1, letter);
        int w = 0, h2 = 0;
        TTF_SizeUTF8(fLg, s.c_str(), &w, &h2);
        drawText(fLg, s, C_WHITE, x + (sz - w) / 2, y + (sz - h2) / 2);
    }

    std::string clamp(TTF_Font* f, const std::string& s, int maxW) {
        int w = 0, h = 0;
        TTF_SizeUTF8(f, s.c_str(), &w, &h);
        if (w <= maxW) return s;
        std::string t = s;
        while (!t.empty()) {
            t.pop_back();
            std::string try_ = t + "...";
            TTF_SizeUTF8(f, try_.c_str(), &w, &h);
            if (w <= maxW) return try_;
        }
        return "...";
    }

    // ------------------------------------------------------------------
    void loadIcons() {
        icons.assign(apks.size(), nullptr);
        for (size_t i = 0; i < apks.size(); i++) {
            if (apks[i].iconPng.empty()) continue;
            SDL_RWops* rw = SDL_RWFromConstMem(
                apks[i].iconPng.data(), (int)apks[i].iconPng.size());
            SDL_Surface* surf = IMG_Load_RW(rw, 1);
            if (!surf) continue;
            icons[i] = SDL_CreateTextureFromSurface(rdr, surf);
            SDL_FreeSurface(surf);
            apks[i].iconPng.clear();
        }
    }

    void rescan() {
        for (auto* t : icons) if (t) SDL_DestroyTexture(t);
        icons.clear();
        apks = ::scanApks(APK_DIR);
        loadIcons();
        selected = 0; scroll = 0;
    }

    // ------------------------------------------------------------------
    void render() {
        Uint32 now = SDL_GetTicks();
        drawBackground();

        if (apks.empty()) {
            drawText(fSm,
                "No APKs found — place .apk files in sdmc:/AndroidHorizonNX/apks/",
                C_GRAY, 30, LIST_Y + 30);
        } else {
            int targetY = LIST_Y + (selected - scroll) * ITEM_H;
            if (selAnimY < 0) selAnimY = (float)targetY;
            selAnimY += (targetY - selAnimY) * 0.35f;
            if (fabsf(selAnimY - targetY) < 0.5f) selAnimY = (float)targetY;
            {
                int cy2 = (int)selAnimY;
                float pulse = 0.5f + 0.5f * sinf(now / 1000.0f * 2.6f);
                SDL_Rect card = {12, cy2 + 4, SW - 24, ITEM_H - 8};
                fill(card.x, card.y, card.w, card.h, {41, 37, 128, 235});
                for (int g = 1; g <= 5; g++) {
                    Uint8 a = (Uint8)((60 - g * 10) * (0.55f + 0.45f * pulse));
                    SDL_SetRenderDrawColor(rdr, 52, 230, 134, a);
                    SDL_Rect gr = {card.x - g, card.y - g,
                                   card.w + 2 * g, card.h + 2 * g};
                    SDL_RenderDrawRect(rdr, &gr);
                }
                SDL_SetRenderDrawColor(rdr, 52, 230, 134,
                                       (Uint8)(160 + 95 * pulse));
                SDL_RenderDrawRect(rdr, &card);
                fill(card.x, card.y, 5, card.h, C_RIM);
            }

            int end = std::min((int)apks.size(), scroll + VISIBLE);
            for (int i = scroll; i < end; i++) {
                int iy = LIST_Y + (i - scroll) * ITEM_H;
                SDL_SetRenderDrawColor(rdr, C_DIV.r, C_DIV.g, C_DIV.b, 130);
                SDL_RenderDrawLine(rdr, 24, iy + ITEM_H - 1, SW - 24, iy + ITEM_H - 1);

                int iconY = iy + (ITEM_H - ICON_SZ) / 2;
                if (i < (int)icons.size() && icons[i]) {
                    SDL_Rect dst = {28, iconY, ICON_SZ, ICON_SZ};
                    SDL_RenderCopy(rdr, icons[i], nullptr, &dst);
                } else {
                    drawMonogram(apks[i].appName, 28, iconY, ICON_SZ);
                }

                int tx   = 28 + ICON_SZ + 16;
                int maxW = SW - tx - 40;
                SDL_Color nameCol = (apks[i].arch == ApkArch::Arm32Only) ? C_DIM : C_WHITE;
                drawText(fLg, clamp(fLg, apks[i].appName, maxW), nameCol, tx, iy + 14);

                if (apks[i].arch == ApkArch::Arm32Only) {
                    static const std::string TAG = "32-BIT — UNSUPPORTED";
                    int bw = 0, bh = 0;
                    TTF_SizeUTF8(fSm, TAG.c_str(), &bw, &bh);
                    int bx = SW - bw - 40;
                    fill(bx - 6, iy + 14, bw + 12, bh, {72, 30, 14, 200});
                    drawText(fSm, TAG, C_WARN, bx, iy + 14);
                } else if (apks[i].installed) {
                    static const std::string INST = "INSTALLED";
                    int bw = 0, bh = 0;
                    TTF_SizeUTF8(fSm, INST.c_str(), &bw, &bh);
                    int bx = SW - bw - 40;
                    fill(bx - 6, iy + 14, bw + 12, bh, {13, 72, 40, 200});
                    drawText(fSm, INST, C_INST, bx, iy + 14);
                }

                std::string pkgLine =
                    (apks[i].packageName.empty() ? apks[i].filename : apks[i].packageName);
                if (!apks[i].versionName.empty())
                    pkgLine += "  v" + apks[i].versionName;
                if (apks[i].fileSizeBytes > 0)
                    pkgLine += "  ·  " + formatSize(apks[i].fileSizeBytes);
                drawText(fSm, clamp(fSm, pkgLine, maxW), C_GRAY, tx, iy + 58);
            }
            if ((int)apks.size() > VISIBLE) {
                int barH = LIST_H * VISIBLE / (int)apks.size();
                int barY = LIST_Y + LIST_H * scroll / (int)apks.size();
                fill(SW - 6, barY, 6, barH, {72, 66, 170, 200});
            }
        }

        std::string cnt;
        if (!apks.empty())
            cnt = std::to_string(apks.size()) + (apks.size() == 1 ? " APK" : " APKs");
        drawHeaderBar(cnt);

        if (noticeUntil && now < noticeUntil) {
            const char* msg = noticeText.c_str();
            int w = 0, h = 0;
            TTF_SizeUTF8(fSm, msg, &w, &h);
            fill((SW - w) / 2 - 16, SH - FOOTER_H - 44, w + 32, 34, {60, 18, 14, 235});
            drawText(fSm, msg, C_WARN, (SW - w) / 2, SH - FOOTER_H - 36);
        }

        bool docked = appletGetOperationMode() == AppletOperationMode_Console;
        drawFooterBar({{BG(GLYPH_A, "A"), "Launch"}, {BG(GLYPH_X, "X"), "Reinstall"},
                       {BG(GLYPH_Y, "Y"), "Rescan"}, {BG(GLYPH_MINUS, "-"), "About"},
                       {BG(GLYPH_PLUS, "+"), "Quit"}},
                      docked ? "Docked — games need handheld (touch screen)" : "");

        SDL_RenderPresent(rdr);
    }

    // ------------------------------------------------------------------
    void showAbout() {
        bool done = false;
        while (!done) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) { done = true; }
                if (ev.type == SDL_JOYBUTTONDOWN &&
                    (ev.jbutton.button == BTN_B || ev.jbutton.button == BTN_MINUS))
                    { done = true; }
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)
                    { done = true; }
            }

            std::vector<uint8_t> img;
            if (avatarPollNewImage(img)) {
                SDL_RWops* rw = SDL_RWFromConstMem(img.data(), (int)img.size());
                SDL_Surface* surf = IMG_Load_RW(rw, 1);
                if (surf) {
                    if (avatarTex) SDL_DestroyTexture(avatarTex);
                    avatarTex = SDL_CreateTextureFromSurface(rdr, surf);
                    SDL_FreeSurface(surf);
                }
            }

            drawBackground();
            drawHeaderBar();

            int avSz = 160;
            int avX  = (SW - avSz) / 2;
            int avY  = LIST_Y + 30;
            if (avatarTex) {
                SDL_Rect dst = {avX, avY, avSz, avSz};
                SDL_RenderCopy(rdr, avatarTex, nullptr, &dst);
            } else {
                drawMonogram("AndroidHorizon", avX, avY, avSz);
                static const std::string FETCH = "Loading avatar...";
                int fw = 0, fh = 0;
                TTF_SizeUTF8(fSm, FETCH.c_str(), &fw, &fh);
                drawText(fSm, FETCH, C_DIM, (SW - fw) / 2, avY + avSz + 8);
            }

            int y = avY + avSz + 40;
            auto center = [&](TTF_Font* f, const std::string& s, SDL_Color col) {
                int w = 0, h = 0;
                TTF_SizeUTF8(f, s.c_str(), &w, &h);
                drawText(f, s, col, (SW - w) / 2, y);
                y += h + 10;
            };
            center(fLg, "Android Horizon", C_WHITE);
            center(fSm, BUILD_VERSION, C_DIM);
            center(fSm, "by aaronworld.uk", C_GRAY);
            y += 10;
            center(fSm, "Android NDK compatibility layer for Nintendo Switch (HorizonOS)", C_GRAY);

            drawFooterBar({{BG(GLYPH_B, "B"), "Back to menu"}});

            SDL_RenderPresent(rdr);
            SDL_Delay(16);
        }
    }

    // ------------------------------------------------------------------
    // Chain-load into the right Translation Core for this game's architecture,
    // passing the package name as argv[0] (libnx's argv parser has no
    // synthetic program-name slot — the first word IS argv[0]) —
    // envSetNextLoad(path, argv) is the
    // same mechanism external forwarders (Sphaira etc.) already use to jump
    // straight into a game; this launcher just uses it internally now too.
    // Returning from main() after this call lets hbloader perform the switch.
    bool launchGame(const ApkInfo& apk, bool* outHandled) {
        *outHandled = false;
        if (apk.arch == ApkArch::Arm32Only) {
            noticeText  = "32-bit binaries aren't supported at the moment — "
                          "there isn't enough public documentation to support "
                          "this safely yet.";
            noticeUntil = SDL_GetTicks() + 7000;
            // CORE_X32_PATH isn't chain-loaded into (it's only a placeholder
            // that prints the same message), but log where a real x32 build
            // would eventually go, for whoever picks this up later.
            logMsg(("launch blocked (32-bit unsupported): " + apk.packageName +
                    " — would-be core: " + CORE_X32_PATH).c_str());
            return false;
        }
        const char* corePath = CORE_X64_PATH;
        struct stat st;
        if (stat(corePath, &st) != 0) {
            noticeText  = "AHNX-Translation-Core-x64.nro not found next to the launcher.";
            noticeUntil = SDL_GetTicks() + 7000;
            logMsg(("core NRO missing: " + std::string(corePath)).c_str());
            return false;
        }
        // Chain-loading is an hbloader feature — if this process wasn't
        // started under hbloader (e.g. a forwarder that launches it some
        // other way), envSetNextLoad may silently have nowhere to hand off
        // to. Log this explicitly so a failed launch attempt is diagnosable
        // from launcher_log.txt instead of a guess.
        bool hasNext = envHasNextLoad();
        logMsg(hasNext ? "envHasNextLoad: true" : "envHasNextLoad: FALSE — chain-load unsupported in this launch context");
        const std::string& pkg = apk.packageName.empty() ? apk.filename : apk.packageName;
        // argv[0] MUST be the Core's own real path, not our package name —
        // libnx's romfsInit() falls back to argv[0] to find and open its own
        // .nro file on the SD card and read its embedded RomFS section from
        // it (confirmed against libnx's actual romfs-mounting behavior).
        // Overwriting argv[0] with the package name broke RomFS entirely —
        // every font load failed immediately on the Core side, confirmed via
        // its own log.txt. The real argument goes at argv[1] instead, same
        // convention any normal argv[0]-is-the-program-path command line
        // would use.
        std::string argvStr = std::string(corePath) + " " + pkg;
        logMsg(("launchGame: chain-loading to " + std::string(corePath) +
                " argv=\"" + argvStr + "\"").c_str());
        Result rc = envSetNextLoad(corePath, argvStr.c_str());
        if (R_FAILED(rc)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "envSetNextLoad failed: 0x%08x", rc);
            noticeText  = buf;
            noticeUntil = SDL_GetTicks() + 7000;
            logMsg(buf);
            return false;
        }
        logMsg("envSetNextLoad OK — returning from main() to hand off");
        *outHandled = true;
        return true;
    }
};

// ---------------------------------------------------------------------------
int main(int, char**) {
    App app;

    if (!app.init()) return 1;

    mkdir(APK_DIR, 0777);

    app.drawBackground();
    app.drawHeaderBar();
    app.drawText(app.fSm, "Scanning for APKs...", C_GRAY, 30, LIST_Y + 30);
    SDL_RenderPresent(app.rdr);

    app.apks = scanApks(APK_DIR);
    app.loadIcons();
    app.render();

    bool   quit      = false;
    bool   handoff   = false;
    Uint32 lastStick = 0;

    while (!quit && !handoff) {
        SDL_Event ev;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { quit = true; break; }

            if (ev.type == SDL_JOYBUTTONDOWN) {
                switch (ev.jbutton.button) {
                    case BTN_PLUS:
                        quit = true;
                        break;

                    case BTN_A:
                        if (!app.apks.empty())
                            app.launchGame(app.apks[app.selected], &handoff);
                        break;

                    case BTN_X:
                        // Reinstall = same chain-load, the Core re-extracts
                        // when it sees no cached install for this package.
                        if (!app.apks.empty())
                            app.launchGame(app.apks[app.selected], &handoff);
                        break;

                    case BTN_Y:
                        app.rescan();
                        break;

                    case BTN_MINUS:
                        app.showAbout();
                        break;

                    case BTN_B:
                        break;
                }
            }

            if (ev.type == SDL_JOYHATMOTION) {
                if (ev.jhat.value & SDL_HAT_DOWN) {
                    if (!app.apks.empty() && app.selected < (int)app.apks.size() - 1) {
                        app.selected++;
                        if (app.selected >= app.scroll + VISIBLE) app.scroll++;
                    }
                }
                if (ev.jhat.value & SDL_HAT_UP) {
                    if (!app.apks.empty() && app.selected > 0) {
                        app.selected--;
                        if (app.selected < app.scroll) app.scroll--;
                    }
                }
            }

            if (ev.type == SDL_JOYAXISMOTION && ev.jaxis.axis == 1) {
                Uint32 now = SDL_GetTicks();
                if (now - lastStick > 180) {
                    if (ev.jaxis.value > 16384 && !app.apks.empty() &&
                        app.selected < (int)app.apks.size() - 1) {
                        app.selected++;
                        if (app.selected >= app.scroll + VISIBLE) app.scroll++;
                        lastStick = now;
                    } else if (ev.jaxis.value < -16384 && !app.apks.empty() &&
                               app.selected > 0) {
                        app.selected--;
                        if (app.selected < app.scroll) app.scroll--;
                        lastStick = now;
                    }
                }
            }
        }

        if (!quit && !handoff) app.render();
        SDL_Delay(16);
    }

    app.cleanup();
    return 0;
}
