#include "ui.h"
#include "game.h"
#include <SDL3/SDL.h>

// SDL_RenderDebugText draws a fixed 8px-tall monospace font; scale it up for
// legible UI. Everything below is expressed in logical (zoom-adjusted) pixels.
#define UI_CHAR 8.0f

// --- Small drawing helpers ---------------------------------------------------

static void UI_SetColor(SDL_Renderer *r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

// Draw a string at (x, y) enlarged by `scale`. Uses SDL's render scale so the
// 8px font can be blown up to any size, then restores the 1:1 scale.
static void UI_Text(SDL_Renderer *r, float x, float y, float scale, SDL_Color c, const char *s) {
    UI_SetColor(r, c);
    SDL_SetRenderScale(r, scale, scale);
    SDL_RenderDebugText(r, x / scale, y / scale, s);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

static float UI_TextWidth(const char *s, float scale) {
    return (float)SDL_strlen(s) * UI_CHAR * scale;
}

static void UI_TextCentered(SDL_Renderer *r, float cx, float y, float scale, SDL_Color c, const char *s) {
    UI_Text(r, cx - UI_TextWidth(s, scale) * 0.5f, y, scale, c, s);
}

static void UI_FillRect(SDL_Renderer *r, float x, float y, float w, float h, SDL_Color c) {
    UI_SetColor(r, c);
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void UI_OutlineRect(SDL_Renderer *r, float x, float y, float w, float h, SDL_Color c) {
    UI_SetColor(r, c);
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderRect(r, &rect);
}

// A progress bar: background track, a proportional fill, and a border.
static void UI_Bar(SDL_Renderer *r, float x, float y, float w, float h, float frac,
                   SDL_Color fill, SDL_Color bg, SDL_Color border) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    UI_FillRect(r, x, y, w, h, bg);
    UI_FillRect(r, x, y, w * frac, h, fill);
    UI_OutlineRect(r, x, y, w, h, border);
}

// --- Palette -----------------------------------------------------------------

static const SDL_Color COL_TEXT     = { 235, 235, 245, 255 };
static const SDL_Color COL_DIM       = { 170, 170, 185, 255 };
static const SDL_Color COL_ACCENT     = { 245, 210,  90, 255 };
static const SDL_Color COL_BARBG       = {  25,  18,  28, 220 };
static const SDL_Color COL_HP_HIGH      = {  70, 200,  90, 255 };
static const SDL_Color COL_HP_LOW        = { 210,  60,  50, 255 };
static const SDL_Color COL_BOSS           = { 200,  50,  60, 255 };

// --- HUD ---------------------------------------------------------------------

void UI_RenderHUD(Game *game) {
    SDL_Renderer *r = game->renderer;
    float logW = (float)game->win.logW;

    const float pad = 12.0f;

    // Player HP bar, top-left.
    const float barW = 220.0f, barH = 18.0f;
    float frac = game->player.maxHealth > 0
                     ? (float)game->player.health / (float)game->player.maxHealth
                     : 0.0f;
    SDL_Color hpFill = frac > 0.3f ? COL_HP_HIGH : COL_HP_LOW;

    UI_Text(r, pad, pad, 1.5f, COL_TEXT, "HP");
    float barX = pad, barY = pad + 18.0f;
    UI_Bar(r, barX, barY, barW, barH, frac, hpFill, COL_BARBG, COL_TEXT);

    char hpText[32];
    SDL_snprintf(hpText, sizeof(hpText), "%d / %d",
                 game->player.health < 0 ? 0 : game->player.health, game->player.maxHealth);
    UI_Text(r, barX + 6.0f, barY + 5.0f, 1.0f, COL_TEXT, hpText);

    // Level indicator, just under the HP bar.
    char lvl[32];
    SDL_snprintf(lvl, sizeof(lvl), "LEVEL %d", game->currentLevel + 1);
    UI_Text(r, barX, barY + barH + 6.0f, 1.0f, COL_DIM, lvl);

    // Boss HP bar, top-center, only while a living boss is on the field.
    if (game->bossPresent && Boss_IsAlive(&game->boss)) {
        float bw = logW * 0.5f, bh = 14.0f;
        float bx = (logW - bw) * 0.5f, by = pad + 10.0f;
        float bfrac = game->boss.maxHealth > 0
                          ? (float)game->boss.health / (float)game->boss.maxHealth
                          : 0.0f;
        UI_TextCentered(r, logW * 0.5f, pad - 2.0f, 1.0f, COL_TEXT, "BOSS");
        UI_Bar(r, bx, by, bw, bh, bfrac, COL_BOSS, COL_BARBG, COL_TEXT);
    }
}

// --- Menus -------------------------------------------------------------------

// Menu row layout, shared by UI_RenderMenu (draw) and UI_MenuItemAt (hit-test)
// so mouse hover/click always lines up with what's on screen. The selection
// highlight starts UI_MENU_ROW_PAD above each row's baseline and is
// UI_MENU_ROW_H tall; rows are spaced UI_MENU_LINE_H apart from startY.
#define UI_MENU_ITEM_SCALE 1.75f
#define UI_MENU_LINE_H     34.0f
#define UI_MENU_ROW_PAD    6.0f
#define UI_MENU_ROW_H      (UI_CHAR * UI_MENU_ITEM_SCALE + 12.0f)

static float UI_MenuStartY(const Game *game) {
    return (float)game->win.logH * 0.42f;
}

void UI_RenderMenu(Game *game, const char *title,
                   const UIMenuItem *items, int count, int selected) {
    SDL_Renderer *r = game->renderer;
    float logW = (float)game->win.logW, logH = (float)game->win.logH;

    // Dim the world behind the menu (requires blend mode, set once in Game_Init).
    UI_FillRect(r, 0.0f, 0.0f, logW, logH, (SDL_Color){ 10, 8, 16, 205 });

    UI_TextCentered(r, logW * 0.5f, logH * 0.20f, 3.0f, COL_ACCENT, title);

    const float itemScale = UI_MENU_ITEM_SCALE;
    const float lineH = UI_MENU_LINE_H;
    float startY = UI_MenuStartY(game);

    for (int i = 0; i < count; i++) {
        float y = startY + (float)i * lineH;
        bool sel = (i == selected);

        char line[96];
        if (items[i].value)
            SDL_snprintf(line, sizeof(line), "%s: %s", items[i].label, items[i].value);
        else
            SDL_snprintf(line, sizeof(line), "%s", items[i].label);

        float w = UI_TextWidth(line, itemScale);
        float x = logW * 0.5f - w * 0.5f;

        if (sel) {
            UI_FillRect(r, x - 26.0f, y - UI_MENU_ROW_PAD, w + 52.0f, UI_MENU_ROW_H,
                        (SDL_Color){ 245, 210, 90, 45 });
            UI_Text(r, x - 22.0f, y, itemScale, COL_ACCENT, ">");
        }
        UI_Text(r, x, y, itemScale, sel ? COL_TEXT : COL_DIM, line);
    }

    UI_TextCentered(r, logW * 0.5f, logH * 0.90f, 1.0f, COL_DIM,
                    "MOVE: ARROWS/WS/PAD   SELECT: ENTER/A/CLICK   BACK: ESC/B");
}

// Hit-test a logical-space point against the menu rows. Rows span the full
// width, so only the vertical band (matching the drawn selection highlight)
// decides the match -- clicking or hovering anywhere on a row selects it.
int UI_MenuItemAt(Game *game, float x, float y, int count) {
    (void)x;
    float startY = UI_MenuStartY(game);
    for (int i = 0; i < count; i++) {
        float top = startY + (float)i * UI_MENU_LINE_H - UI_MENU_ROW_PAD;
        if (y >= top && y < top + UI_MENU_ROW_H) return i;
    }
    return -1;
}
