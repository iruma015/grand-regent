#ifndef BOSS_H
#define BOSS_H

#include <SDL3/SDL.h>
#include "animation.h"
#include "../combat/collision.h"
#include "../combat/hitbox.h"
#include "../world/map.h"
#include "player.h"

typedef enum {
    BOSS_IDLE,
    BOSS_WINDUP,   // telegraph before a skill becomes active (gives the player a dodge window)
    BOSS_ACTIVE,   // skill's hitbox is live
    BOSS_RECOVER,  // brief vulnerable recovery after a skill
    BOSS_HURT,
    BOSS_DEAD
} BossState;

// One row = one boss skill. Add a row here to give the boss a new attack --
// nothing else in the engine needs to change.
typedef struct {
    const char *name;
    float cooldown;       // minimum time between uses of this skill
    float range;           // horizontal reach of the hitbox
    int damage;
    float windupTime;       // telegraph duration (boss is vulnerable, doesn't move)
    float activeTime;        // how long the hitbox stays live
    float recoverTime;        // post-attack vulnerable window
    int animFrames;
    float frameDuration;
} BossSkillDef;

#define BOSS_MAX_SKILLS 4

typedef struct {
    float x;
    float y;

    float velocityX;
    float velocityY;

    int health;
    int maxHealth;

    BossState state;
    bool facingRight; // which way the sprite faces (flips the texture when false)

    Animation idleAnim;
    Animation walkAnim;  // played while chasing/patrolling
    Animation skillAnim; // reused for whichever skill is currently playing

    SDL_Texture *texture;

    const BossSkillDef *skills;
    int skillCount;
    int currentSkill;       // index into `skills` for the attack in progress
    float skillTimer;        // counts down within the current state (windup/active/recover)
    float skillCooldowns[BOSS_MAX_SKILLS];
    bool hitboxSpawnedThisAttack;

    float patrolAnchorX;     // left edge of the boss's patrol range
    float patrolRangeX;       // width of the patrol range
} Boss;

void Boss_Init(Boss *boss, float x, float y, SDL_Texture *texture, float mapPixelWidth);

void Boss_Update(Boss *boss, Player *player, float dt, const Map *gameMap, HitboxList *hitboxes);
void Boss_Render(Boss *boss, SDL_Renderer *renderer, const Camera *camera);

void Boss_TakeDamage(Boss *boss, int damage, float knockbackX, float knockbackY);
int Boss_IsAlive(Boss *boss);

Rect Boss_GetBounds(const Boss *boss);

#endif // BOSS_H
