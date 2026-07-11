# CTR Native — Android build

Android port of the same source tree: **zero hardcoded code paths**. Every
Android-specific fix is behind a compile-time gate (`#ifdef __ANDROID__` in
platform sources, `if(ANDROID)` in CMake) so no other platform is affected.
The renderer runs the same OpenGL 2.0 feature set it uses on PC, on an
OpenGL ES 2.0 context — same shaders, same single code path. The root
CMakeLists builds everything; this folder only adds the APK packaging
(Gradle + the SDLActivity Java classes from the vendored SDL).

## Requirements

- **Minimum supported Android version: 5.0 Lollipop (API 21, 2014)** — the
  oldest level modern NDK/SDL3 support.
- ABI: `armeabi-v7a` (32-bit, same as the PC port — the decompiled game code
  assumes 32-bit pointers). Does not run on the few recent 64-bit-only
  flagships (Pixel 7+).
- A GPU with OpenGL ES 2.0 (every Android 5.0 device has one).
- Controls: PS1-style touch overlay is shown by default; it hides
  automatically while a physical/Bluetooth gamepad is connected.

## What to install

**Android Studio** (https://developer.android.com/studio) — it bundles the
JDK, Gradle and the SDK manager. After installing it, open *Tools → SDK
Manager* and install:

- *SDK Platforms* tab: **Android 14.0 (API 34)** — this is only the
  compile-time SDK; it does NOT raise the minimum Android version, which is
  fixed at 5.0 by `minSdkVersion` in `app/build.gradle`.
- *SDK Tools* tab (tick **"Show Package Details"** first):
  - **NDK (Side by side) → 27.3.13750724** — this exact version is pinned in
    `app/build.gradle` (`ndkVersion`). Do NOT take the newest NDK: newer ones
    raise the minimum supported Android version and endanger 32-bit support.
    If you install a different 27.x, update `ndkVersion` to match.
  - **CMake 3.22.1**
  - **Android SDK Build-Tools 34+** and **Platform-Tools** (usually preinstalled)

## Building the APK

### From Android Studio

1. *Open* → select this `android/` folder (not the repo root).
2. *Trust Project* if asked, then wait for the Gradle sync (the first run
   downloads Gradle 8.14.3 and dependencies — several minutes).
3. *Build → Build App Bundle(s) / APK(s) → Build APK(s)*.
4. The APK lands at `android/app/build/outputs/apk/debug/app-debug.apk`.

### From the command line

After the first Studio sync (which downloads the Gradle distribution):

```
set JAVA_HOME=<Android Studio dir>\jbr
cd android
%USERPROFILE%\.gradle\wrapper\dists\gradle-8.14.3-bin\<hash>\gradle-8.14.3\bin\gradle assembleDebug
```

### Expected warnings (all harmless)

- `[CXX5202] This app only has 32-bit [armeabi-v7a] native libraries` —
  Google Play requires 64-bit for *store publishing*; this port is sideloaded
  and 32-bit on purpose.
- `SDK processing. This version only understands SDK XML versions up to 3` —
  version skew between Studio's SDK manager and AGP's bundled parser; only a
  warning. If it appears as a blocking *error* instead, your Android Gradle
  Plugin is too old for your Studio: raise the version in `android/build.gradle`
  (plugin `com.android.application`) and, if Gradle complains next, the
  `distributionUrl` in `gradle/wrapper/gradle-wrapper.properties`.
- `2000+ warnings` while compiling the C code — decompiled-code noise
  (typedef redefinitions etc.), same as the PC build.

## Installing and providing the game data

1. Install the APK: `adb install -r app-debug.apk` (or copy it to the device
   and open it).
2. Launch the app **once** (black screen or instant close is expected — it has
   no data yet) and close it. This creates the app's data folder.
3. Copy the game files (the same content as the PC `assets/` folder:
   `BIGFILE.BIG`, `SOUNDS/`, `XA/`, etc.) to:
   `Android/data/com.penta3.ctrnative/files/assets/`
4. Play with the on-screen PS1-style touch controls, or connect a
   Bluetooth/USB gamepad (the touch overlay hides itself automatically while
   a gamepad is connected).

### Copying assets over adb (Android 11+ quirk)

`adb push` into `Android/data` may fail with `secure_mkdirs() failed:
Operation not permitted` — adb can write files there but often cannot CREATE
directories. Pre-create the tree with the shell, then push:

```
adb shell mkdir -p /sdcard/Android/data/com.penta3.ctrnative/files/assets/SOUNDS
adb shell mkdir -p /sdcard/Android/data/com.penta3.ctrnative/files/assets/XA/ENG/EXTRA
adb shell mkdir -p /sdcard/Android/data/com.penta3.ctrnative/files/assets/XA/ENG/GAME
adb shell mkdir -p /sdcard/Android/data/com.penta3.ctrnative/files/assets/XA/MUSIC
adb push --sync assets/. /sdcard/Android/data/com.penta3.ctrnative/files/assets/
```

(`--sync` also lets you re-run the push and only copy what is missing.)

## Debugging

- The game log is written next to the assets:
  `Android/data/com.penta3.ctrnative/files/Crash Team Racing.log`
- Native crashes: `adb logcat -d -s libc:F DEBUG:F` shows the signal and a
  symbolized backtrace into `libmain.so`.
- Launch from the command line:
  `adb shell am start -n com.penta3.ctrnative/org.libsdl.app.SDLActivity`
