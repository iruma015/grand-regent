#include "game.h"
#include "audio.h"
#include "input.h"
#include "ui.h"
#include "../world/tmj_loader.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <stdlib.h>

// ===========================================================================
//  LEVELS
//
//  The game plays these in order. Walk off the right edge of a level to
//  advance to the next one; clearing the last level wins the game.
//
//  Spawn points are defined right here so they're easy to adjust -- just edit
//  the numbers below. (x, y) are world pixels; y is the sprite's top, so a
//  spawn sits just above the floor it should land on.
//
//  If you'd rather place spawns visually in Tiled, add objects named
//  "player_spawn" / "enemy_spawn" / "boss_spawn" to the map's "spawns" object
//  layer -- those override the coordinates here (see LoadSpawnsForLevel).
// ===========================================================================

#define LEVEL_MAX_ENEMIES 6

typedef struct {
    float x, y;
} SpawnPoint;

typedef struct {
    const char *mapPath;
    SpawnPoint player;
    SpawnPoint enemies[LEVEL_MAX_ENEMIES];
    int enemyCount;
    bool hasBoss;
    SpawnPoint boss;
    // The player clears the level once their right edge passes this world X.
    // Set it to wherever the reachable "end" of the level is. 0 means "use the
    // right edge of the map" (good for flat levels you can walk straight across).
    float exitX;
} LevelConfig;

// Floors: level 1 ground top y=480 (32px tiles); levels 2 & 3 use 64px tiles
// with floors at y=640 (left of map 2) and y=960 (map 3).
static const LevelConfig LEVELS[] = {
    // --- Level 1: intro fight on a flat field --------------------------------
    // Flat, full-width floor at y=480. Walk straight across to the right edge.
    {
        .mapPath = "assets/maps/1-nArea.tmj",
        .player  = { 60.0f, 420.0f },
        .enemies = { {400.0f, 430.0f}, {560.0f, 430.0f}, {720.0f, 430.0f} },
        .enemyCount = 3,
        .hasBoss = false,
        .exitX = 0.0f, // use the map's right edge
    },
    // --- Level 2: drop to the lower floor and fight across -------------------
    // The far-right of this map is a tall wall, so the reachable "end" is the
    // bottom floor just before it (~x=1408). Player starts on the upper-left
    // ledge, drops into the gap, and crosses the bottom.
    {
        .mapPath = "assets/maps/2-nArea.tmj",
        .player  = { 60.0f, 560.0f },
        // Two on the upper-left ledge, two in the lower pocket you fall into.
        .enemies = { {250.0f, 560.0f}, {420.0f, 560.0f}, {520.0f, 1080.0f}, {650.0f, 1080.0f} },
        .enemyCount = 4,
        .hasBoss = false,
        .exitX = 1380.0f, // reachable top of the right staircase, before the wall
    },
    // --- Level 3: the boss arena ---------------------------------------------
    // Open floor at y=960 out to ~x=1280 (a 2-tile wall blocks further right).
    // The boss stands in that open arena. Win by beating it.
    {
        .mapPath = "assets/maps/3-nArea.tmj",
        .player  = { 60.0f, 900.0f },
        .enemies = { {700.0f, 900.0f}, {950.0f, 900.0f} },
        .enemyCount = 2,
        .hasBoss = true,
        .boss    = { 1050.0f, 860.0f },
    },
};
#define LEVEL_COUNT ((int)(sizeof(LEVELS) / sizeof(LEVELS[0])))

// How close to the right edge the player must get to clear the level.
#define LEVEL_EXIT_MARGIN 8.0f

// --- Combat & camera "juice" ------------------------------------------------
// Hit-stop is a tiny world-wide freeze the instant an attack connects; it makes
// impacts read as heavy. Screen shake reinforces them. Look-ahead nudges the
// camera in the direction the player faces so more of the level is visible.
#define HITSTOP_ON_HIT    0.04f   // player landed a hit on an enemy/boss
#define HITSTOP_ON_HURT   0.07f   // player got hit -- freeze reads as a bigger blow
#define SHAKE_ON_HIT_MAG      2.5f
#define SHAKE_ON_HIT_TIME     0.10f
#define SHAKE_ON_HURT_MAG     5.0f
#define SHAKE_ON_HURT_TIME    0.28f
#define CAMERA_LOOKAHEAD  30.0f

