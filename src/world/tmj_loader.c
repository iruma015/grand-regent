#include "tmj_loader.h"
#include "../third_party/cJSON.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *ReadEntireFile(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) { fclose(file); return NULL; }

    char *content = (char *)malloc((size_t)size + 1);
    if (!content) { fclose(file); return NULL; }

    size_t readBytes = fread(content, 1, (size_t)size, file);
    content[readBytes] = '\0';
    fclose(file);
    return content;
}

static void GetDirectory(const char *path, char *out, size_t outSize) {
    const char *lastSlash = strrchr(path, '/');
#ifdef _WIN32
    const char *lastBackslash = strrchr(path, '\\');
    if (lastBackslash && (!lastSlash || lastBackslash > lastSlash)) lastSlash = lastBackslash;
#endif
    if (!lastSlash) {
        out[0] = '\0';
        return;
    }
    size_t len = (size_t)(lastSlash - path);
    if (len >= outSize) len = outSize - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

static bool StrContainsCI(const char *haystack, const char *needle) {
    if (!haystack || !*haystack) return false;
    size_t hn = strlen(haystack), nn = strlen(needle);
    if (nn > hn) return false;
    for (size_t i = 0; i + nn <= hn; i++) {
        size_t j = 0;
        for (; j < nn; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) break;
        }
        if (j == nn) return true;
    }
    return false;
}

static SpawnType ClassifySpawn(const char *name, const char *type) {
    if (StrContainsCI(name, "player") || StrContainsCI(type, "player")) return SPAWN_PLAYER;
    if (StrContainsCI(name, "boss") || StrContainsCI(type, "boss")) return SPAWN_BOSS;
    if (StrContainsCI(name, "enemy") || StrContainsCI(type, "enemy")) return SPAWN_ENEMY;
    return SPAWN_UNKNOWN;
}

