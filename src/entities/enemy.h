#ifndef ENEMY_H
#define ENEMY_H

#include <SDL3/SDL.h>
#include "animation.h"
#include "../combat/collision.h"
#include "../combat/hitbox.h"
#include "../world/map.h"
#include "player.h"

typedef enum {
    ENEMY_IDLE,
    ENEMY_RUN,
    ENEMY_ATTACK,
    ENEMY_HURT,
    ENEMY_DEAD
} EnemyState;

typedef struct {
    float x;
    float y;

    float velocityX;
    float velocityY;

    int health;
    int maxHealth;

    EnemyState state;
    bool flipHorizontal; // face the player instead of always facing one way

    Animation idleAnim;
    Animation runAnim;
    Animation attackAnim;

    SDL_Texture *texture;

    float attackCooldownTimer;
    bool attackHitboxSpawned;
    float hurtTimer;
} Enemy;

void Enemy_Init(Enemy *enemy, float x, float y, SDL_Texture *texture);

// `enemyIndex` identifies this enemy within the game's enemy array; it's
// stamped onto any hitbox it spawns so per-target hit dedup works correctly.
void Enemy_Update(Enemy *enemy, Player *player, int enemyIndex, float dt, const Map *gameMap, HitboxList *hitboxes);

void Enemy_Render(Enemy *enemy, SDL_Renderer *renderer, const Camera *camera);

void Enemy_TakeDamage(Enemy *enemy, int damage, float knockbackX, float knockbackY);
int Enemy_IsAlive(Enemy *enemy);

Rect Enemy_GetAttackRange(const Enemy *enemy);
Rect Enemy_GetBounds(const Enemy *enemy);

#endif // ENEMY_H
