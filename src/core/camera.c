#include "camera.h"

void Camera_Init(Camera *cam, float viewW, float viewH) {
    cam->x = 0.0f;
    cam->y = 0.0f;
    cam->viewW = viewW;
    cam->viewH = viewH;
    cam->worldW = viewW;
    cam->worldH = viewH;
    cam->followLerp = 6.0f; // higher = snappier follow, lower = floatier
    cam->targetX = viewW * 0.5f;
    cam->targetY = viewH * 0.5f;
}

void Camera_SetWorldBounds(Camera *cam, float worldW, float worldH) {
    cam->worldW = worldW;
    cam->worldH = worldH;
}

static float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

void Camera_Follow(Camera *cam, float targetX, float targetY, float dt) {
    cam->targetX = targetX;
    cam->targetY = targetY;

    float desiredX = targetX - cam->viewW * 0.5f;
    float desiredY = targetY - cam->viewH * 0.5f;

    // Exponential smoothing, frame-rate independent
    float t = 1.0f - SDL_expf(-cam->followLerp * dt);
    if (t > 1.0f) t = 1.0f;

    cam->x = Lerp(cam->x, desiredX, t);
    cam->y = Lerp(cam->y, desiredY, t);
}

void Camera_SnapTo(Camera *cam, float targetX, float targetY) {
    cam->targetX = targetX;
    cam->targetY = targetY;
    cam->x = targetX - cam->viewW * 0.5f;
    cam->y = targetY - cam->viewH * 0.5f;
    Camera_Update(cam);
}

void Camera_Update(Camera *cam) {
    float maxX = cam->worldW - cam->viewW;
    float maxY = cam->worldH - cam->viewH;

    if (maxX < 0.0f) {
        // World is narrower than the view: center it instead of clamping to [0,0]
        cam->x = -((cam->viewW - cam->worldW) * 0.5f);
    } else {
        if (cam->x < 0.0f) cam->x = 0.0f;
        if (cam->x > maxX) cam->x = maxX;
    }

    if (maxY < 0.0f) {
        cam->y = -((cam->viewH - cam->worldH) * 0.5f);
    } else {
        if (cam->y < 0.0f) cam->y = 0.0f;
        if (cam->y > maxY) cam->y = maxY;
    }
}

SDL_FRect Camera_ToScreen(const Camera *cam, SDL_FRect worldRect) {
    SDL_FRect out = worldRect;
    out.x -= cam->x;
    out.y -= cam->y;
    return out;
}

float Camera_ToScreenX(const Camera *cam, float worldX) {
    return worldX - cam->x;
}

float Camera_ToScreenY(const Camera *cam, float worldY) {
    return worldY - cam->y;
}

bool Camera_RectVisible(const Camera *cam, SDL_FRect worldRect) {
    return !(worldRect.x + worldRect.w < cam->x ||
             worldRect.x > cam->x + cam->viewW ||
             worldRect.y + worldRect.h < cam->y ||
             worldRect.y > cam->y + cam->viewH);
}
