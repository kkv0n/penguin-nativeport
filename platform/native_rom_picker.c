// First-run ROM installer for Android. Runs before Platform_Init when asset
// validation fails: shows the system file picker (SAF - grants access to just
// the chosen file, no storage permissions), then streams the content:// URI
// through SDL_IOStream (plain fopen cannot open content URIs) into
// <assets>/ctr-u.bin, the disc-image asset source the game boots from.
// Runs on the SDL_main thread with the game thread not spawned yet, so the
// wait loop below owns the event pump.

#ifdef __ANDROID__

#include "platform/native_rom_picker.h"

#include "platform/native_assets.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <macros.h>

#define NATIVE_ROM_PICKER_STATE_WAITING  0
#define NATIVE_ROM_PICKER_STATE_PICKED   1
#define NATIVE_ROM_PICKER_STATE_CANCELED 2

#define NATIVE_ROM_PICKER_COPY_CHUNK     (1024 * 1024)
#define NATIVE_ROM_PICKER_LOG_STEP       (32 * 1024 * 1024)

global_variable SDL_AtomicInt s_romPickerState;
global_variable char s_romPickerUri[2048];

internal void SDLCALL NativeRomPicker_DialogCallback(void *userdata, const char *const *filelist, int filter)
{
	(void)userdata;
	(void)filter;

	if ((filelist != NULL) && (filelist[0] != NULL))
	{
		SDL_strlcpy(s_romPickerUri, filelist[0], sizeof(s_romPickerUri));
		SDL_SetAtomicInt(&s_romPickerState, NATIVE_ROM_PICKER_STATE_PICKED);
	}
	else
	{
		// canceled, or filelist == NULL on error (SDL_GetError has details)
		SDL_SetAtomicInt(&s_romPickerState, NATIVE_ROM_PICKER_STATE_CANCELED);
	}
}

internal int NativeRomPicker_CopyUriToFile(const char *uri, const char *dstPath)
{
	SDL_IOStream *src;
	FILE *dst;
	u8 *chunk;
	size_t total = 0;
	size_t nextLog = NATIVE_ROM_PICKER_LOG_STEP;
	int ok = 0;

	src = SDL_IOFromFile(uri, "rb");
	if (src == NULL)
	{
		fprintf(stderr, "[CTR Native] ROM picker: cannot open '%s': %s\n", uri, SDL_GetError());
		return 0;
	}

	dst = fopen(dstPath, "wb");
	if (dst == NULL)
	{
		fprintf(stderr, "[CTR Native] ROM picker: cannot create '%s' (%s)\n", dstPath, strerror(errno));
		SDL_CloseIO(src);
		return 0;
	}

	chunk = (u8 *)malloc(NATIVE_ROM_PICKER_COPY_CHUNK);
	if (chunk != NULL)
	{
		for (;;)
		{
			const size_t got = SDL_ReadIO(src, chunk, NATIVE_ROM_PICKER_COPY_CHUNK);

			if (got == 0)
			{
				ok = (SDL_GetIOStatus(src) == SDL_IO_STATUS_EOF);
				if (!ok)
				{
					fprintf(stderr, "[CTR Native] ROM picker: read failed at %u MB: %s\n", (unsigned)(total >> 20), SDL_GetError());
				}
				break;
			}

			if (fwrite(chunk, 1, got, dst) != got)
			{
				fprintf(stderr, "[CTR Native] ROM picker: write failed at %u MB (disk full?)\n", (unsigned)(total >> 20));
				break;
			}

			total += got;
			if (total >= nextLog)
			{
				printf("[CTR Native] ROM picker: copied %u MB...\n", (unsigned)(total >> 20));
				fflush(stdout);
				nextLog += NATIVE_ROM_PICKER_LOG_STEP;
			}

			// keep the SDL event queue drained during the long copy
			SDL_PumpEvents();
		}
		free(chunk);
	}

	if (fclose(dst) != 0)
	{
		ok = 0;
	}
	SDL_CloseIO(src);

	if (!ok || (total == 0))
	{
		remove(dstPath); // never leave a truncated image behind
		return 0;
	}

	printf("[CTR Native] ROM picker: installed %u MB as %s\n", (unsigned)(total >> 20), dstPath);
	fflush(stdout);
	return 1;
}

int NativeRomPicker_InstallRomInteractive(void)
{
	local_persist const SDL_DialogFileFilter filters[] = {{"PS1 disc image (.bin)", "bin"}};
	char dstPath[1024];

	// refcounted; Platform_Init does its own SDL_Init later
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "[CTR Native] ROM picker: SDL init failed: %s\n", SDL_GetError());
		return 0;
	}

	printf("[CTR Native] No game data found - opening the system file picker...\n");
	fflush(stdout);

	SDL_SetAtomicInt(&s_romPickerState, NATIVE_ROM_PICKER_STATE_WAITING);
	s_romPickerUri[0] = 0;

	SDL_ShowOpenFileDialog(NativeRomPicker_DialogCallback, NULL, NULL, filters, (int)(sizeof(filters) / sizeof(filters[0])), NULL, false);

	while (SDL_GetAtomicInt(&s_romPickerState) == NATIVE_ROM_PICKER_STATE_WAITING)
	{
		SDL_PumpEvents();
		SDL_Delay(50);
	}

	if (SDL_GetAtomicInt(&s_romPickerState) != NATIVE_ROM_PICKER_STATE_PICKED)
	{
		fprintf(stderr, "[CTR Native] ROM picker: no file selected\n");
		return 0;
	}

	// the assets dir may not exist yet on a fresh install
	if ((mkdir(NativeAssets_GetAssetDir(), 0770) != 0) && (errno != EEXIST))
	{
		fprintf(stderr, "[CTR Native] ROM picker: cannot create assets dir '%s' (%s)\n", NativeAssets_GetAssetDir(), strerror(errno));
		return 0;
	}

	if (snprintf(dstPath, sizeof(dstPath), "%s/ctr-u.bin", NativeAssets_GetAssetDir()) >= (int)sizeof(dstPath))
	{
		return 0;
	}

	return NativeRomPicker_CopyUriToFile(s_romPickerUri, dstPath);
}

#endif // __ANDROID__
