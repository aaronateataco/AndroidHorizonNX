// AHNX-Translation-Core-x32 — placeholder.
//
// 32-bit (AArch32) Android game binaries aren't supported yet. Running them
// natively on Switch is possible in principle (there's real prior art for
// AArch32 execution via a per-title Atmosphere address-space override), but
// it needs its own 32-bit build of libnx and a whole separate ELF loader/ABI
// layer — and the one real precedent project we found for this depends on a
// 32-bit libnx build that isn't publicly available anywhere. Until that
// foundational piece exists, this binary only exists to complete the
// launcher's folder layout and explain why it can't do anything yet.
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) return 1;
    if (TTF_Init() != 0) { SDL_Quit(); return 1; }

    SDL_Window* win = SDL_CreateWindow("AHNX Translation Core (x32)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_SHOWN);
    SDL_Renderer* rdr = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED) : nullptr;
    if (!rdr && win) rdr = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    TTF_Font* font = nullptr;
    PlFontData fd = {};
    if (plInitialize(PlServiceType_User) == 0 &&
        plGetSharedFontByType(&fd, PlSharedFontType_Standard) == 0 && fd.size > 8) {
        SDL_RWops* rw = SDL_RWFromConstMem((const uint8_t*)fd.address + 8, (int)fd.size - 8);
        font = TTF_OpenFontRW(rw, 1, 26);
    }

    static const char* LINES[] = {
        "AHNX Translation Core (x32) — placeholder",
        "",
        "32-bit binaries aren't supported at the moment.",
        "There isn't enough public documentation available to support",
        "AArch32 execution on Switch homebrew safely yet.",
        "",
        "This component exists only to complete the launcher's folder layout.",
        nullptr
    };

    bool done = false;
    while (rdr && !done) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) done = true;
            if (ev.type == SDL_JOYBUTTONDOWN && ev.jbutton.button == 10 /*PLUS*/) done = true;
        }
        SDL_SetRenderDrawColor(rdr, 13, 12, 46, 255);
        SDL_RenderClear(rdr);
        int y = 260;
        for (int i = 0; LINES[i]; i++) {
            if (font && LINES[i][0]) {
                SDL_Color col = (i == 0) ? SDL_Color{230, 190, 70, 255} : SDL_Color{200, 200, 220, 255};
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font, LINES[i], col);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(rdr, surf);
                    SDL_Rect dst = {60, y, surf->w, surf->h};
                    SDL_FreeSurface(surf);
                    if (tex) { SDL_RenderCopy(rdr, tex, nullptr, &dst); SDL_DestroyTexture(tex); }
                }
            }
            y += 34;
        }
        SDL_RenderPresent(rdr);
        SDL_Delay(16);
    }

    if (font) TTF_CloseFont(font);
    if (rdr) SDL_DestroyRenderer(rdr);
    if (win) SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
