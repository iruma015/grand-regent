#ifndef COLLISION_H
#define COLLISION_H

#include "../world/map.h"

typedef struct {
    float x, y;
    float width, height;
} Rect;

// A circular hurtbox/hitbox. (cx, cy) is the center in world pixels.
typedef struct {
    float cx, cy;
    float radius;
} Circle;

typedef struct {
    float penetration;
    int direction;
} CollisionInfo;

#define COLLISION_NONE   0
#define COLLISION_BOTTOM 1
#define COLLISION_TOP    2
#define COLLISION_LEFT   3
#define COLLISION_RIGHT  4

int Collision_RectOverlap(Rect a, Rect b);
int Collision_PointInRect(float point_x, float point_y, Rect rect);

// True if a circle overlaps an axis-aligned rect. Used so the player's
// circular hurtbox can be tested against rectangular attack hitboxes.
int Collision_CircleRectOverlap(Circle c, Rect r);

// Resolves vertical/horizontal movement against the map's collision layer(s).
// Mutates entityBounds->x/y and zeroes the relevant velocity component on impact.
CollisionInfo Collision_ResolveVertical(Rect *entityBounds, const Map *map, float *velocityY);
CollisionInfo Collision_ResolveHorizontal(Rect *entityBounds, const Map *map, float *velocityX);

#endif // COLLISION_H