static Uint64 lastFpsTime = 0;
static int frameCount = 0;
static int fps = 0;

// Populates entity spawns for a level: prefers spawn objects placed in Tiled,
// otherwise falls back to the coordinates in the LevelConfig above.
static void SpawnEntitiesForLevel(Game *game, const LevelConfig *cfg) {
    TextureManager *manager = &game->textures;
    SDL_Texture *playerTex = TextureManager_Load(manager, "assets/player/player.png");
    SDL_Texture *enemyTex = TextureManager_Load(manager, "assets/enemy/mainEnemy.png");
    SDL_Texture *bossTex = TextureManager_Load(manager, "assets/boss/boss.png"); // ok if missing; falls back to a colored rect

    // ---- Player ----
    // Spawn like the SDL2 build: one tile in from the left edge, resting on top
    // of the first solid tile in that column (scanning the "first-ground" layer
    // top-to-bottom). A "player" spawn object placed in Tiled still overrides it;
    // cfg->player.y is only the fallback if that column somehow has no floor.
    const MapSpawn *playerSpawn = Map_FindSpawn(&game->map, SPAWN_PLAYER, 0);
    float px = playerSpawn ? playerSpawn->x : (float)game->map.tileWidth;
    float py = playerSpawn ? playerSpawn->y : cfg->player.y;
    Player_Init(&game->player, px, py, playerTex);
    if (!playerSpawn) {
        float playerH = Player_GetBounds(&game->player).height;
        float colCenterX = 1.5f * (float)game->map.tileWidth; // center of tile column 1
        for (int ty = 0; ty < game->map.height; ty++) {
            float rowCenterY = ((float)ty + 0.5f) * (float)game->map.tileHeight;
            if (Map_IsSolidTile(&game->map, colCenterX, rowCenterY)) {
                game->player.y = (float)ty * (float)game->map.tileHeight - playerH;
                break;
            }
        }
    }

    // ---- Enemies ----
    game->enemyCount = 0;
    int mapEnemies = Map_CountSpawns(&game->map, SPAWN_ENEMY);
    if (mapEnemies > 0) {
        for (int i = 0; i < mapEnemies && game->enemyCount < MAX_ENEMIES; i++) {
            const MapSpawn *s = Map_FindSpawn(&game->map, SPAWN_ENEMY, i);
            Enemy_Init(&game->enemies[game->enemyCount++], s->x, s->y, enemyTex);
        }
    } else {
        for (int i = 0; i < cfg->enemyCount && game->enemyCount < MAX_ENEMIES; i++) {
            Enemy_Init(&game->enemies[game->enemyCount++], cfg->enemies[i].x, cfg->enemies[i].y, enemyTex);
        }
    }

    // ---- Boss ----
    const MapSpawn *bossSpawn = Map_FindSpawn(&game->map, SPAWN_BOSS, 0);
    if (bossSpawn) {
        Boss_Init(&game->boss, bossSpawn->x, bossSpawn->y, bossTex, Map_PixelWidth(&game->map));
        game->bossPresent = true;
    } else if (cfg->hasBoss) {
        Boss_Init(&game->boss, cfg->boss.x, cfg->boss.y, bossTex, Map_PixelWidth(&game->map));
        game->bossPresent = true;
    } else {
        game->bossPresent = false;
    }
}

// Loads a level by index: swaps in its map, rebuilds the camera bounds, and
// spawns its entities. Safe to call mid-game to transition between levels.
static bool Game_LoadLevel(Game *game, int levelIndex) {
    if (levelIndex < 0 || levelIndex >= LEVEL_COUNT) return false;
    const LevelConfig *cfg = &LEVELS[levelIndex];

    Map_Free(&game->map); // no-op on a zeroed map, so this is fine on first load
    if (!Map_LoadFromTMJ(&game->map, cfg->mapPath, &game->textures)) {
        SDL_Log("Failed to load '%s', using built-in fallback map", cfg->mapPath);
        Map_InitFallback(&game->map);
    }

    game->currentLevel = levelIndex;

    Camera_SetWorldBounds(&game->camera, Map_PixelWidth(&game->map), Map_PixelHeight(&game->map));

    SpawnEntitiesForLevel(game, cfg);
    Camera_SnapTo(&game->camera, game->player.x, game->player.y);

    HitboxList_Clear(&game->hitboxes);

    SDL_Log("Loaded level %d/%d ('%s'), world: %.0fx%.0f",
            levelIndex + 1, LEVEL_COUNT, cfg->mapPath,
            Map_PixelWidth(&game->map), Map_PixelHeight(&game->map));
    return true;
}

