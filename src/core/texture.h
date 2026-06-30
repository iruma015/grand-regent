#ifndef TEXTURE_H
#define TEXTURE_H

#include <SDL3/SDL.h>

/**
    TextureManager: Manage loading and caching textures
    This helps us avoid loading the same texture multiple times,
    improving performance and memory usage.
*/

typedef struct {
    char filename[256];
    SDL_Texture *texture;
} TextureEntry;

typedef struct {
    TextureEntry entries[32];  // Can store up to 32 textures
    int count;
    SDL_Renderer *renderer;
} TextureManager;

// Initialize the texture manager
void TextureManager_Init(TextureManager *manager, SDL_Renderer *renderer);

// Load a texture (will use cached version if already loaded)
SDL_Texture* TextureManager_Load(TextureManager *manager, const char *filename);

// Free all textures
void TextureManager_Destroy(TextureManager *manager);

#endif // TEXTURE_H
