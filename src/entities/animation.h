#ifndef ANIMATION_H
#define ANIMATION_H

typedef struct {
    int frameCount;
    int currentFrame;

    float timer;
    float frameDuration;
} Animation;

void Animation_Init(
    Animation *anim,
    int frameCount,
    float frameDuration
);

void Animation_Update(
    Animation *anim,
    float deltaTime
);

#endif // ANIMATION_H