bool Game_Init(Game *game) {
    bool initSuccess = true;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        initSuccess = false;
    }

    game->window = SDL_CreateWindow("Grand Regent", game->win.width, game->win.height, SDL_WINDOW_RESIZABLE);
    if (!game->window) {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        SDL_Quit();
        initSuccess = false;
    }

    game->renderer = SDL_CreateRenderer(game->window, NULL);
    if (!game->renderer) {
        SDL_Log("Unable to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(game->window);
        initSuccess = false;
    }

    SDL_SetRenderLogicalPresentation(game->renderer, game->win.logW, game->win.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // Remember the authored logical size as the 1.0x zoom baseline, then enable
    // alpha blending so translucent HUD/menu overlays composite correctly.
    game->win.baseLogW = game->win.logW;
    game->win.baseLogH = game->win.logH;
    game->zoom = 1.0f;
    game->menuSelection = 0;
    game->settingsReturn = GAME_MENU;
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);

    // VSync gives evenly paced, tear-free frames -- the single biggest win for
    // perceived smoothness. If the driver refuses it, main.c falls back to a
    // fixed delay to cap the frame rate.
    game->vsyncEnabled = SDL_SetRenderVSync(game->renderer, 1);
    SDL_Log("VSync %s", game->vsyncEnabled ? "enabled" : "unavailable (using delay fallback)");

    TextureManager_Init(&game->textures, game->renderer);

    // Audio is optional: if the device can't be opened the game still runs
    // (silently), so a failure here doesn't fail Game_Init.
    Audio_Init();

    // Gamepad + mouse input. Also optional: with no controller attached the
    // keyboard/mouse still work and gamepad queries just report "not pressed".
    Input_Init();

    Camera_Init(&game->camera, (float)game->win.logW, (float)game->win.logH);

    // Boot into the first level (this loads the map, sets camera bounds,
    // spawns entities, and snaps the camera).
    if (!Game_LoadLevel(game, 0)) {
        SDL_Log("Failed to load the first level");
        initSuccess = false;
    }

    // Boot to the main menu (the freshly-loaded level 0 shows behind it).
    game->state = GAME_MENU;
    game->prevTime = SDL_GetTicks();
    lastFpsTime = SDL_GetTicks();

    Audio_PlayMusic(); // looping background track (no-op if music failed to load)

    SDL_Log("Game initialized. Window: %dx%d, logical: %dx%d",
            game->win.width, game->win.height, game->win.logW, game->win.logH);

    return initSuccess;
}

// ===========================================================================
//  MENU / SETTINGS
//
//  The game is a small state machine (see GameState). GAME_PLAYING runs the
//  simulation; every other state is a menu overlay drawn on top of the frozen
//  world. All menu navigation flows through Game_MenuKey below.
// ===========================================================================

// Camera zoom is expressed as a factor over the authored logical resolution:
// a smaller effective logical size means fewer world pixels fill the screen,
// i.e. everything looks bigger (more zoomed in).
static const float ZOOM_LEVELS[] = { 1.0f, 1.25f, 1.5f, 2.0f };
#define ZOOM_COUNT ((int)(sizeof(ZOOM_LEVELS) / sizeof(ZOOM_LEVELS[0])))

