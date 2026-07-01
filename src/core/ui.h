#ifndef UI_H
#define UI_H

#include <SDL3/SDL.h>

// Screen-space UI: the in-game HUD and the full-screen menu overlays.
//
// All text is drawn with SDL3's built-in 8x8 debug font (SDL_RenderDebugText),
// scaled up as needed -- no SDL_ttf / font assets required. Everything renders
// in the logical (zoom-adjusted) coordinate space, so it lays out correctly at
// any window size or camera zoom.

typedef struct Game Game;

// One row in a menu. `value` is optional: when set, the row renders as
// "label: value" (used for settings toggles like "Sound: ON").
typedef struct {
    const char *label;
    const char *value;
} UIMenuItem;

// In-game HUD: player HP bar (top-left), level indicator, boss HP bar.
void UI_RenderHUD(Game *game);

// Full-screen menu overlay: dims the world, draws a title and a vertical list
// of items with the selected one highlighted, plus a controls hint footer.
void UI_RenderMenu(Game *game, const char *title,
                   const UIMenuItem *items, int count, int selected);

// Which menu row (0..count-1) contains the logical-space point (x, y), or -1 if
// none. Rows are matched by their vertical band across the full width, so mouse
// hover/click lines up with the layout UI_RenderMenu draws. Pass the same item
// count used to render the current menu.
int UI_MenuItemAt(Game *game, float x, float y, int count);

#endif // UI_H