bool Map_LoadFromTMJ(Map *map, const char *tmjPath, TextureManager *texManager) {
    char *jsonText = ReadEntireFile(tmjPath);
    if (!jsonText) {
        SDL_Log("Map_LoadFromTMJ: could not open '%s'", tmjPath);
        return false;
    }

    cJSON *root = cJSON_Parse(jsonText);
    free(jsonText);
    if (!root) {
        SDL_Log("Map_LoadFromTMJ: JSON parse error in '%s' near: %s",
                tmjPath, cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "?");
        return false;
    }

    char baseDir[256];
    GetDirectory(tmjPath, baseDir, sizeof(baseDir));

    memset(map, 0, sizeof(Map));

    cJSON *jWidth = cJSON_GetObjectItemCaseSensitive(root, "width");
    cJSON *jHeight = cJSON_GetObjectItemCaseSensitive(root, "height");
    cJSON *jTileW = cJSON_GetObjectItemCaseSensitive(root, "tilewidth");
    cJSON *jTileH = cJSON_GetObjectItemCaseSensitive(root, "tileheight");
    if (!cJSON_IsNumber(jWidth) || !cJSON_IsNumber(jHeight) ||
        !cJSON_IsNumber(jTileW) || !cJSON_IsNumber(jTileH)) {
        SDL_Log("Map_LoadFromTMJ: '%s' is missing width/height/tilewidth/tileheight", tmjPath);
        cJSON_Delete(root);
        return false;
    }
    map->width = jWidth->valueint;
    map->height = jHeight->valueint;
    map->tileWidth = jTileW->valueint;
    map->tileHeight = jTileH->valueint;

    // ---- Tilesets (embedded only; external .tsx references aren't supported) ----
    cJSON *jTilesets = cJSON_GetObjectItemCaseSensitive(root, "tilesets");
    cJSON *jTileset = NULL;
    cJSON_ArrayForEach(jTileset, jTilesets) {
        if (map->tilesetCount >= MAP_MAX_TILESETS) {
            SDL_Log("Map_LoadFromTMJ: tileset limit (%d) reached, skipping rest", MAP_MAX_TILESETS);
            break;
        }
        cJSON *jImage = cJSON_GetObjectItemCaseSensitive(jTileset, "image");
        if (!cJSON_IsString(jImage)) {
            // External tileset (.tsx reference via "source") -- not supported.
            cJSON *jSource = cJSON_GetObjectItemCaseSensitive(jTileset, "source");
            SDL_Log("Map_LoadFromTMJ: external tileset '%s' not supported, skipping "
                    "(re-export from Tiled with 'Embed tilesets' enabled)",
                    cJSON_IsString(jSource) ? jSource->valuestring : "?");
            continue;
        }

        TileSet *ts = &map->tilesets[map->tilesetCount++];
        ts->firstGid = cJSON_GetObjectItemCaseSensitive(jTileset, "firstgid")->valueint;
        ts->tileCount = cJSON_GetObjectItemCaseSensitive(jTileset, "tilecount")->valueint;
        ts->columns = cJSON_GetObjectItemCaseSensitive(jTileset, "columns")->valueint;
        ts->tileWidth = cJSON_GetObjectItemCaseSensitive(jTileset, "tilewidth")->valueint;
        ts->tileHeight = cJSON_GetObjectItemCaseSensitive(jTileset, "tileheight")->valueint;

        char fullPath[256];
        if (baseDir[0]) {
            SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s", baseDir, jImage->valuestring);
        } else {
            SDL_snprintf(fullPath, sizeof(fullPath), "%s", jImage->valuestring);
        }
        SDL_strlcpy(ts->imagePath, fullPath, sizeof(ts->imagePath));
        ts->texture = TextureManager_Load(texManager, fullPath);
        if (!ts->texture) {
            SDL_Log("Map_LoadFromTMJ: failed to load tileset texture '%s'", fullPath);
        }
    }

    // ---- Layers (tile layers + the "spawns" object layer) ----
    cJSON *jLayers = cJSON_GetObjectItemCaseSensitive(root, "layers");
    cJSON *jLayer = NULL;
    cJSON_ArrayForEach(jLayer, jLayers) {
        cJSON *jType = cJSON_GetObjectItemCaseSensitive(jLayer, "type");
        if (!cJSON_IsString(jType)) continue;

        if (strcmp(jType->valuestring, "tilelayer") == 0) {
            if (map->layerCount >= MAP_MAX_LAYERS) {
                SDL_Log("Map_LoadFromTMJ: layer limit (%d) reached, skipping rest", MAP_MAX_LAYERS);
                continue;
            }
            TileLayer *layer = &map->layers[map->layerCount++];
            memset(layer, 0, sizeof(TileLayer));

            cJSON *jName = cJSON_GetObjectItemCaseSensitive(jLayer, "name");
            if (cJSON_IsString(jName)) {
                SDL_strlcpy(layer->name, jName->valuestring, sizeof(layer->name));
            }
            // Match the SDL2 build: only the layer literally named "first-ground"
            // is solid. Other ground-ish layers (e.g. "second ground") are purely
            // decorative, exactly as in sdl2_tilemap_merged.
            layer->isCollision = SDL_strcasecmp(layer->name, "first-ground") == 0;

            cJSON *jData = cJSON_GetObjectItemCaseSensitive(jLayer, "data");
            int total = map->width * map->height;
            layer->gids = (int *)calloc((size_t)total, sizeof(int));
            int i = 0;
            cJSON *jGid = NULL;
            cJSON_ArrayForEach(jGid, jData) {
                if (i >= total) break;
                layer->gids[i++] = jGid->valueint;
            }
        } else if (strcmp(jType->valuestring, "objectgroup") == 0) {
            cJSON *jObjects = cJSON_GetObjectItemCaseSensitive(jLayer, "objects");
            cJSON *jObj = NULL;
            cJSON_ArrayForEach(jObj, jObjects) {
                if (map->spawnCount >= MAP_MAX_SPAWNS) {
                    SDL_Log("Map_LoadFromTMJ: spawn limit (%d) reached, skipping rest", MAP_MAX_SPAWNS);
                    break;
                }
                cJSON *jName = cJSON_GetObjectItemCaseSensitive(jObj, "name");
                cJSON *jObjType = cJSON_GetObjectItemCaseSensitive(jObj, "type");
                cJSON *jX = cJSON_GetObjectItemCaseSensitive(jObj, "x");
                cJSON *jY = cJSON_GetObjectItemCaseSensitive(jObj, "y");

                const char *name = cJSON_IsString(jName) ? jName->valuestring : "";
                const char *otype = cJSON_IsString(jObjType) ? jObjType->valuestring : "";

                MapSpawn *spawn = &map->spawns[map->spawnCount++];
                SDL_strlcpy(spawn->name, name, sizeof(spawn->name));
                spawn->x = cJSON_IsNumber(jX) ? (float)jX->valuedouble : 0.0f;
                spawn->y = cJSON_IsNumber(jY) ? (float)jY->valuedouble : 0.0f;
                spawn->type = ClassifySpawn(name, otype);
            }
        }
        // other layer types (imagelayer, group) are ignored for now
    }

    cJSON_Delete(root);

    SDL_Log("Map_LoadFromTMJ: loaded '%s' (%dx%d tiles, %d tileset(s), %d layer(s), %d spawn(s))",
            tmjPath, map->width, map->height, map->tilesetCount, map->layerCount, map->spawnCount);
    return true;
}
