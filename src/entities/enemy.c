#include "enemy.h"
#include "physics.h"
#include <math.h>

#define ENEMY_WIDTH 48.0f
#define ENEMY_HEIGHT 72.0f
#define ENEMY_SPEED 130.0f
#define ENEMY_ATTACK_RANGE 55.0f
#define ENEMY_ATTACK_DAMAGE 10
#define ENEMY_ATTACK_COOLDOWN 1.2f
#define ENEMY_ATTACK_ACTIVE_FRAME 2
#define ENEMY_DETECTION_RANGE 300.0f

// The sprite frame is 48x48 but the character only fills part of it; draw it at
// 2x (like the player) so the enemy reads at a comparable on-screen size. The
// physics/collision body stays ENEMY_WIDTH x ENEMY_HEIGHT; the sprite is
// centered horizontally over it and bottom-aligned (feet on the ground).
#define ENEMY_RENDER_WIDTH  96.0f
#define ENEMY_RENDER_HEIGHT 96.0f
#define ENEMY_MAX_HEALTH 30
#define ENEMY_HURT_TIME 0.2f

void Enemy_Init(Enemy *enemy, float x, float y, SDL_Texture *texture) {
    enemy->x = x;
    enemy->y = y;
    enemy->velocityX = 0.0f;
    enemy->velocityY = 0.0f;
    enemy->health = ENEMY_MAX_HEALTH;
    enemy->maxHealth = ENEMY_MAX_HEALTH;
    enemy->state = ENEMY_IDLE;
    enemy->flipHorizontal = false;
    enemy->texture = texture;
    enemy->attackCooldownTimer = 0.0f;
    enemy->attackHitboxSpawned = false;
    enemy->hurtTimer = 0.0f;

    Animation_Init(&enemy->idleAnim, 3, 0.15f);
    Animation_Init(&enemy->runAnim, 8, 0.1f);
    Animation_Init(&enemy->attackAnim, 8, 0.1f);
}

Rect Enemy_GetAttackRange(const Enemy *enemy) {
    Rect range;
    range.x = enemy->x - ENEMY_ATTACK_RANGE;
    range.y = enemy->y;
    range.width = ENEMY_WIDTH + (ENEMY_ATTACK_RANGE * 2);
    range.height = ENEMY_HEIGHT;
    return range;
}

Rect Enemy_GetBounds(const Enemy *enemy) {
    Rect bounds = { enemy->x, enemy->y, ENEMY_WIDTH, ENEMY_HEIGHT };
    return bounds;
}

void Enemy_Update(Enemy *enemy, Player *player, int enemyIndex, float dt, const Map *gameMap, HitboxList *hitboxes) {
    if (enemy->state == ENEMY_DEAD) {
        return;
    }

    if (enemy->hurtTimer > 0.0f) {
        enemy->hurtTimer -= dt;
        if (enemy->hurtTimer <= 0.0f && enemy->state == ENEMY_HURT) {
            enemy->state = ENEMY_IDLE;
        }
    }
    if (enemy->attackCooldownTimer > 0.0f) {
        enemy->attackCooldownTimer -= dt;
    }

    bool stunned = (enemy->state == ENEMY_HURT);

    if (!stunned) {
        float dx = player->x - enemy->x;
        float dy = player->y - enemy->y;
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance < ENEMY_DETECTION_RANGE) {
            // The enemy sprite's neutral facing is LEFT (opposite the player's,
            // whose unflipped sprite faces right), so flip it only when the
            // player is to the RIGHT. Turn to face the player.
            enemy->flipHorizontal = (dx > 0.0f);
            if (distance > ENEMY_ATTACK_RANGE) {
                enemy->velocityX = (dx > 0) ? ENEMY_SPEED : -ENEMY_SPEED;
                if (enemy->state != ENEMY_ATTACK) enemy->state = ENEMY_RUN;
            } else {
                enemy->velocityX = 0;
                if (enemy->attackCooldownTimer <= 0.0f && enemy->state != ENEMY_ATTACK) {
                    enemy->state = ENEMY_ATTACK;
                    enemy->attackCooldownTimer = ENEMY_ATTACK_COOLDOWN;
                    enemy->attackHitboxSpawned = false;
                    Animation_Init(&enemy->attackAnim, 6, 0.1f);
                } else if (enemy->state != ENEMY_ATTACK) {
                    enemy->state = ENEMY_IDLE;
                }
            }
        } else {
            enemy->velocityX = 0;
            if (enemy->state != ENEMY_DEAD && enemy->state != ENEMY_ATTACK) {
                enemy->state = ENEMY_IDLE;
            }
        }
    }

    Body body = { enemy->x, enemy->y, ENEMY_WIDTH, ENEMY_HEIGHT,
                  enemy->velocityX, enemy->velocityY, false };
    Physics_Step(&body, gameMap, dt);
    enemy->x = body.x;
    enemy->y = body.y;
    enemy->velocityX = body.vx;
    enemy->velocityY = body.vy;

    switch (enemy->state) {
        case ENEMY_IDLE:
            Animation_Update(&enemy->idleAnim, dt);
            break;
        case ENEMY_RUN:
            Animation_Update(&enemy->runAnim, dt);
            break;
        case ENEMY_ATTACK: {
            Animation_Update(&enemy->attackAnim, dt);

            if (!enemy->attackHitboxSpawned && enemy->attackAnim.currentFrame >= ENEMY_ATTACK_ACTIVE_FRAME) {
                Rect range = Enemy_GetAttackRange(enemy);
                float kbX = (player->x >= enemy->x) ? 180.0f : -180.0f;
                if (hitboxes) {
                    HitboxList_Spawn(hitboxes, range, ENEMY_ATTACK_DAMAGE,
                                      HITBOX_SOURCE_ENEMY, enemyIndex, kbX, -60.0f, 0.08f);
                }
                enemy->attackHitboxSpawned = true;
            }

            if (enemy->attackAnim.currentFrame >= enemy->attackAnim.frameCount - 1 &&
                enemy->attackAnim.timer > enemy->attackAnim.frameDuration * 0.8f) {
                enemy->state = ENEMY_IDLE;
            }
            break;
        }
        case ENEMY_HURT:
        case ENEMY_DEAD:
            break;
    }

    if (enemy->health <= 0) {
        enemy->state = ENEMY_DEAD;
    }
}

