#include "hitbox.h"
#include <string.h>

void HitboxList_Clear(HitboxList *list) {
    memset(list, 0, sizeof(HitboxList));
}

void HitboxList_Update(HitboxList *list, float dt) {
    int writeIndex = 0;
    for (int i = 0; i < list->count; i++) {
        Hitbox *hb = &list->boxes[i];
        hb->lifetime -= dt;
        if (hb->lifetime > 0.0f) {
            if (writeIndex != i) {
                list->boxes[writeIndex] = list->boxes[i];
            }
            writeIndex++;
        }
    }
    list->count = writeIndex;
}

int HitboxList_Spawn(HitboxList *list, Rect rect, int damage, HitboxSource source,
                      int sourceId, float knockbackX, float knockbackY, float lifetime) {
    if (list->count >= MAX_HITBOXES) return -1;

    Hitbox *hb = &list->boxes[list->count];
    memset(hb, 0, sizeof(Hitbox));
    hb->rect = rect;
    hb->damage = damage;
    hb->source = source;
    hb->sourceId = sourceId;
    hb->knockbackX = knockbackX;
    hb->knockbackY = knockbackY;
    hb->lifetime = lifetime;

    return list->count++;
}

bool Hitbox_HasHitEnemy(const Hitbox *hb, int enemyIndex) {
    if (enemyIndex < 0 || enemyIndex >= HITBOX_MAX_ENEMY_TARGETS) return true;
    return (hb->hitEnemyMask & (1u << enemyIndex)) != 0;
}

void Hitbox_MarkHitEnemy(Hitbox *hb, int enemyIndex) {
    if (enemyIndex < 0 || enemyIndex >= HITBOX_MAX_ENEMY_TARGETS) return;
    hb->hitEnemyMask |= (1u << enemyIndex);
}
