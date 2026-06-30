#include "player.h"
#include "physics.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <string.h>

#define PLAYER_WIDTH 48.0f
#define PLAYER_HEIGHT 48.0f
#define PLAYER_SPEED 130.0f
#define PLAYER_ATTACK_RANGE 44.0f
#define PLAYER_ATTACK_DAMAGE 15
#define PLAYER_ATTACK_COOLDOWN 0.6f
#define PLAYER_ATTACK_ACTIVE_FRAME 2 // frame index where the swing's hitbox becomes live
#define PLAYER_MAX_HEALTH 100
#define PLAYER_HURT_FLASH_TIME 0.3f

// --- Circular hurtbox (adjust freely) -------------------------------------
// Radius of the player's body hitbox, and where its center sits relative to
// the sprite's top-left. Defaults put it at the center of the 48x48 body.
#define PLAYER_HITBOX_RADIUS    18.0f
#define PLAYER_HITBOX_OFFSET_X  (PLAYER_WIDTH * 0.5f)
#define PLAYER_HITBOX_OFFSET_Y  (PLAYER_HEIGHT * 0.5f)

// Tracks "just pressed" state across frames for skill key edge-detection.
static bool prevKeyState[SDL_SCANCODE_COUNT] = {0};

void Player_Init(Player *player, float x, float y, SDL_Texture *texture) {
    player->x = x;
    player->y = y;
    player->velocityX = 0.0f;
    player->velocityY = 0.0f;
    player->health = PLAYER_MAX_HEALTH;
    player->maxHealth = PLAYER_MAX_HEALTH;
    player->state = PLAYER_IDLE;
    player->texture = texture;
    player->isGrounded = false;
    player->attackCooldownTimer = 0.0f;
    player->attackHitboxSpawned = false;
    player->hurtFlashTimer = 0.0f;
    player->activeSwingSkill = -1;
    player->hitboxRadius = PLAYER_HITBOX_RADIUS;

    Animation_Init(&player->idleAnim, 9, 0.1f);
    Animation_Init(&player->runAnim, 9, 0.1f);
    Animation_Init(&player->attackAnim, 9, 0.06f);

    // Skills are just data -- add a row here to give the player a new ability.
    player->skillCount = 0;
    Skill_Init(&player->skills[player->skillCount++], "Heavy Strike", SDL_SCANCODE_K,
               /*cooldown*/ 2.0f, /*damage*/ 30, /*range*/ 55.0f,
               /*knockbackX*/ 140.0f, /*knockbackY*/ -60.0f);
    Skill_Init(&player->skills[player->skillCount++], "Dash Strike", SDL_SCANCODE_L,
               /*cooldown*/ 3.0f, /*damage*/ 20, /*range*/ 70.0f,
               /*knockbackX*/ 220.0f, /*knockbackY*/ 0.0f);
}

Rect Player_GetAttackRange(const Player *player) {
    Rect range;
    range.width = PLAYER_WIDTH + (PLAYER_ATTACK_RANGE * 2);
    range.height = PLAYER_HEIGHT;
    range.x = player->x - PLAYER_ATTACK_RANGE;
    range.y = player->y;
    return range;
}

static Rect SkillRange(const Player *player, const Skill *skill) {
    Rect range;
    range.width = PLAYER_WIDTH + (skill->range * 2);
    range.height = PLAYER_HEIGHT;
    range.x = player->x - skill->range;
    range.y = player->y;
    return range;
}

Rect Player_GetBounds(const Player *player) {
    Rect bounds = { player->x, player->y, PLAYER_WIDTH, PLAYER_HEIGHT };
    return bounds;
}

Circle Player_GetCircle(const Player *player) {
    Circle c = {
        player->x + PLAYER_HITBOX_OFFSET_X,
        player->y + PLAYER_HITBOX_OFFSET_Y,
        player->hitboxRadius
    };
    return c;
}

static void StartAttack(Player *player, float cooldown) {
    player->state = PLAYER_ATTACK;
    player->attackCooldownTimer = cooldown;
    player->attackHitboxSpawned = false;
    Animation_Init(&player->attackAnim, 6, 0.08f);
}

