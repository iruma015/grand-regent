#include "physics.h"

void Physics_Step(Body *body, const Map *map, float dt) {
    // Gravity (accumulates downward velocity, capped at terminal speed).
    body->vy += GRAVITY * dt;
    if (body->vy > MAX_FALL_SPEED) {
        body->vy = MAX_FALL_SPEED;
    }

    // --- Horizontal axis: move, then resolve against the tiles spanning the
    //     body's height. ResolveHorizontal zeroes vx on impact.
    body->x += body->vx * dt;
    Rect bounds = { body->x, body->y, body->w, body->h };
    Collision_ResolveHorizontal(&bounds, map, &body->vx);
    body->x = bounds.x;

    // --- Vertical axis: move, then resolve against the tiles spanning the
    //     body's width. A BOTTOM hit means we landed on a floor.
    body->y += body->vy * dt;
    bounds.x = body->x;
    bounds.y = body->y;
    CollisionInfo info = Collision_ResolveVertical(&bounds, map, &body->vy);
    body->y = bounds.y;

    body->grounded = (info.direction == COLLISION_BOTTOM);
}