// Recompute the logical render size from the current zoom and push it to both
// the renderer and the camera viewport (the two must agree for follow/clamp to
// work). The whole engine keys off the logical size, so this is the only place
// zoom needs to touch.
static void Game_ApplyZoom(Game *game) {
    int logW = (int)((float)game->win.baseLogW / game->zoom + 0.5f);
    int logH = (int)((float)game->win.baseLogH / game->zoom + 0.5f);
    game->win.logW = logW;
    game->win.logH = logH;
    SDL_SetRenderLogicalPresentation(game->renderer, logW, logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    game->camera.viewW = (float)logW;
    game->camera.viewH = (float)logH;
    Camera_Update(&game->camera);
}

static void Game_StepZoom(Game *game, int dir) {
    int idx = 0;
    for (int i = 0; i < ZOOM_COUNT; i++) {
        if (ZOOM_LEVELS[i] == game->zoom) { idx = i; break; }
    }
    idx = (idx + dir % ZOOM_COUNT + ZOOM_COUNT) % ZOOM_COUNT;
    game->zoom = ZOOM_LEVELS[idx];
    Game_ApplyZoom(game);
}

// Reload level 0 with a fresh player and drop straight into play. Used by
// "Start Game", "Restart", and "Play Again".
static void Game_Restart(Game *game) {
    Game_LoadLevel(game, 0);
    game->hitStopTimer = 0.0f;
    game->state = GAME_PLAYING;
    game->prevTime = SDL_GetTicks(); // avoid a large dt after time spent in a menu
    Audio_PlayMusic();
}

static void Game_Pause(Game *game) {
    game->state = GAME_PAUSED;
    game->menuSelection = 0;
}

static void Game_Resume(Game *game) {
    game->state = GAME_PLAYING;
    game->prevTime = SDL_GetTicks();
}

static void Game_ToMenu(Game *game) {
    game->state = GAME_MENU;
    game->menuSelection = 0;
}

static void Game_OpenSettings(Game *game, GameState returnTo) {
    game->settingsReturn = returnTo;
    game->state = GAME_SETTINGS;
    game->menuSelection = 0;
}

static void Game_CloseSettings(Game *game) {
    game->state = game->settingsReturn;
    game->menuSelection = 0;
}

// Number of selectable rows in the menu for `state` -- kept in lockstep with
// the item lists in Game_RenderCurrentMenu and the actions in Game_MenuActivate.
static int Game_MenuItemCount(GameState state) {
    switch (state) {
        case GAME_MENU:        return 3; // Start / Settings / Quit
        case GAME_PAUSED:      return 4; // Resume / Restart / Settings / Quit to Menu
        case GAME_SETTINGS:    return 3; // Sound / Camera Zoom / Back
        case GAME_PLAYER_DEAD: return 2; // Restart / Quit to Menu
        case GAME_WIN:         return 2; // Play Again / Quit to Menu
        default:               return 0;
    }
}

// Change the value of the currently-highlighted settings row (Left/Right, or
// Enter which nudges it forward).
static void Game_SettingsAdjust(Game *game, int dir) {
    switch (game->menuSelection) {
        case 0: Audio_ToggleMute();      break; // Sound: either direction toggles
        case 1: Game_StepZoom(game, dir); break; // Camera Zoom: cycle
        default: break;
    }
}

// Activate (Enter/Space) the highlighted row for the current menu.
static void Game_MenuActivate(Game *game) {
    switch (game->state) {
        case GAME_MENU:
            if (game->menuSelection == 0)      Game_Restart(game);
            else if (game->menuSelection == 1) Game_OpenSettings(game, GAME_MENU);
            else                               game->running = false;
            break;
        case GAME_PAUSED:
            if (game->menuSelection == 0)      Game_Resume(game);
            else if (game->menuSelection == 1) Game_Restart(game);
            else if (game->menuSelection == 2) Game_OpenSettings(game, GAME_PAUSED);
            else                               Game_ToMenu(game);
            break;
        case GAME_SETTINGS:
            if (game->menuSelection == 2)      Game_CloseSettings(game); // Back
            else                               Game_SettingsAdjust(game, +1);
            break;
        case GAME_PLAYER_DEAD:
        case GAME_WIN:
            if (game->menuSelection == 0)      Game_Restart(game);
            else                               Game_ToMenu(game);
            break;
        default: break;
    }
}

// Esc / "back" from within a menu.
static void Game_MenuBack(Game *game) {
    if (game->state == GAME_PAUSED)        Game_Resume(game);
    else if (game->state == GAME_SETTINGS) Game_CloseSettings(game);
}

// Move the menu highlight by `dir` (-1 up, +1 down), wrapping. Shared by the
// keyboard, gamepad D-pad, and left stick.
static void Game_MenuMove(Game *game, int dir) {
    int count = Game_MenuItemCount(game->state);
    if (count > 0) game->menuSelection = (game->menuSelection + dir % count + count) % count;
}

static void Game_MenuKey(Game *game, SDL_Keycode key) {
    switch (key) {
        case SDLK_UP: case SDLK_W:
            Game_MenuMove(game, -1);
            break;
        case SDLK_DOWN: case SDLK_S:
            Game_MenuMove(game, +1);
            break;
        case SDLK_LEFT: case SDLK_A:
            if (game->state == GAME_SETTINGS) Game_SettingsAdjust(game, -1);
            break;
        case SDLK_RIGHT: case SDLK_D:
            if (game->state == GAME_SETTINGS) Game_SettingsAdjust(game, +1);
            break;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE:
            Game_MenuActivate(game);
            break;
        case SDLK_ESCAPE:
            Game_MenuBack(game);
            break;
        default: break;
    }
}

static void Game_OnKeyDown(Game *game, SDL_Keycode key) {
    if (game->state == GAME_PLAYING) {
        if (key == SDLK_ESCAPE || key == SDLK_P) {
            Game_Pause(game);
        } else if (key == SDLK_M) {
            Audio_ToggleMute();
            SDL_Log("Audio %s", Audio_IsMuted() ? "muted" : "unmuted");
        }
    } else {
        Game_MenuKey(game, key);
    }
}

// Gamepad buttons drive menus the same way the keyboard does. During play only
// Start matters here (pause) -- movement/attack/skills are polled in player.c.
static void Game_OnGamepadButton(Game *game, Uint8 button) {
    if (button == SDL_GAMEPAD_BUTTON_START) {
        if (game->state == GAME_PLAYING)      Game_Pause(game);
        else if (game->state == GAME_PAUSED)  Game_Resume(game);
        else                                  Game_MenuActivate(game);
        return;
    }
    if (game->state == GAME_PLAYING) return;

    switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:    Game_MenuMove(game, -1); break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  Game_MenuMove(game, +1); break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            if (game->state == GAME_SETTINGS) Game_SettingsAdjust(game, -1);
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            if (game->state == GAME_SETTINGS) Game_SettingsAdjust(game, +1);
            break;
        case SDL_GAMEPAD_BUTTON_SOUTH:      Game_MenuActivate(game); break;
        case SDL_GAMEPAD_BUTTON_EAST:       Game_MenuBack(game);     break;
        default: break;
    }
}

