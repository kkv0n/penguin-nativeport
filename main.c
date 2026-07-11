#define _CRT_SECURE_NO_WARNINGS
// NOTE(penta3): On Android SDL owns the process entry (SDLActivity calls our
// main() renamed to SDL_main by SDL_main.h); everywhere else we keep the plain
// C main().
#ifndef __ANDROID__
#define SDL_MAIN_HANDLED
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#if __GNUC__
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define _EnterCriticalSection(x)
#define EnterCriticalSection(x)
#define ExitCriticalSection()
#endif

#include "platform/native_assets.h"
#include "platform/native_log.h"
#include "platform/native_memory.h"
#include "platform/native_perf.h"
#include "platform/native_replay_scheduler.h"
#include "platform/native_savestate.h"

#ifndef __GNUC__
#define __attribute__(x)
#endif

#include <platform.h>

#include "game/game_unity.h"

#include "game/zGlobal_RDATA.c"
#include "game/zGlobal_DATA.c"
#include "game/zGlobal_SDATA.c"

#undef RECT

#include "platform/native_disc_image.c"
#include "platform/native_assets.c"
#include "platform/native_audio.c"
#include "platform/native_memory.c"
#include "platform/native_checkpoint.c"
#include "platform/native_checkpoint_file.c"
#include "platform/native_cd.c"
#include "platform/native_gpu_links.c"
#include "platform/native_gpu.c"
#include "platform/native_gte_core.c"
#include "platform/native_glad.c"
#include "platform/native_input.c"
#include "platform/native_inline_c.c"
#include "platform/native_libapi.c"
#include "platform/native_libetc.c"
#include "platform/native_libgte.c"
#include "platform/native_libgpu.c"
#include "platform/native_libpad.c"
#include "platform/native_libspu.c"
#include "platform/native_log.c"
#include "platform/native_memcard.c"
#include "platform/native_memcard_adapter.c"
#include "platform/native_perf.c"
#include "platform/native_platform.c"
#include "platform/native_replay_scheduler.c"
#include "platform/native_renderer.c"
#include "platform/native_savestate.c"
#include "platform/native_state.c"
#include "platform/native_str.c"
#include "platform/native_touch.c"

#ifndef CC
#if __GNUC__
#if _WIN32
#ifndef __clang__
#define CC "MINGW-GCC"
#else
#define CC "MINGW-CLANG"
#endif
#else
#ifndef __clang__
#define CC "GCC"
#else
#define CC "CLANG"
#endif
#endif
#elif defined(_MSC_VER)
#define CC "MSVC"
#else
#define CC "Unknown"
#endif
#endif

#ifndef CTR_NATIVE_VERSION
#define CTR_NATIVE_VERSION "0.0.0-dev"
#endif

#ifndef CTR_NATIVE_BUILD_ID
#define CTR_NATIVE_BUILD_ID "unknown"
#endif

static int NativeConsole_ShouldPauseOnError(void)
{
#if defined(_WIN32)
	DWORD consoleProcesses[2];
	DWORD consoleProcessCount;

	if (GetConsoleWindow() == NULL)
		return 0;

	consoleProcessCount = GetConsoleProcessList(consoleProcesses, (DWORD)(sizeof(consoleProcesses) / sizeof(consoleProcesses[0])));
	return (consoleProcessCount == 1) && (consoleProcesses[0] == GetCurrentProcessId());
#else
	return 0;
#endif
}

static s32 NativeConsole_Return(const u32 result)
{
	if ((result != 0) && NativeConsole_ShouldPauseOnError())
	{
		fflush(stdout);
		fflush(stderr);
		fprintf(stderr, "\n[CTR Native] Press Enter to close this window...");
		fflush(stderr);

		while (getchar() != '\n' && !feof(stdin))
		{
		}
	}

	return (s32)result;
}

// TODO(aalhendi): just make an argparser?
static int NativeArg_IsVersion(const char *arg)
{
	return (arg != NULL) && ((strcmp(arg, "--version") == 0) || (strcmp(arg, "-v") == 0));
}

// NOTE(penta3): Standard engine thread split. The OS delivers window messages
// to the thread that created the window (the main thread), and Windows parks
// that thread inside a modal loop while the title bar is clicked/dragged. So
// the game (simulation + GL render) runs on its own thread, and the main
// thread does nothing but pump events: the game can never freeze from window
// interaction. Cross-platform - pumping on the main thread is what SDL
// requires on every OS (macOS mandates it), and the game thread drains the
// queue with thread-safe SDL_PeepEvents (Platform_PollHostEvents).
// game thread state: 0 = running, 1 = CTR_Main returned, 2 = quit requested
// (window close/QUIT event - the game thread parks and the MAIN thread exits)
#define NATIVE_GAME_THREAD_RUNNING  0
#define NATIVE_GAME_THREAD_RETURNED 1
#define NATIVE_GAME_THREAD_QUIT     2
static SDL_AtomicInt s_gameThreadState;
static int s_gameThreadActive = 0;