void Player_Update(Player *player, float dt, const Map *gameMap, HitboxList *hitboxes) {
    if (player->state == PLAYER_DEAD) {
        return;
    }

    const bool *keys = SDL_GetKeyboardState(NULL);

    bool canAct = (player->state != PLAYER_ATTACK && player->state != PLAYER_HURT);
    if (canAct) {
        player->velocityX = 0.0f;
        if (keys[SDL_SCANCODE_A]) {
            player->flipHorizontal = true;
            player->velocityX = -PLAYER_SPEED;
        }
        if (keys[SDL_SCANCODE_D]) {
            player->flipHorizontal = false;
            player->velocityX = PLAYER_SPEED;
        }
        if (keys[SDL_SCANCODE_SPACE] && player->isGrounded) {
            player->velocityY = JUMP_FORCE;
            player->isGrounded = false;
        }
    }

    Body body = { player->x, player->y, PLAYER_WIDTH, PLAYER_HEIGHT,
                  player->velocityX, player->velocityY, false };
    Physics_Step(&body, gameMap, dt);
    player->x = body.x;
    player->y = body.y;
    player->velocityX = body.vx;
    player->velocityY = body.vy;
    player->isGrounded = body.grounded;

    if (player->attackCooldownTimer > 0.0f) {
        player->attackCooldownTimer -= dt;
    }
    if (player->hurtFlashTimer > 0.0f) {
        player->hurtFlashTimer -= dt;
    }

    for (int i = 0; i < player->skillCount; i++) {
        Skill_Update(&player->skills[i], dt);
    }

    // Movement/idle state, unless an attack or hurt reaction is currently playing
    if (player->state != PLAYER_ATTACK && player->state != PLAYER_HURT) {
        if (player->velocityX != 0.0f) {
            if (player->state != PLAYER_RUN) {
                player->state = PLAYER_RUN;
                Animation_Init(&player->runAnim, 6, 0.1f);
            }
        } else {
            if (player->state != PLAYER_IDLE) {
                player->state = PLAYER_IDLE;
                Animation_Init(&player->idleAnim, 6, 0.15f);
            }
        }
    }

    // ---- Combat input: normal attack + skills ----
    int activeSkillIndex = -1; // -1 == normal attack, otherwise index into skills[]

    bool attackPressed = keys[SDL_SCANCODE_J] && !prevKeyState[SDL_SCANCODE_J];
    if (attackPressed && player->attackCooldownTimer <= 0.0f && player->state != PLAYER_ATTACK) {
        StartAttack(player, PLAYER_ATTACK_COOLDOWN);
    }

    for (int i = 0; i < player->skillCount; i++) {
        Skill *skill = &player->skills[i];
        bool pressed = keys[skill->key] && !prevKeyState[skill->key];
        if (pressed && player->state != PLAYER_ATTACK && Skill_TryActivate(skill, true)) {
            // Skill_TryActivate already started the skill's own cooldown;
            // the attack-cooldown here just stops the swing animation from
            // being interrupted by another input on the very next frame.
            StartAttack(player, PLAYER_ATTACK_COOLDOWN);
            activeSkillIndex = i;
        }
    }

    // Remember which skill (if any) is powering the swing currently playing.
    if (activeSkillIndex != -1) {
        player->activeSwingSkill = activeSkillIndex;
    }
    if (player->state != PLAYER_ATTACK) {
        player->activeSwingSkill = -1;
    }

    switch (player->state) {
        case PLAYER_IDLE:
            Animation_Update(&player->idleAnim, dt);
            break;
        case PLAYER_RUN:
            Animation_Update(&player->runAnim, dt);
            break;
        case PLAYER_ATTACK: {
            Animation_Update(&player->attackAnim, dt);

            if (!player->attackHitboxSpawned && player->attackAnim.currentFrame >= PLAYER_ATTACK_ACTIVE_FRAME) {
                Rect range;
                int damage;
                float kbX, kbY;
                if (player->activeSwingSkill >= 0 && player->activeSwingSkill < player->skillCount) {
                    Skill *skill = &player->skills[player->activeSwingSkill];
                    range = SkillRange(player, skill);
                    damage = skill->damage;
                    kbX = player->flipHorizontal ? -skill->knockbackX : skill->knockbackX;
                    kbY = skill->knockbackY;
                } else {
                    range = Player_GetAttackRange(player);
                    damage = PLAYER_ATTACK_DAMAGE;
                    kbX = player->flipHorizontal ? -120.0f : 120.0f;
                    kbY = -40.0f;
                }
                if (hitboxes) {
                    HitboxList_Spawn(hitboxes, range, damage, HITBOX_SOURCE_PLAYER, 0, kbX, kbY, 0.08f);
                }
                player->attackHitboxSpawned = true;
            }

            if (player->attackAnim.currentFrame >= player->attackAnim.frameCount - 1 &&
                player->attackAnim.timer > player->attackAnim.frameDuration * 0.8f) {
                player->state = PLAYER_IDLE;
                Animation_Init(&player->idleAnim, 6, 0.15f);
            }
            break;
        }
        case PLAYER_HURT:
            if (player->hurtFlashTimer <= 0.0f) {
                player->state = PLAYER_IDLE;
            }
            break;
        case PLAYER_DEAD:
            break;
    }

    if (player->health <= 0) {
        player->state = PLAYER_DEAD;
    }

    if (player->x < 0) player->x = 0;
    if (player->x + PLAYER_WIDTH > Map_PixelWidth(gameMap)) {
        player->x = Map_PixelWidth(gameMap) - PLAYER_WIDTH;
    }

    memcpy(prevKeyState, keys, sizeof(bool) * SDL_SCANCODE_COUNT);
}