// Left stick as a menu D-pad. The latch re-arms only after the stick returns
// near center, so one push moves the highlight one row (no runaway scrolling).
static void Game_OnGamepadAxis(Game *game, Uint8 axis, Sint16 value) {
    static int stickLatch = 0;
    if (game->state == GAME_PLAYING || axis != SDL_GAMEPAD_AXIS_LEFTY) {
        stickLatch = 0;
        return;
    }
    const Sint16 threshold = 16000;
    int dir = (value < -threshold) ? -1 : (value > threshold) ? 1 : 0;
    if (dir != 0 && dir != stickLatch) Game_MenuMove(game, dir);
    stickLatch = dir;
}

// Convert a window-space mouse position to the logical (zoom-adjusted) coords
// menus are laid out in, then return the row under it (or -1).
static int Game_MenuRowAtWindow(Game *game, float winX, float winY) {
    int count = Game_MenuItemCount(game->state);
    if (count <= 0) return -1;
    float lx = winX, ly = winY;
    SDL_RenderCoordinatesFromWindow(game->renderer, winX, winY, &lx, &ly);
    return UI_MenuItemAt(game, lx, ly, count);
}

static void Game_OnMouseMotion(Game *game, float winX, float winY) {
    if (game->state == GAME_PLAYING) return; // in-game aiming isn't used
    int row = Game_MenuRowAtWindow(game, winX, winY);
    if (row >= 0) game->menuSelection = row; // hover to highlight
}

static void Game_OnMouseButton(Game *game, Uint8 button, float winX, float winY) {
    if (button != SDL_BUTTON_LEFT) return;
    if (game->state == GAME_PLAYING) return; // clicks attack via polling in player.c
    int row = Game_MenuRowAtWindow(game, winX, winY);
    if (row >= 0) {
        game->menuSelection = row;
        Game_MenuActivate(game); // click selects + activates the row
    }
}

