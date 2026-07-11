#ifndef NATIVE_TOUCH_H
#define NATIVE_TOUCH_H

// On-screen PS1-style touch controls (Android only - every call site is
// compile-time gated with __ANDROID__, other platforms never reference this).
// The overlay is shown by default and hides itself automatically while a
// physical/Bluetooth gamepad is connected.

#ifdef __ANDROID__

#include <macros.h>
#include <SDL3/SDL.h>

// Track finger down/up/motion events (called from the host event drain).
void NativeTouch_HandleFingerEvent(const SDL_Event *event);

// Current PSX pad mask from the touch buttons, active-low (0xffff = nothing
// pressed). Returns 0xffff while a gamepad is connected.
u16 NativeTouch_GetPadMask(void);

// Draw the overlay; called right before the buffer swap so the buttons can
// never leak into the VRAM feedback captures. No-op while a gamepad is
// connected.
void NativeTouch_Render(void);

#endif // __ANDROID__

#endif