void Enemy_TakeDamage(Enemy *enemy, int damage, float knockbackX, float knockbackY) {
    if (enemy->state == ENEMY_DEAD) return;

    enemy->health -= damage;
    if (enemy->health < 0) enemy->health = 0;

    if (enemy->health <= 0) {
        enemy->state = ENEMY_DEAD;
    } else {
        enemy->state = ENEMY_HURT;
        enemy->hurtTimer = ENEMY_HURT_TIME;
        enemy->velocityX = knockbackX;
        enemy->velocityY = knockbackY;
    }
}

int Enemy_IsAlive(Enemy *enemy) {
    return enemy->health > 0;
}

void Enemy_Render(Enemy *enemy, SDL_Renderer *renderer, const Camera *camera) {
    if (enemy->state == ENEMY_DEAD) return;

    // Physics/collision body (anchors the fallback block + health bar).
    SDL_FRect worldBody = { enemy->x, enemy->y, ENEMY_WIDTH, ENEMY_HEIGHT };
    SDL_FRect bodyDst = camera ? Camera_ToScreen(camera, worldBody) : worldBody;

    // Sprite frame drawn larger than the body: centered horizontally over it and
    // bottom-aligned so the feet line up with the body's bottom (the ground).
    SDL_FRect worldSprite = {
        enemy->x + ENEMY_WIDTH * 0.5f - ENEMY_RENDER_WIDTH * 0.5f,
        enemy->y + ENEMY_HEIGHT - ENEMY_RENDER_HEIGHT,
        ENEMY_RENDER_WIDTH,
        ENEMY_RENDER_HEIGHT
    };
    SDL_FRect dst = camera ? Camera_ToScreen(camera, worldSprite) : worldSprite;

    if (!enemy->texture) {
        SDL_SetRenderDrawColor(renderer, 0, 200, 50, 255);
        SDL_RenderFillRect(renderer, &bodyDst);
    } else {
        int frameWidth = 48;
        int frameHeight = 48;

        Animation *currentAnim = NULL;
        int rowOffset = 0;
        switch (enemy->state) {
            case ENEMY_IDLE:   currentAnim = &enemy->idleAnim;   rowOffset = 0; break;
            case ENEMY_RUN:    currentAnim = &enemy->runAnim;    rowOffset = 1; break;
            case ENEMY_ATTACK: currentAnim = &enemy->attackAnim; rowOffset = 2; break;
            default: currentAnim = &enemy->idleAnim; rowOffset = 0; break;
        }

        SDL_FRect src = {
            (float)(currentAnim->currentFrame * frameWidth),
            (float)(rowOffset * frameHeight),
            (float)frameWidth,
            (float)frameHeight
        };

        if (enemy->state == ENEMY_HURT) {
            SDL_SetTextureColorMod(enemy->texture, 255, 120, 120);
        } else {
            SDL_SetTextureColorMod(enemy->texture, 255, 255, 255);
        }

        SDL_RenderTextureRotated(renderer, enemy->texture, &src, &dst, 0, NULL,
                (enemy->flipHorizontal) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }

    // Health bar over the body's width, just above the character's head.
    float barX = bodyDst.x;
    float barY = dst.y + 4.0f;
    SDL_FRect healthBarBg = { barX, barY, ENEMY_WIDTH, 2 };
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &healthBarBg);

    float healthPercent = (float)enemy->health / (float)enemy->maxHealth;
    SDL_FRect healthBar = { barX, barY, ENEMY_WIDTH * healthPercent, 2 };
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &healthBar);
}
