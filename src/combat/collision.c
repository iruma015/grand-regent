#include "collision.h"

int Collision_RectOverlap(Rect a, Rect b) {
    if (a.x + a.width < b.x) return 0;
    if (a.x > b.x + b.width) return 0;
    if (a.y + a.height < b.y) return 0;
    if (a.y > b.y + b.height) return 0;
    return 1;
}

int Collision_PointInRect(float point_x, float point_y, Rect rect) {
    return (point_x >= rect.x && point_x <= rect.x + rect.width &&
            point_y >= rect.y && point_y <= rect.y + rect.height);
}

int Collision_CircleRectOverlap(Circle c, Rect r) {
    // Closest point on the rect to the circle center, then distance test.
    float closestX = c.cx;
    if (closestX < r.x) closestX = r.x;
    else if (closestX > r.x + r.width) closestX = r.x + r.width;

    float closestY = c.cy;
    if (closestY < r.y) closestY = r.y;
    else if (closestY > r.y + r.height) closestY = r.y + r.height;

    float dx = c.cx - closestX;
    float dy = c.cy - closestY;
    return (dx * dx + dy * dy) <= (c.radius * c.radius);
}

static int IsTileSolid(const Map *map, int tileX, int tileY) {
    // Only the area *below* the map is solid -- that's the floor, so nothing
    // falls out the bottom of the world. Left/right/top out-of-bounds are NOT
    // solid: invisible side/ceiling walls were what made the player snag and
    // jitter at the map borders, and the level-transition trigger handles the
    // right edge instead.
    if (tileY >= map->height) return 1;
    if (tileX < 0 || tileX >= map->width || tileY < 0) {
        return 0;
    }
    for (int li = 0; li < map->layerCount; li++) {
        const TileLayer *layer = &map->layers[li];
        if (!layer->isCollision || !layer->gids) continue;
        if (layer->gids[tileY * map->width + tileX] != 0) return 1;
    }
    return 0;
}

CollisionInfo Collision_ResolveVertical(Rect *entityBounds, const Map *map, float *velocityY) {
    CollisionInfo info = {0, COLLISION_NONE};
    const int tileW = map->tileWidth;
    const int tileH = map->tileHeight;

    if (*velocityY > 0) {
        int tileY = (int)((entityBounds->y + entityBounds->height) / tileH);
        int tileX1 = (int)(entityBounds->x / tileW);
        int tileX2 = (int)((entityBounds->x + entityBounds->width - 1) / tileW);

        int hasCollision = 0;
        for (int x = tileX1; x <= tileX2 && !hasCollision; x++) {
            if (IsTileSolid(map, x, tileY)) hasCollision = 1;
        }

        if (hasCollision) {
            float tileTopY = (float)(tileY * tileH);
            info.penetration = (entityBounds->y + entityBounds->height) - tileTopY;
            entityBounds->y = tileTopY - entityBounds->height;
            *velocityY = 0;
            info.direction = COLLISION_BOTTOM;
        }
    } else if (*velocityY < 0) {
        int tileY = (int)(entityBounds->y / tileH);
        int tileX1 = (int)(entityBounds->x / tileW);
        int tileX2 = (int)((entityBounds->x + entityBounds->width - 1) / tileW);

        int hasCollision = 0;
        for (int x = tileX1; x <= tileX2 && !hasCollision; x++) {
            if (IsTileSolid(map, x, tileY)) hasCollision = 1;
        }

        if (hasCollision) {
            float tileBottomY = (float)((tileY + 1) * tileH);
            info.penetration = tileBottomY - entityBounds->y;
            entityBounds->y = tileBottomY;
            *velocityY = 0;
            info.direction = COLLISION_TOP;
        }
    }

    return info;
}

CollisionInfo Collision_ResolveHorizontal(Rect *entityBounds, const Map *map, float *velocityX) {
    CollisionInfo info = {0, COLLISION_NONE};
    const int tileW = map->tileWidth;
    const int tileH = map->tileHeight;

    if (*velocityX > 0) {
        int tileX = (int)((entityBounds->x + entityBounds->width) / tileW);
        int tileY1 = (int)(entityBounds->y / tileH);
        int tileY2 = (int)((entityBounds->y + entityBounds->height - 1) / tileH);

        int hasCollision = 0;
        for (int y = tileY1; y <= tileY2 && !hasCollision; y++) {
            if (IsTileSolid(map, tileX, y)) hasCollision = 1;
        }

        if (hasCollision) {
            float tileLeftX = (float)(tileX * tileW);
            info.penetration = (entityBounds->x + entityBounds->width) - tileLeftX;
            entityBounds->x = tileLeftX - entityBounds->width;
            *velocityX = 0;
            info.direction = COLLISION_RIGHT;
        }
    } else if (*velocityX < 0) {
        int tileX = (int)(entityBounds->x / tileW);
        int tileY1 = (int)(entityBounds->y / tileH);
        int tileY2 = (int)((entityBounds->y + entityBounds->height - 1) / tileH);

        int hasCollision = 0;
        for (int y = tileY1; y <= tileY2 && !hasCollision; y++) {
            if (IsTileSolid(map, tileX, y)) hasCollision = 1;
        }

        if (hasCollision) {
            float tileRightX = (float)((tileX + 1) * tileW);
            info.penetration = tileRightX - entityBounds->x;
            entityBounds->x = tileRightX;
            *velocityX = 0;
            info.direction = COLLISION_LEFT;
        }
    }

    return info;
}
