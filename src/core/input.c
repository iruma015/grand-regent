#include "input.h"

// The active gamepad (we drive the game from a single pad -- the first one
// connected). `g_padId` is its instance id, used to match the REMOVED event.
static SDL_Gamepad *g_pad = NULL;
static SDL_JoystickID g_padId = 0;

// Previous-frame snapshot for edge detection, mirroring player.c's prevKeyState.
static bool g_prevBtn[SDL_GAMEPAD_BUTTON_COUNT];
static SDL_MouseButtonFlags g_prevMouse = 0;

// Ignore small stick wobble near center (Sint16 axis range is -32768..32767).
#define STICK_DEADZONE 8000

static void OpenFirstGamepad(void) {
    if (g_pad) return;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (!ids) return;
    for (int i = 0; i < count; i++) {
        SDL_Gamepad *pad = SDL_OpenGamepad(ids[i]);
        if (pad) {
            g_pad = pad;
            g_padId = ids[i];
            SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(pad));
            break;
        }
    }
    SDL_free(ids);
}

void Input_Init(void) {
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        SDL_Log("Gamepad subsystem unavailable (%s); continuing without a controller",
                SDL_GetError());
        return;
    }
    OpenFirstGamepad();
}

void Input_Shutdown(void) {
    if (g_pad) {
        SDL_CloseGamepad(g_pad);
        g_pad = NULL;
        g_padId = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

void Input_HandleEvent(const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            // Adopt a newly-plugged pad only if we don't already have one.
            if (!g_pad) {
                g_pad = SDL_OpenGamepad(event->gdevice.which);
                if (g_pad) {
                    g_padId = event->gdevice.which;
                    SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(g_pad));
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (g_pad && event->gdevice.which == g_padId) {
                SDL_CloseGamepad(g_pad);
                g_pad = NULL;
                g_padId = 0;
                SDL_Log("Gamepad disconnected");
                OpenFirstGamepad(); // fall back to another pad if one is still attached
            }
            break;
        default:
            break;
    }
}

bool Input_HasGamepad(void) {
    return g_pad != NULL;
}

static bool PadDown(SDL_GamepadButton button) {
    return g_pad && SDL_GetGamepadButton(g_pad, button);
}

static bool MouseDown(SDL_MouseButtonFlags mask) {
    return (SDL_GetMouseState(NULL, NULL) & mask) != 0;
}

float Input_MoveX(void) {
    if (!g_pad) return 0.0f;

    float x = 0.0f;
    Sint16 axis = SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTX);
    if (axis > STICK_DEADZONE || axis < -STICK_DEADZONE) {
        x = (float)axis / 32767.0f;
    }
    if (PadDown(SDL_GAMEPAD_BUTTON_DPAD_LEFT))  x -= 1.0f;
    if (PadDown(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) x += 1.0f;

    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    return x;
}

bool Input_JumpHeld(void) {
    return PadDown(SDL_GAMEPAD_BUTTON_SOUTH);
}

// True on the frame `button` transitions from up to down (vs. the last snapshot).
static bool PadPressed(SDL_GamepadButton button) {
    return PadDown(button) && !g_prevBtn[button];
}

static bool MousePressed(SDL_MouseButtonFlags mask) {
    return MouseDown(mask) && !(g_prevMouse & mask);
}

bool Input_AttackPressed(void) {
    return PadPressed(SDL_GAMEPAD_BUTTON_WEST) || MousePressed(SDL_BUTTON_LMASK);
}

bool Input_SkillPressed(int slot) {
    if (slot == 0) {
        return PadPressed(SDL_GAMEPAD_BUTTON_NORTH) ||
               PadPressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ||
               MousePressed(SDL_BUTTON_RMASK);
    }
    if (slot == 1) {
        return PadPressed(SDL_GAMEPAD_BUTTON_EAST) ||
               PadPressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    }
    return false;
}

void Input_Snapshot(void) {
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        g_prevBtn[b] = g_pad && SDL_GetGamepadButton(g_pad, (SDL_GamepadButton)b);
    }
    g_prevMouse = SDL_GetMouseState(NULL, NULL);
}