void Player_TakeDamage(Player *player, int damage, float knockbackX, float knockbackY) {
    if (player->state == PLAYER_DEAD || player->hurtFlashTimer > 0.0f) {
        return; // brief invulnerability window after getting hit
    }

    player->health -= damage;
    if (player->health < 0) player->health = 0;

    if (player->state != PLAYER_DEAD) {
        player->state = PLAYER_HURT;
        player->hurtFlashTimer = PLAYER_HURT_FLASH_TIME;
        player->velocityX = knockbackX;
        player->velocityY = knockbackY;
    }
}

void Player_Heal(Player *player, int amount) {
    player->health += amount;
    if (player->health > player->maxHealth) {
        player->health = player->maxHealth;
    }
}

int Player_IsAlive(Player *player) {
    return player->health > 0;
}

void Player_Render(Player *player, SDL_Renderer *renderer, const Camera *camera) {
    SDL_FRect worldDst = { player->x, player->y, PLAYER_WIDTH, PLAYER_HEIGHT };
    SDL_FRect dst = camera ? Camera_ToScreen(camera, worldDst) : worldDst;

    if (!player->texture) {
        SDL_SetRenderDrawColor(renderer, 0, 200, 50, 255);
        SDL_RenderFillRect(renderer, &dst);
    } else {
        int frameWidth = 64;
        int frameHeight = 64;

        Animation *currentAnim = NULL;
        int rowOffset = 0;

        switch (player->state) {
            case PLAYER_IDLE:   currentAnim = &player->idleAnim;   rowOffset = 0; break;
            case PLAYER_RUN:    currentAnim = &player->runAnim;    rowOffset = 1; break;
            case PLAYER_ATTACK: currentAnim = &player->attackAnim; rowOffset = 2; break;
            default: currentAnim = &player->idleAnim; rowOffset = 0; break;
        }

        SDL_FRect src = {
            (float)(currentAnim->currentFrame * frameWidth),
            (float)(rowOffset * frameHeight),
            (float)frameWidth,
            (float)frameHeight
        };

        // Flash white-ish while the brief hurt invulnerability window is active
        if (player->hurtFlashTimer > 0.0f) {
            SDL_SetTextureColorMod(player->texture, 255, 120, 120);
        } else {
            SDL_SetTextureColorMod(player->texture, 255, 255, 255);
        }

        SDL_RenderTextureRotated(renderer, player->texture, &src, &dst, 0, NULL,
                (player->flipHorizontal) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }

    SDL_FRect healthBarBg = { dst.x, dst.y - 5, PLAYER_WIDTH, 3 };
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &healthBarBg);

    float healthPercent = (float)player->health / (float)player->maxHealth;
    SDL_FRect healthBar = { dst.x, dst.y - 5, PLAYER_WIDTH * healthPercent, 3 };
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &healthBar);
}