void Game_HandleEvents(Game *game) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                game->running = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                SDL_GetWindowSize(game->window, &game->win.width, &game->win.height);
                break;
            case SDL_EVENT_KEY_DOWN:
                Game_OnKeyDown(game, event.key.key);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                Input_HandleEvent(&event); // keep the active pad handle in sync
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                Game_OnGamepadButton(game, event.gbutton.button);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                Game_OnGamepadAxis(game, event.gaxis.axis, event.gaxis.value);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                Game_OnMouseMotion(game, event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                Game_OnMouseButton(game, event.button.button, event.button.x, event.button.y);
                break;
        }
    }
}

// Resolves every live hitbox against the entities it's allowed to damage.
// This is the one place attacker-vs-target rules live, so adding a new
// attacker (e.g. a second boss) doesn't require touching player/enemy code.
static void Game_ResolveHitboxes(Game *game) {
    // The player is hit-tested as a circle (its adjustable hurtbox); enemies
    // and the boss still use rectangular bounds.
    Circle playerCircle = Player_GetCircle(&game->player);

    for (int i = 0; i < game->hitboxes.count; i++) {
        Hitbox *hb = &game->hitboxes.boxes[i];

        if (hb->source == HITBOX_SOURCE_PLAYER) {
            for (int e = 0; e < game->enemyCount; e++) {
                if (!Enemy_IsAlive(&game->enemies[e])) continue;
                if (Hitbox_HasHitEnemy(hb, e)) continue;
                if (Collision_RectOverlap(hb->rect, Enemy_GetBounds(&game->enemies[e]))) {
                    Enemy_TakeDamage(&game->enemies[e], hb->damage, hb->knockbackX, hb->knockbackY);
                    Hitbox_MarkHitEnemy(hb, e);
                    Audio_PlaySfx(Enemy_IsAlive(&game->enemies[e]) ? SFX_ENEMY_HIT : SFX_ENEMY_DEATH);
                    game->hitStopTimer = HITSTOP_ON_HIT;
                    Camera_Shake(&game->camera, SHAKE_ON_HIT_MAG, SHAKE_ON_HIT_TIME);
                }
            }
            if (game->bossPresent && !hb->hitBoss && Boss_IsAlive(&game->boss)) {
                if (Collision_RectOverlap(hb->rect, Boss_GetBounds(&game->boss))) {
                    Boss_TakeDamage(&game->boss, hb->damage, hb->knockbackX, hb->knockbackY);
                    hb->hitBoss = true;
                    Audio_PlaySfx(SFX_ENEMY_HIT);
                    game->hitStopTimer = HITSTOP_ON_HIT;
                    Camera_Shake(&game->camera, SHAKE_ON_HIT_MAG, SHAKE_ON_HIT_TIME);
                }
            }
        } else if (hb->source == HITBOX_SOURCE_ENEMY || hb->source == HITBOX_SOURCE_BOSS) {
            if (!hb->hitPlayer && Collision_CircleRectOverlap(playerCircle, hb->rect)) {
                int healthBefore = game->player.health;
                Player_TakeDamage(&game->player, hb->damage, hb->knockbackX, hb->knockbackY);
                // Only play the hurt cue when damage actually landed (the player
                // has brief i-frames after a hit that swallow overlapping ones).
                if (game->player.health < healthBefore) {
                    Audio_PlaySfx(SFX_HURT);
                    game->hitStopTimer = HITSTOP_ON_HURT;
                    Camera_Shake(&game->camera, SHAKE_ON_HURT_MAG, SHAKE_ON_HURT_TIME);
                }
                hb->hitPlayer = true;
            }
        }
    }
}

// True once the player has reached the current level's exit point.
static bool Player_AtLevelExit(const Game *game) {
    float playerRight = game->player.x + Player_GetBounds(&game->player).width;
    float exitX = LEVELS[game->currentLevel].exitX;
    if (exitX <= 0.0f) {
        exitX = Map_PixelWidth(&game->map) - LEVEL_EXIT_MARGIN; // default: map's right edge
    }
    return playerRight >= exitX;
}

