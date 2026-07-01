#include "boss.h"
#include "physics.h"
#include <math.h>
#include <string.h>

#define BOSS_WIDTH 120.0f
#define BOSS_HEIGHT 144.0f
#define BOSS_SPEED 160.0f
#define BOSS_DETECTION_RANGE 460.0f
#define BOSS_MAX_HEALTH 200
#define BOSS_HURT_TIME 0.25f

// Data-driven skill table. Add a row, bump BOSS_SKILL_COUNT, done.
static const BossSkillDef BOSS_SKILLS[] = {
    { "Slash",       2.0f, 110.0f, 15, 0.30f, 0.15f, 0.30f, 5, 0.12f },
    { "Heavy Smash",  3.5f, 150.0f, 25, 0.50f, 0.25f, 0.55f, 6, 0.10f },
};
#define BOSS_SKILL_COUNT (int)(sizeof(BOSS_SKILLS) / sizeof(BOSS_SKILLS[0]))

void Boss_Init(Boss *boss, float x, float y, SDL_Texture *texture, float mapPixelWidth) {
    boss->x = x;
    boss->y = y;
    boss->velocityX = 0.0f;
    boss->velocityY = 0.0f;
    boss->health = BOSS_MAX_HEALTH;
    boss->maxHealth = BOSS_MAX_HEALTH;
    boss->state = BOSS_IDLE;
    boss->texture = texture;

    boss->skills = BOSS_SKILLS;
    boss->skillCount = BOSS_SKILL_COUNT;
    boss->currentSkill = -1;
    boss->skillTimer = 0.0f;
    boss->hitboxSpawnedThisAttack = false;
    memset(boss->skillCooldowns, 0, sizeof(boss->skillCooldowns));

    Animation_Init(&boss->idleAnim, 4, 0.2f);
    Animation_Init(&boss->skillAnim, 5, 0.12f);

    // Patrol a band centered on the spawn point, clamped to the map
    float halfRange = 160.0f;
    boss->patrolAnchorX = x - halfRange;
    boss->patrolRangeX = halfRange * 2.0f;
    if (boss->patrolAnchorX < 0) boss->patrolAnchorX = 0;
    if (mapPixelWidth > 0 && boss->patrolAnchorX + boss->patrolRangeX + BOSS_WIDTH > mapPixelWidth) {
        boss->patrolAnchorX = mapPixelWidth - BOSS_WIDTH - boss->patrolRangeX;
        if (boss->patrolAnchorX < 0) boss->patrolAnchorX = 0;
    }
}

Rect Boss_GetBounds(const Boss *boss) {
    Rect bounds = { boss->x, boss->y, BOSS_WIDTH, BOSS_HEIGHT };
    return bounds;
}

static Rect SkillRange(const Boss *boss, const BossSkillDef *skill) {
    Rect range;
    range.width = BOSS_WIDTH + (skill->range * 2.0f);
    range.height = BOSS_HEIGHT;
    range.x = boss->x - skill->range;
    range.y = boss->y;
    return range;
}

// Picks the first skill that's off cooldown, cycling so attacks vary.
static int ChooseReadySkill(Boss *boss) {
    for (int offset = 0; offset < boss->skillCount; offset++) {
        int idx = (boss->currentSkill + 1 + offset) % boss->skillCount;
        if (boss->skillCooldowns[idx] <= 0.0f) {
            return idx;
        }
    }
    return -1;
}

