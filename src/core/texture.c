#include "texture.h"
#include <SDL3_image/SDL_image.h>
#include <string.h>


void TextureManager_Init(TextureManager *manager, SDL_Renderer *renderer) {
    manager->renderer = renderer;
    manager->count = 0;
    memset(manager->entries, 0, sizeof(manager->entries));
}


SDL_Texture* TextureManager_Load(TextureManager *manager, const char *filename) {
    // Check if texture is already loaded
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->entries[i].filename, filename) == 0) {
            return manager->entries[i].texture;
        }
    }
    
    // Texture not found, load it
    if (manager->count >= 32) {
        SDL_Log("ERROR: TextureManager full, cannot load more textures");
        return NULL;
    }
    
    SDL_Texture *texture = IMG_LoadTexture(manager->renderer, filename);
    if (!texture) {
        SDL_Log("ERROR: Failed to load texture '%s': %s", filename, SDL_GetError());
        return NULL;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    
    // Add to cache
    strcpy(manager->entries[manager->count].filename, filename);
    manager->entries[manager->count].texture = texture;
    manager->count++;
    
    SDL_Log("Loaded texture: %s", filename);
    return texture;
}


void TextureManager_Destroy(TextureManager *manager) {
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].texture) {
            SDL_DestroyTexture(manager->entries[i].texture);
        }
    }
    manager->count = 0;
}
