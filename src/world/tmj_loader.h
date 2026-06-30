#ifndef TMJ_LOADER_H
#define TMJ_LOADER_H

#include <stdbool.h>
#include "map.h"
#include "../core/texture.h"

/*
    Loads a Tiled JSON (.tmj) map: tile layers, tilesets (textures resolved
    through `texManager`), and the "spawns" object layer (player/enemy/boss
    placement -- see tools/tmx_to_tmj.py for the naming convention).

    Layers whose name contains "ground" or "collision" (case-insensitive)
    are automatically flagged as collision geometry. Returns false (and logs
    why) on failure; the caller should fall back to Map_InitFallback().
*/
bool Map_LoadFromTMJ(Map *map, const char *tmjPath, TextureManager *texManager);

#endif // TMJ_LOADER_H
