#ifndef INPUT_H
#define INPUT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

// Gamepad + mouse input.
//
// Like core/audio.c, this is a deliberate *global* module: gameplay reads input
// straight from the player/combat code, so threading an input manager through
// every Update() signature isn't worth it. Any file can call the query helpers
// below after #include "core/input.h".
//
// The keyboard is still read directly via SDL_GetKeyboardState in player.c; this
// module adds the gamepad (a hot-pluggable handle it owns) and the mouse on top,
// exposing them as the same "held" / "just pressed" queries the keyboard uses.
//
// Edge detection (just-pressed) works exactly like the keyboard's prevKeyState:
// query the live state during the frame, then call Input_Snapshot() once at the
// end of the gameplay update to remember it for next frame.

// Opens the gamepad subsystem and grabs the first connected pad (if any).
// Non-fatal: with no pad, every gamepad query simply reports "not pressed".
void Input_Init(void);
void Input_Shutdown(void);

// Feed gamepad hot-plug events (SDL_EVENT_GAMEPAD_ADDED / _REMOVED) here so the
// active pad handle stays in sync as controllers come and go.
void Input_HandleEvent(const SDL_Event *event);

// Remembers this frame's gamepad/mouse button state for next-frame edge
// detection. Call once at the end of the gameplay update (see player.c).
void Input_Snapshot(void);

bool Input_HasGamepad(void);

// --- Gameplay queries (gamepad + mouse) -------------------------------------
// Horizontal move intent from the left stick + D-pad, in [-1, 1] (deadzoned).
float Input_MoveX(void);
// Jump button (south / "A") currently held -- combined with the keyboard in
// player.c to drive coyote time, buffering, and variable jump height.
bool Input_JumpHeld(void);
// Normal attack just pressed: west ("X") button or left mouse button.
bool Input_AttackPressed(void);
// Skill `slot` just pressed. Slot 0: north/right-shoulder/right-mouse;
// slot 1: east/left-shoulder. Slots map 1:1 onto the player's skills[].
bool Input_SkillPressed(int slot);

#endif // INPUT_H
