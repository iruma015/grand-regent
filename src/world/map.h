#ifndef MAP_H
#define MAP_H

#include <SDL3/SDL.h>

#include "../core/camera.h"

#define MAP_MAX_TILESETS 16
#define MAP_MAX_LAYERS   16
#define MAP_MAX_SPAWNS   64

// One <tileset> -> one texture atlas, addressed by global tile id (gid)
typedef struct {
    int firstGid;
    int tileCount;
    int columns;
    int tileWidth;
    int tileHeight;
    char imagePath[256];   // relative to the .tmj file's directory
    SDL_Texture *texture;  // resolved by Map_LoadFromTMJ via the TextureManager
} TileSet;

// One renderable layer of gids, width*height in size
typedef struct {
    char name[64];
    int *gids;          // heap-allocated, length = map->width * map->height
    bool isCollision;    // true if Map_IsSolidTile() should consult this layer
} TileLayer;

typedef enum {
    SPAWN_PLAYER,
    SPAWN_ENEMY,
    SPAWN_BOSS,
    SPAWN_UNKNOWN
} SpawnType;

// A point pulled from the "spawns" object layer in Tiled
typedef struct {
    SpawnType type;
    char name[64];
    float x, y;
} MapSpawn;

typedef struct {
    int width, height;          // map size, in tiles
    int tileWidth, tileHeight;  // tile size, in pixels

    TileSet tilesets[MAP_MAX_TILESETS];
    int tilesetCount;

    TileLayer layers[MAP_MAX_LAYERS];
    int layerCount;

    MapSpawn spawns[MAP_MAX_SPAWNS];
    int spawnCount;
} Map;

// Minimal built-in fallback map, used if a .tmj fails to load so the game
// still boots into something playable.
void Map_InitFallback(Map *map);

// Frees heap-allocated layer data. Safe to call on a zeroed Map.
void Map_Free(Map *map);

// Renders all tile layers in order, camera-relative, with off-screen culling.
void Map_Render(Map *map, SDL_Renderer *renderer, const Camera *camera);

// World-space point -> is it inside a tile flagged as collision geometry?
int Map_IsSolidTile(const Map *map, float worldX, float worldY);

float Map_PixelWidth(const Map *map);
float Map_PixelHeight(const Map *map);

// Returns the first spawn matching `type`, or NULL if none was placed in Tiled.
const MapSpawn *Map_FindSpawn(const Map *map, SpawnType type, int occurrence);
int Map_CountSpawns(const Map *map, SpawnType type);

#endif // MAP_H
