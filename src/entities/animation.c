#include "animation.h"


void Animation_Init(
    Animation *anim,
    int frameCount,
    float frameDuration
) {
    anim->frameCount = frameCount;
    anim->currentFrame = 0;
    anim->timer = 0.0f;
    anim->frameDuration = frameDuration;
}


void Animation_Update(
    Animation *anim,
    float deltaTime
) {
    anim->timer += deltaTime;
    
    // Move to next frame when timer exceeds frameDuration
    if (anim->timer >= anim->frameDuration) {
        anim->timer = 0.0f;
        anim->currentFrame++;
        
        // Loop animation back to first frame
        if (anim->currentFrame >= anim->frameCount) {
            anim->currentFrame = 0;
        }
    }
}