// NOTE(penta3): Called by the platform layer on the QUIT / window-close event
// (game thread). exit() must NOT run on the game thread: the atexit teardown
// destroys the window, which the OS only allows from the thread that created
// it (the main thread) - doing it cross-thread deadlocked against the stopped
// pump. Instead the game thread releases the GL context and parks forever;
// the main thread picks the state change up, re-acquires GL and runs the
// normal exit(0) teardown on the correct thread. Never returns.
void NativeMain_RequestQuit(void)
{
	if (!s_gameThreadActive)
	{
		// single-threaded fallback: this IS the main thread
		exit(0);
	}

	NativeRenderer_ReleaseGLContext();
	SDL_SetAtomicInt(&s_gameThreadState, NATIVE_GAME_THREAD_QUIT);

	for (;;)
	{
		SDL_Delay(100000); // parked; exit() on the main thread ends the process
	}
}

static int SDLCALL NativeMain_GameThread(void *userdata)
{
	int result;

	(void)userdata;

	// GL context handoff: released by the main thread after init, owned by
	// this thread for the game's lifetime.
	NativeRenderer_AcquireGLContext();

	result = CTR_Main();

	NativeRenderer_ReleaseGLContext();
	SDL_SetAtomicInt(&s_gameThreadState, NATIVE_GAME_THREAD_RETURNED);
	return result;
}

static int NativeMain_RunGame(void)
{
	SDL_Thread *gameThread;
	int state;
	int result = 1;

	NativeRenderer_ReleaseGLContext();

	SDL_SetAtomicInt(&s_gameThreadState, NATIVE_GAME_THREAD_RUNNING);
	gameThread = SDL_CreateThread(NativeMain_GameThread, "CTR Game", NULL);

	if (gameThread == NULL)
	{
		// no thread available: run single-threaded like before (window will
		// freeze during title-bar drags, but the game works)
		fprintf(stderr, "[CTR Native] Failed to create game thread (%s), running single-threaded.\n", SDL_GetError());
		NativeRenderer_AcquireGLContext();
		return CTR_Main();
	}

	s_gameThreadActive = 1;

	for (;;)
	{
		state = SDL_GetAtomicInt(&s_gameThreadState);
		if (state != NATIVE_GAME_THREAD_RUNNING)
		{
			break;
		}
		SDL_PumpEvents();
		SDL_Delay(4);
	}

	// teardown always happens on this (main) thread, GL context re-acquired
	NativeRenderer_AcquireGLContext();

	if (state == NATIVE_GAME_THREAD_QUIT)
	{
		exit(0); // atexit(Platform_Shutdown) runs here, on the window's thread
	}

	SDL_WaitThread(gameThread, &result);
	return result;
}


int main(int argc, char *argv[])
{
	for (int argIndex = 1; argIndex < argc; argIndex++)
	{
		if (NativeArg_IsVersion(argv[argIndex]))
		{
			printf("CTR Native %s (%s)\n", CTR_NATIVE_VERSION, CTR_NATIVE_BUILD_ID);
			return 0;
		}
	}

	printf("[CTR Native] Starting...\n");
	fflush(stdout);

	const char *sdlBasePath = SDL_GetBasePath();
#ifdef __ANDROID__
	// NOTE(penta3): No exe directory on Android - game data (BIGFILE.BIG etc.)
	// lives in the app's external files dir, user-visible at
	// Android/data/<package>/files/ so assets can be copied over USB.
	sdlBasePath = SDL_GetAndroidExternalStoragePath();
#endif
	printf("[CTR Native] SDL base path: %s\n", sdlBasePath ? sdlBasePath : "(null)");
	fflush(stdout);

	if (!NativeAssets_Init(sdlBasePath))
	{
		fprintf(stderr, "[CTR Native] Failed to initialize asset paths.\n");
		return NativeConsole_Return(1);
	}

	printf("[CTR Native] Version: %s (%s)\n", CTR_NATIVE_VERSION, CTR_NATIVE_BUILD_ID);
	printf("[CTR Native] Built with: " CC "\n");
	printf("[CTR Native] Base: %s\n", NativeAssets_GetBaseDir());
	printf("[CTR Native] Assets: %s\n", NativeAssets_GetAssetDir());
	fflush(stdout);

	if (chdir(NativeAssets_GetBaseDir()) != 0)
	{
		fprintf(stderr, "[CTR Native] Failed to enter base directory: %s\n", NativeAssets_GetBaseDir());
		return NativeConsole_Return(1);
	}

	if (!NativeAssets_Validate())
	{
		return NativeConsole_Return(1);
	}

#if defined(CTR_INTERNAL)
	if (NativeReplayScheduler_PrepareReportFromArgs(argc, argv) != 0)
	{
		return NativeConsole_Return(1);
	}
#endif

#ifdef USE_16BY9
	printf("[CTR Native] Widescreen\n");
	Platform_Init("Crash Team Racing", 1280, 720);
#else
	printf("[CTR Native] 4:3\n");
	Platform_Init("Crash Team Racing", 800, 600);
#endif

#if defined(CTR_INTERNAL)
	if (NativePerf_ConfigureFromArgs(argc, argv) != 0)
	{
		Platform_LogFlush();
		Platform_Shutdown();
		return NativeConsole_Return(1);
	}
#endif

	Platform_InitScratchpad();
	Platform_RepairResidentPointers(0);

#if defined(CTR_INTERNAL)
	if (NativeReplayScheduler_ConfigureFromArgs(argc, argv) != 0)
	{
		Platform_LogFlush();
		Platform_Shutdown();
		return NativeConsole_Return(1);
	}
#else
	(void)argc;
	(void)argv;
#endif

	const int result = NativeMain_RunGame();

	Platform_Shutdown();
	return NativeConsole_Return(result);
}
