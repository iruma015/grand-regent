#ifndef PLAYER_H
#define PLAYER_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "animation.h"
#include "../combat/collision.h"
#include "../combat/hitbox.h"
#include "../combat/skill.h"
#include "../world/map.h"

typedef enum {
    PLAYER_IDLE,
    PLAYER_RUN,
    PLAYER_ATTACK,
    PLAYER_HURT,
    PLAYER_DEAD
} PlayerState;

typedef struct {
    float x;
    float y;

    float velocityX;
    float velocityY;

    int health;
    int maxHealth;
    bool isGrounded;

    PlayerState state;

    bool flipHorizontal;
    Animation idleAnim;
    Animation runAnim;
    Animation attackAnim;

    SDL_Texture *texture;

    float attackCooldownTimer;
    bool attackHitboxSpawned; // prevents spawning more than one hitbox per swing

    Skill skills[MAX_PLAYER_SKILLS];
    int skillCount;

    float hurtFlashTimer; // brief invulnerability/flash window after taking damage
    int activeSwingSkill; // -1 == normal attack; otherwise index into skills[] powering the current swing

    // Circular hurtbox: this is what enemy/boss attacks are tested against.
    // Tune `hitboxRadius` (and the offsets in Player_GetCircle) to taste.
    float hitboxRadius;
} Player;

void Player_Init(Player *player, float x, float y, SDL_Texture *texture);

// Reads keyboard input, updates physics/animation/combat, and spawns attack
// hitboxes into `hitboxes` when the active frame of an attack/skill swing hits.
void Player_Update(Player *player, float dt, const Map *gameMap, HitboxList *hitboxes);

void Player_Render(Player *player, SDL_Renderer *renderer, const Camera *camera);

void Player_TakeDamage(Player *player, int damage, float knockbackX, float knockbackY);
void Player_Heal(Player *player, int amount);
int Player_IsAlive(Player *player);

Rect Player_GetAttackRange(const Player *player);
Rect Player_GetBounds(const Player *player);

// The player's circular hurtbox in world space (center + radius).
Circle Player_GetCircle(const Player *player);

#endif // PLAYER_H
