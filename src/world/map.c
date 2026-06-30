#include "map.h"
#include "../core/camera.h"
#include <stdlib.h>
#include <string.h>

void Map_InitFallback(Map *map) {
    memset(map, 0, sizeof(Map));

    map->width = 20;
    map->height = 10;
    map->tileWidth = 32;
    map->tileHeight = 32;

    map->layerCount = 1;
    TileLayer *layer = &map->layers[0];
    SDL_strlcpy(layer->name, "fallback-ground", sizeof(layer->name));
    layer->isCollision = true;
    layer->gids = (int *)calloc((size_t)(map->width * map->height), sizeof(int));

    // Simple floor + side walls so the player has something to stand on
    for (int x = 0; x < map->width; x++) {
        layer->gids[8 * map->width + x] = 1; // floor row
    }
    for (int y = 0; y < map->height; y++) {
        layer->gids[y * map->width + 0] = 1;
        layer->gids[y * map->width + (map->width - 1)] = 1;
    }

    map->tilesetCount = 0; // no texture; renderer falls back to a flat color

    map->spawnCount = 0;
}

void Map_Free(Map *map) {
    for (int i = 0; i < map->layerCount; i++) {
        free(map->layers[i].gids);
        map->layers[i].gids = NULL;
    }
    map->layerCount = 0;
}

static const TileSet *FindTilesetForGid(const Map *map, int gid) {
    const TileSet *best = NULL;
    for (int i = 0; i < map->tilesetCount; i++) {
        const TileSet *ts = &map->tilesets[i];
        if (gid >= ts->firstGid && (best == NULL || ts->firstGid > best->firstGid)) {
            best = ts;
        }
    }
    return best;
}

void Map_Render(Map *map, SDL_Renderer *renderer, const Camera *camera) {
    // Tiled sets high bits on a gid to flag flips/rotations; we don't support
    // those yet, so mask them off rather than misreading the tile id.
    const unsigned int FLIP_MASK = 0x1FFFFFFFu;

    for (int li = 0; li < map->layerCount; li++) {
        TileLayer *layer = &map->layers[li];
        if (!layer->gids) continue;

        // Cull to the tiles actually visible through the camera
        int startX = 0, startY = 0, endX = map->width - 1, endY = map->height - 1;
        if (camera) {
            startX = (int)(camera->x / map->tileWidth) - 1;
            startY = (int)(camera->y / map->tileHeight) - 1;
            endX = (int)((camera->x + camera->viewW) / map->tileWidth) + 1;
            endY = (int)((camera->y + camera->viewH) / map->tileHeight) + 1;
            if (startX < 0) startX = 0;
            if (startY < 0) startY = 0;
            if (endX > map->width - 1) endX = map->width - 1;
            if (endY > map->height - 1) endY = map->height - 1;
        }

        for (int y = startY; y <= endY; y++) {
            for (int x = startX; x <= endX; x++) {
                unsigned int rawGid = (unsigned int)layer->gids[y * map->width + x];
                int gid = (int)(rawGid & FLIP_MASK);
                if (gid == 0) continue;

                SDL_FRect dst = {
                    (float)(x * map->tileWidth),
                    (float)(y * map->tileHeight),
                    (float)map->tileWidth,
                    (float)map->tileHeight
                };
                if (camera) {
                    dst = Camera_ToScreen(camera, dst);
                }

                const TileSet *ts = FindTilesetForGid(map, gid);
                if (ts && ts->texture) {
                    int localId = gid - ts->firstGid;
                    int col = localId % ts->columns;
                    int row = localId / ts->columns;
                    SDL_FRect src = {
                        (float)(col * ts->tileWidth),
                        (float)(row * ts->tileHeight),
                        (float)ts->tileWidth,
                        (float)ts->tileHeight
                    };
                    SDL_RenderTexture(renderer, ts->texture, &src, &dst);
                } else {
                    // No texture resolved (fallback map, or asset failed to load):
                    // draw a flat block so collision geometry is still visible.
                    SDL_SetRenderDrawColor(renderer, 110, 90, 70, 255);
                    SDL_RenderFillRect(renderer, &dst);
                }
            }
        }
    }
}

int Map_IsSolidTile(const Map *map, float worldX, float worldY) {
    int tileX = (int)(worldX / map->tileWidth);
    int tileY = (int)(worldY / map->tileHeight);

    if (tileY >= map->height) {
        return 1; // below the map is solid floor, so entities can't fall out the bottom
    }
    if (tileX < 0 || tileX >= map->width || tileY < 0) {
        return 0; // sides/ceiling are open (no invisible walls at the borders)
    }

    for (int li = 0; li < map->layerCount; li++) {
        const TileLayer *layer = &map->layers[li];
        if (!layer->isCollision || !layer->gids) continue;
        if (layer->gids[tileY * map->width + tileX] != 0) {
            return 1;
        }
    }
    return 0;
}

float Map_PixelWidth(const Map *map) {
    return (float)(map->width * map->tileWidth);
}

float Map_PixelHeight(const Map *map) {
    return (float)(map->height * map->tileHeight);
}

const MapSpawn *Map_FindSpawn(const Map *map, SpawnType type, int occurrence) {
    int seen = 0;
    for (int i = 0; i < map->spawnCount; i++) {
        if (map->spawns[i].type == type) {
            if (seen == occurrence) return &map->spawns[i];
            seen++;
        }
    }
    return NULL;
}

int Map_CountSpawns(const Map *map, SpawnType type) {
    int count = 0;
    for (int i = 0; i < map->spawnCount; i++) {
        if (map->spawns[i].type == type) count++;
    }
    return count;
}