void Game_Update(Game *game) {
    Uint64 nowTime = SDL_GetTicks();
    game->deltaTime = (nowTime - game->prevTime) / 1000.0f;
    game->prevTime = nowTime;
    if (game->deltaTime > 0.05f) game->deltaTime = 0.05f; // clamp to avoid huge steps on a stall

    // Hit-stop: hold the simulation still for a few frames after an impact so
    // the hit reads as weighty. We still burn real time off the timer, and
    // rendering continues (the frozen frame is exactly what we want on screen).
    if (game->hitStopTimer > 0.0f) {
        game->hitStopTimer -= game->deltaTime;
        return;
    }

    if (game->state == GAME_PLAYING) {
        HitboxList_Update(&game->hitboxes, game->deltaTime);

        Player_Update(&game->player, game->deltaTime, &game->map, &game->hitboxes);

        for (int i = 0; i < game->enemyCount; i++) {
            Enemy_Update(&game->enemies[i], &game->player, i, game->deltaTime, &game->map, &game->hitboxes);
        }

        if (game->bossPresent) {
            Boss_Update(&game->boss, &game->player, game->deltaTime, &game->map, &game->hitboxes);
        }

        Game_ResolveHitboxes(game);

        Rect pb = Player_GetBounds(&game->player);
        // Bias the camera ahead of the player in the direction they face so the
        // player can see more of what's coming. The camera's own smoothing eases
        // the shift when they turn around.
        float lookAhead = game->player.flipHorizontal ? -CAMERA_LOOKAHEAD : CAMERA_LOOKAHEAD;
        float camTargetX = pb.x + pb.width * 0.5f + lookAhead;
        float camTargetY = pb.y + pb.height * 0.5f;
        Camera_Follow(&game->camera, camTargetX, camTargetY, game->deltaTime);
        Camera_Update(&game->camera);

        if (!Player_IsAlive(&game->player)) {
            game->state = GAME_PLAYER_DEAD;
            game->menuSelection = 0;
            Audio_StopMusic();
            Audio_PlaySfx(SFX_PLAYER_DEATH);
            SDL_Log("YOU DIED! Game Over.");
            return;
        }

        bool isFinalLevel = (game->currentLevel >= LEVEL_COUNT - 1);

        // Beating the final boss wins the game outright.
        if (isFinalLevel && game->bossPresent && !Boss_IsAlive(&game->boss)) {
            game->state = GAME_WIN;
            game->menuSelection = 0;
            Audio_StopMusic();
            Audio_PlaySfx(SFX_VICTORY);
            SDL_Log("BOSS DEFEATED! You win!");
            return;
        }

        // Walk off the right edge to advance to the next level. On the final
        // level there's nowhere to advance to -- you win by beating the boss
        // (handled above), so the edge just acts as a wall there.
        if (!isFinalLevel && Player_AtLevelExit(game)) {
            Game_LoadLevel(game, game->currentLevel + 1);
        }
    }
}

// Draws a circle outline (screen space) as a ring of line segments.
static void DrawCircleOutline(SDL_Renderer *renderer, float cx, float cy, float radius) {
    const int segments = 24;
    float prevX = cx + radius, prevY = cy;
    for (int i = 1; i <= segments; i++) {
        float a = (float)i / segments * 2.0f * (float)SDL_PI_D;
        float x = cx + radius * SDL_cosf(a);
        float y = cy + radius * SDL_sinf(a);
        SDL_RenderLine(renderer, prevX, prevY, x, y);
        prevX = x;
        prevY = y;
    }
}

static void RenderHitboxDebug(Game *game) {
    // Attack hitboxes (yellow rectangles)
    SDL_SetRenderDrawColor(game->renderer, 255, 255, 0, 120);
    for (int i = 0; i < game->hitboxes.count; i++) {
        SDL_FRect worldRect = {
            game->hitboxes.boxes[i].rect.x, game->hitboxes.boxes[i].rect.y,
            game->hitboxes.boxes[i].rect.width, game->hitboxes.boxes[i].rect.height
        };
        SDL_FRect screenRect = Camera_ToScreen(&game->camera, worldRect);
        SDL_RenderRect(game->renderer, &screenRect);
    }

    // Player circular hurtbox (cyan ring) -- handy while tuning the radius
    Circle pc = Player_GetCircle(&game->player);
    SDL_SetRenderDrawColor(game->renderer, 0, 220, 255, 200);
    DrawCircleOutline(game->renderer,
                      Camera_ToScreenX(&game->camera, pc.cx),
                      Camera_ToScreenY(&game->camera, pc.cy),
                      pc.radius);
}