void Boss_Update(Boss *boss, Player *player, float dt, const Map *gameMap, HitboxList *hitboxes) {
    if (boss->state == BOSS_DEAD) {
        return;
    }

    for (int i = 0; i < boss->skillCount; i++) {
        if (boss->skillCooldowns[i] > 0.0f) boss->skillCooldowns[i] -= dt;
    }

    float dx = player->x - boss->x;
    float dy = player->y - boss->y;
    float distance = sqrtf(dx * dx + dy * dy);
    bool facingRight = dx >= 0.0f;

    switch (boss->state) {
        case BOSS_IDLE: {
            if (distance < BOSS_DETECTION_RANGE) {
                const BossSkillDef *def = (boss->currentSkill >= 0) ? &boss->skills[boss->currentSkill] : NULL;
                float engageRange = def ? def->range : 120.0f;

                if (distance > engageRange) {
                    boss->velocityX = facingRight ? BOSS_SPEED : -BOSS_SPEED;
                } else {
                    boss->velocityX = 0.0f;
                    int next = ChooseReadySkill(boss);
                    if (next != -1) {
                        boss->currentSkill = next;
                        boss->state = BOSS_WINDUP;
                        boss->skillTimer = boss->skills[next].windupTime;
                        boss->hitboxSpawnedThisAttack = false;
                        Animation_Init(&boss->skillAnim, boss->skills[next].animFrames, boss->skills[next].frameDuration);
                    }
                }
            } else {
                // Patrol back and forth across the anchored range
                float left = boss->patrolAnchorX;
                float right = boss->patrolAnchorX + boss->patrolRangeX;
                if (boss->x <= left) boss->velocityX = BOSS_SPEED * 0.5f;
                else if (boss->x + BOSS_WIDTH >= right) boss->velocityX = -BOSS_SPEED * 0.5f;
                else if (boss->velocityX == 0.0f) boss->velocityX = BOSS_SPEED * 0.5f;
            }
            Animation_Update(&boss->idleAnim, dt);
            break;
        }

        case BOSS_WINDUP: {
            boss->velocityX = 0.0f;
            Animation_Update(&boss->skillAnim, dt);
            boss->skillTimer -= dt;
            if (boss->skillTimer <= 0.0f) {
                const BossSkillDef *def = &boss->skills[boss->currentSkill];
                boss->state = BOSS_ACTIVE;
                boss->skillTimer = def->activeTime;
            }
            break;
        }

        case BOSS_ACTIVE: {
            boss->velocityX = 0.0f;
            Animation_Update(&boss->skillAnim, dt);

            const BossSkillDef *def = &boss->skills[boss->currentSkill];
            if (!boss->hitboxSpawnedThisAttack && hitboxes) {
                Rect range = SkillRange(boss, def);
                float kbX = facingRight ? 320.0f : -320.0f;
                HitboxList_Spawn(hitboxes, range, def->damage, HITBOX_SOURCE_BOSS, 0, kbX, -140.0f, 0.1f);
                boss->hitboxSpawnedThisAttack = true;
            }

            boss->skillTimer -= dt;
            if (boss->skillTimer <= 0.0f) {
                boss->skillCooldowns[boss->currentSkill] = def->cooldown;
                boss->state = BOSS_RECOVER;
                boss->skillTimer = def->recoverTime;
            }
            break;
        }

        case BOSS_RECOVER: {
            boss->velocityX = 0.0f;
            Animation_Update(&boss->idleAnim, dt);
            boss->skillTimer -= dt;
            if (boss->skillTimer <= 0.0f) {
                boss->state = BOSS_IDLE;
            }
            break;
        }

        case BOSS_HURT: {
            boss->skillTimer -= dt;
            if (boss->skillTimer <= 0.0f) {
                boss->state = BOSS_IDLE;
            }
            break;
        }

        case BOSS_DEAD:
            break;
    }

    Body body = { boss->x, boss->y, BOSS_WIDTH, BOSS_HEIGHT,
                  boss->velocityX, boss->velocityY, false };
    Physics_Step(&body, gameMap, dt);
    boss->x = body.x;
    boss->y = body.y;
    boss->velocityX = body.vx;
    boss->velocityY = body.vy;

    if (boss->health <= 0) {
        boss->state = BOSS_DEAD;
    }
}

void Boss_TakeDamage(Boss *boss, int damage, float knockbackX, float knockbackY) {
    if (boss->state == BOSS_DEAD) return;

    boss->health -= damage;
    if (boss->health < 0) boss->health = 0;

    if (boss->health <= 0) {
        boss->state = BOSS_DEAD;
        return;
    }

    // Boss is only briefly stunned -- don't interrupt an active attack's
    // windup/active phase, or it'd feel unfair to be perma-stunlocked.
    if (boss->state == BOSS_IDLE || boss->state == BOSS_RECOVER) {
        boss->state = BOSS_HURT;
        boss->skillTimer = BOSS_HURT_TIME;
        boss->velocityX = knockbackX * 0.3f; // bosses resist knockback more than regular enemies
        boss->velocityY = knockbackY * 0.3f;
    }
}

int Boss_IsAlive(Boss *boss) {
    return boss->health > 0;
}

void Boss_Render(Boss *boss, SDL_Renderer *renderer, const Camera *camera) {
    if (boss->state == BOSS_DEAD) return;

    SDL_FRect worldRect = { boss->x, boss->y, BOSS_WIDTH, BOSS_HEIGHT };
    SDL_FRect dst = camera ? Camera_ToScreen(camera, worldRect) : worldRect;

    if (boss->texture) {
        int frameWidth = 64;
        int frameHeight = 64;
        Animation *anim = (boss->state == BOSS_WINDUP || boss->state == BOSS_ACTIVE)
                               ? &boss->skillAnim : &boss->idleAnim;
        SDL_FRect src = {
            (float)(anim->currentFrame * frameWidth), 0.0f,
            (float)frameWidth, (float)frameHeight
        };
        SDL_RenderTextureRotated(renderer, boss->texture, &src, &dst, 0, NULL, SDL_FLIP_NONE);
    } else {
        switch (boss->state) {
            case BOSS_IDLE:    SDL_SetRenderDrawColor(renderer, 150, 0, 200, 255); break;
            case BOSS_WINDUP:  SDL_SetRenderDrawColor(renderer, 255, 220, 0, 255); break; // telegraph flash
            case BOSS_ACTIVE:  SDL_SetRenderDrawColor(renderer, 255, 0, 200, 255); break;
            case BOSS_RECOVER: SDL_SetRenderDrawColor(renderer, 120, 60, 160, 255); break;
            case BOSS_HURT:    SDL_SetRenderDrawColor(renderer, 200, 50, 255, 255); break;
            default:           SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255); break;
        }
        SDL_RenderFillRect(renderer, &dst);

        SDL_FRect border = { dst.x - 1, dst.y - 1, BOSS_WIDTH + 2, BOSS_HEIGHT + 2 };
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_RenderRect(renderer, &border);
    }

    SDL_FRect healthBarBg = { dst.x, dst.y - 8, BOSS_WIDTH, 5 };
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &healthBarBg);

    float healthPercent = (float)boss->health / (float)boss->maxHealth;
    SDL_FRect healthBar = { dst.x, dst.y - 8, BOSS_WIDTH * healthPercent, 5 };
    SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255);
    SDL_RenderFillRect(renderer, &healthBar);
}
