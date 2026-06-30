#include "skill.h"

void Skill_Init(Skill *skill, const char *name, SDL_Scancode key, float cooldown,
                 int damage, float range, float knockbackX, float knockbackY) {
    SDL_strlcpy(skill->name, name, sizeof(skill->name));
    skill->key = key;
    skill->cooldown = cooldown;
    skill->cooldownTimer = 0.0f;
    skill->damage = damage;
    skill->range = range;
    skill->knockbackX = knockbackX;
    skill->knockbackY = knockbackY;
}

void Skill_Update(Skill *skill, float dt) {
    if (skill->cooldownTimer > 0.0f) {
        skill->cooldownTimer -= dt;
        if (skill->cooldownTimer < 0.0f) skill->cooldownTimer = 0.0f;
    }
}

bool Skill_IsReady(const Skill *skill) {
    return skill->cooldownTimer <= 0.0f;
}

bool Skill_TryActivate(Skill *skill, bool pressed) {
    if (pressed && Skill_IsReady(skill)) {
        skill->cooldownTimer = skill->cooldown;
        return true;
    }
    return false;
}
