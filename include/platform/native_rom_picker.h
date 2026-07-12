#ifndef NATIVE_ROM_PICKER_H
#define NATIVE_ROM_PICKER_H

// First-run ROM installer (Android only - every call site is compile-time
// gated with __ANDROID__). When no game data is found, the system file
// picker (Storage Access Framework - no storage permissions needed) lets the
// user choose a .bin PS1 disc image, which is then copied into the app's
// assets dir as ctr-u.bin, the disc-image asset source the game boots from.

#ifdef __ANDROID__

// Returns 1 when a ROM was picked and copied successfully, 0 on cancel/error.
int NativeRomPicker_InstallRomInteractive(void);

#endif // __ANDROID__

#endif
