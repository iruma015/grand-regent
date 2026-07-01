#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdbool.h>
#include "../combat/collision.h"

// These are doubled from their original values to match the 2x map zoom-out
// (logical resolution 480x270 -> 960x540): doubling the world-space gravity and
// jump velocity keeps the jump ARC looking the same on screen as before, rather
// than a tiny half-height hop at the more zoomed-out scale.
#define GRAVITY 1800.0f
#define MAX_FALL_SPEED 1200.0f
#define JUMP_FORCE -840.0f

// Falling pulls harder than rising. This is a classic "game feel" trick: the
// jump keeps its full height on the way up (so level gaps stay clearable), but
// the descent is snappier so jumps don't feel floaty. Applied only while vy > 0.
#define FALL_GRAVITY_MULT 1.45f

// A moving, collidable box. One Physics_Step() call moves it by its velocity,
// resolves it against the map's solid tiles, and reports whether it ended the
// step standing on the ground.
typedef struct {
    float x, y;          // top-left, world pixels
    float w, h;          // size, world pixels
    float vx, vy;        // velocity, pixels/second
    bool grounded;       // set by Physics_Step: true if resting on a floor
} Body;

// Applies gravity, then moves and collides one axis at a time (horizontal
// then vertical). Resolving each axis right after moving on it is what keeps
// the body from snagging on tile corners/seams when jumping or running along
// walls. Mutates body->x/y/vx/vy/grounded in place.
void Physics_Step(Body *body, const Map *map, float dt);

#endif
