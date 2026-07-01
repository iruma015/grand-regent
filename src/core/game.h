#ifndef GAME_H
#define GAME_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "camera.h"
#include "texture.h"
#include "../entities/player.h"
#include "../entities/enemy.h"
#include "../entities/boss.h"
#include "../combat/hitbox.h"
#include "../world/map.h"

#define MAX_ENEMIES 10

typedef enum {
    GAME_MENU,        // main menu (startup / quit-to-menu)
    GAME_PLAYING,
    GAME_PAUSED,      // pause overlay
    GAME_SETTINGS,    // settings submenu (mute sound / camera zoom)
    GAME_PLAYER_DEAD,
    GAME_BOSS_DEAD,
    GAME_WIN          // cleared the final level
} GameState;

typedef struct {
    int width, height, logW, logH;
    // The authored (un-zoomed) logical resolution. Camera zoom shrinks the
    // effective logW/logH below; these remember the 1.0x baseline to divide by.
    int baseLogW, baseLogH;
} Win;

typedef struct Game {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Win win;

    bool running;
    GameState state;
    bool vsyncEnabled;  // true if the renderer paces frames itself (no manual delay)

    // --- Menu / settings ---
    int menuSelection;      // highlighted item in the current menu
    GameState settingsReturn; // state to return to when leaving the settings screen
    float zoom;             // camera zoom factor (1.0 = authored view; higher = closer)

    float deltaTime;
    Uint64 prevTime;
    float hitStopTimer; // brief, world-wide freeze on impact for combat "juice"

    Map map;
    Camera camera;

    int currentLevel;   // index into the LEVELS table (0-based)

    Player player;
    Enemy enemies[MAX_ENEMIES];
    int enemyCount;
    Boss boss;
    bool bossPresent;

    HitboxList hitboxes;

    TextureManager textures; // kept so we can load the next level's map mid-game
} Game;

bool Game_Init(Game *game);
void Game_HandleEvents(Game *game);
void Game_Update(Game *game);
void Game_Render(Game *game);
void Game_Destroy(Game *game);

#endif // GAME_H
