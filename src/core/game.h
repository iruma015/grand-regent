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
    GAME_PLAYING,
    GAME_PLAYER_DEAD,
    GAME_BOSS_DEAD,
    GAME_WIN          // cleared the final level
} GameState;

typedef struct {
    int width, height, logW, logH;
} Win;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Win win;

    bool running;
    GameState state;

    float deltaTime;
    Uint64 prevTime;

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