// Draws the menu overlay for whatever non-playing state we're in. The item
// lists here mirror Game_MenuItemCount / Game_MenuActivate.
static void Game_RenderCurrentMenu(Game *game) {
    char zoomVal[16];
    SDL_snprintf(zoomVal, sizeof(zoomVal), "%.2fx", (double)game->zoom);
    const char *soundVal = Audio_IsMuted() ? "OFF" : "ON";

    switch (game->state) {
        case GAME_MENU: {
            UIMenuItem items[] = {
                { "Start Game", NULL }, { "Settings", NULL }, { "Quit", NULL },
            };
            UI_RenderMenu(game, "GRAND REGENT", items, 3, game->menuSelection);
            break;
        }
        case GAME_PAUSED: {
            UIMenuItem items[] = {
                { "Resume", NULL }, { "Restart", NULL },
                { "Settings", NULL }, { "Quit to Menu", NULL },
            };
            UI_RenderMenu(game, "PAUSED", items, 4, game->menuSelection);
            break;
        }
        case GAME_SETTINGS: {
            UIMenuItem items[] = {
                { "Sound", soundVal }, { "Camera Zoom", zoomVal }, { "Back", NULL },
            };
            UI_RenderMenu(game, "SETTINGS", items, 3, game->menuSelection);
            break;
        }
        case GAME_PLAYER_DEAD: {
            UIMenuItem items[] = { { "Restart", NULL }, { "Quit to Menu", NULL } };
            UI_RenderMenu(game, "YOU DIED", items, 2, game->menuSelection);
            break;
        }
        case GAME_WIN: {
            UIMenuItem items[] = { { "Play Again", NULL }, { "Quit to Menu", NULL } };
            UI_RenderMenu(game, "VICTORY", items, 2, game->menuSelection);
            break;
        }
        default: break;
    }
}

void Game_Render(Game *game) {
    SDL_SetRenderDrawColor(game->renderer, 20, 10, 30, 255);
    SDL_RenderClear(game->renderer);

    Map_Render(&game->map, game->renderer, &game->camera);

    Player_Render(&game->player, game->renderer, &game->camera);
    for (int i = 0; i < game->enemyCount; i++) {
        Enemy_Render(&game->enemies[i], game->renderer, &game->camera);
    }
    if (game->bossPresent) {
        Boss_Render(&game->boss, game->renderer, &game->camera);
    }

    // Debug outlines and the HUD belong to live play; menus draw over a frozen,
    // clean world instead.
    if (game->state == GAME_PLAYING || game->state == GAME_PAUSED) {
        RenderHitboxDebug(game);
        UI_RenderHUD(game);
    }
    if (game->state != GAME_PLAYING) {
        Game_RenderCurrentMenu(game);
    }

    SDL_RenderPresent(game->renderer);

    frameCount++;
    Uint64 currentTime = SDL_GetTicks();
    if (currentTime - lastFpsTime >= 1000) {
        fps = frameCount;
        frameCount = 0;
        lastFpsTime = currentTime;

        char title[160];
        SDL_snprintf(title, sizeof(title), "Grand Regent - FPS: %d | HP: %d | Boss: %s",
                     fps, game->player.health,
                     game->bossPresent ? (Boss_IsAlive(&game->boss) ? "alive" : "defeated") : "none");
        SDL_SetWindowTitle(game->window, title);
    }
}

void Game_Destroy(Game *game) {
    Audio_Shutdown();
    Input_Shutdown();
    Map_Free(&game->map);
    TextureManager_Destroy(&game->textures);
    if (game->renderer) SDL_DestroyRenderer(game->renderer);
    if (game->window) SDL_DestroyWindow(game->window);
    SDL_Quit();
    SDL_Log("Game destroyed.");
}
