#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "core/game.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Game *game = calloc(1, sizeof(Game));
    if (!game) {
        SDL_Log("Failed to allocate memory for game");
        return 1;
    }

    game->win.width = 1600;
    game->win.height = 900;
    // Logical (render) resolution. Smaller = more zoomed in, so everything on
    // screen looks bigger. 480x270 is 16:9 like the window, so it scales up
    // cleanly with no letterbox bars. Lower these to zoom in further.
    game->win.logW = 480;
    game->win.logH = 270;

    if (!Game_Init(game)) {
        SDL_Log("Failed to initialize game");
        free(game);
        return 1;
    }

    game->running = true;
    while (game->running) {
        Game_HandleEvents(game);
        Game_Update(game);
        Game_Render(game);

        // With VSync on, the renderer paces frames for us (smoother, tear-free).
        // Only fall back to a fixed delay when VSync couldn't be enabled.
        if (!game->vsyncEnabled) {
            SDL_Delay(16);
        }
    }

    Game_Destroy(game);
    free(game);

    return 0;
}
