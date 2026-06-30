#ifndef SKILL_H
#define SKILL_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/*
    Player skill system

    Skills are small, self-contained, table-driven abilities layered on top
    of the normal attack. To add a new one: add a Skill_Init(...) call in
    Player_Init -- nothing else in the engine needs to change. Each skill
    tracks its own cooldown; Player_Update ticks them all and checks for key
    presses.
*/

typedef struct {
    char name[32];
    SDL_Scancode key;

    float cooldown;
    float cooldownTimer;

    int damage;
    float range;       // extends the attack hitbox beyond the normal attack range
    float knockbackX;  // applied to the target on hit, away from the player
    float knockbackY;
} Skill;

#define MAX_PLAYER_SKILLS 4

void Skill_Init(Skill *skill, const char *name, SDL_Scancode key, float cooldown,
                 int damage, float range, float knockbackX, float knockbackY);

void Skill_Update(Skill *skill, float dt);
bool Skill_IsReady(const Skill *skill);

// Returns true if the key was just pressed and the skill is off cooldown.
// `pressed` should already reflect "just pressed this frame", not "held".
bool Skill_TryActivate(Skill *skill, bool pressed);

#endif // SKILL_H
