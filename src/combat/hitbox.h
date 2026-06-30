#ifndef HITBOX_H
#define HITBOX_H

#include "collision.h"
#include <stdbool.h>

/*
    Hitbox system

    A single, central place that owns "did an attack land" logic for every
    attacker in the game (player normal attacks, player skills, enemy
    attacks, boss attacks/skills). Instead of each entity hard-coding its
    own damage-application code, attackers just spawn a Hitbox describing
    where damage should land, and the game loop resolves all of them
    against the right targets each frame. Adding a new attack anywhere is
    then just one HitboxList_Spawn() call -- nothing else needs to know
    about it.
*/

typedef enum {
    HITBOX_SOURCE_PLAYER,
    HITBOX_SOURCE_ENEMY,
    HITBOX_SOURCE_BOSS
} HitboxSource;

#define HITBOX_MAX_ENEMY_TARGETS 32

typedef struct {
    Rect rect;
    int damage;
    HitboxSource source;
    int sourceId;        // which enemy index, if source == HITBOX_SOURCE_ENEMY

    float knockbackX;
    float knockbackY;
    float lifetime;       // seconds remaining; auto-removed at/below 0

    // Each hitbox damages a given target at most once over its lifetime,
    // even though it may be tested against that target across several
    // frames. This still lets one swing cleave several different enemies.
    Uint32 hitEnemyMask;  // bit i set => already damaged enemy i
    bool hitPlayer;
    bool hitBoss;
} Hitbox;

#define MAX_HITBOXES 32

typedef struct {
    Hitbox boxes[MAX_HITBOXES];
    int count;
} HitboxList;

void HitboxList_Clear(HitboxList *list);

// Ages out expired hitboxes (call once per frame before spawning new ones)
void HitboxList_Update(HitboxList *list, float dt);

// Returns the index of the new hitbox, or -1 if the pool is full
int HitboxList_Spawn(HitboxList *list, Rect rect, int damage, HitboxSource source,
                      int sourceId, float knockbackX, float knockbackY, float lifetime);

bool Hitbox_HasHitEnemy(const Hitbox *hb, int enemyIndex);
void Hitbox_MarkHitEnemy(Hitbox *hb, int enemyIndex);

#endif // HITBOX_H
