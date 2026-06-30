#ifndef CAMERA_H
#define CAMERA_H

#include <SDL3/SDL.h>

/*
    Camera

    Owns the current view into the world. Everything renders relative to
    the camera's top-left world position (camera.x, camera.y), so moving
    the camera is all it takes to "scroll" the level.

    Usage each frame:
        Camera_Follow(&cam, player.x + PLAYER_WIDTH/2, player.y, dt);
        Camera_Update(&cam); // clamps to world bounds

    Then when rendering anything, convert its world rect to a screen rect:
        SDL_FRect screenRect = Camera_ToScreen(&cam, worldRect);
*/

typedef struct Camera {
    float x, y;             // top-left of the view, in world pixels
    float viewW, viewH;     // size of the viewport (use the logical resolution)
    float worldW, worldH;   // size of the playable world in pixels (map bounds)
    float followLerp;       // 0..1 smoothing factor applied per second
    float targetX, targetY; // last follow target (center point the camera wants)
} Camera;

// viewW/viewH should match the game's logical resolution (Win.logW/logH)
void Camera_Init(Camera *cam, float viewW, float viewH);

// Call once the map is loaded so the camera knows how far it's allowed to scroll
void Camera_SetWorldBounds(Camera *cam, float worldW, float worldH);

// Smoothly move the camera so that (targetX, targetY) sits at the center of the view
void Camera_Follow(Camera *cam, float targetX, float targetY, float dt);

// Snap the camera straight to a position (e.g. on level start, no smoothing)
void Camera_SnapTo(Camera *cam, float targetX, float targetY);

// Clamps camera.x/y to stay within world bounds. Call after Follow/SnapTo.
void Camera_Update(Camera *cam);

// Convert a world-space rect to a screen-space rect for rendering
SDL_FRect Camera_ToScreen(const Camera *cam, SDL_FRect worldRect);
float Camera_ToScreenX(const Camera *cam, float worldX);
float Camera_ToScreenY(const Camera *cam, float worldY);

// Quick visibility check so callers can skip rendering off-screen things
bool Camera_RectVisible(const Camera *cam, SDL_FRect worldRect);

#endif // CAMERA_H
