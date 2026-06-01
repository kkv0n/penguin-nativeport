
#ifdef CTR_NATIVE
#include <stdio.h>
#include <string.h>
#endif

#if 0
typedef struct {
	CdlLOC	pos;		/* file location */
	u32	size;		/* file size */
	char	name[16];	/* file name (body) */
} CdlFILE;
#endif

#ifndef CTR_NATIVE
// Optional PSX debugger/emulator host-file path. Normal PSX ISO builds should
// leave USE_PCDRV undefined and use the real CD APIs instead.
int fileCount = 0;
int fileFD[8] = {0};
int currFD = 0;
#else
// Native PCDRV backs PSX-shaped Cd* calls with extracted files under assets/.
#define PCDRV_NATIVE_MAX_OPEN_FILES 8
#define PCDRV_NATIVE_SECTOR_SIZE    0x800

static FILE *sPcdrvNativeFiles[PCDRV_NATIVE_MAX_OPEN_FILES];
static int sPcdrvNativeFileCount;
static int sPcdrvNativeCurrentFile;

static int PCDRV_NativeNormalizeFilename(char *dst, int dst_count, const char *src)
{
	int i;

	if ((dst_count <= 0) || (src == NULL))
		return 0;

	for (i = 0; src[i] != '\0'; i++)
	{
		char c = src[i];

		if (i + 1 >= dst_count)
			return 0;

		if (c == ';')
			break;

#if !defined(_WIN32)
		if (c == '\\')
			c = '/';
#endif

		dst[i] = c;
	}

	dst[i] = '\0';
	return 1;
}

static const char *PCDRV_NativePathAfterRoot(const char *filename)
{
	if ((filename[0] == '/') || (filename[0] == '\\'))
		return filename + 1;

	return filename;
}

static int PCDRV_NativeOpenHostFile(const char *filename, int *outSize)
{
	char normalized[256];
	char assetPath[256];
	const char *rootless;
	FILE *file;
	int fileIndex;
	long fileSize;

	if (sPcdrvNativeFileCount >= PCDRV_NATIVE_MAX_OPEN_FILES)
		return -1;

	if (PCDRV_NativeNormalizeFilename(normalized, sizeof(normalized), filename) == 0)
		return -1;

	rootless = PCDRV_NativePathAfterRoot(normalized);
	snprintf(assetPath, sizeof(assetPath), "assets/%s", rootless);

	file = fopen(assetPath, "rb");
	if (file == NULL)
		file = fopen(rootless, "rb");

	if (file == NULL)
		return -1;

	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return -1;
	}

	fileSize = ftell(file);
	if (fileSize < 0)
	{
		fclose(file);
		return -1;
	}

	if (fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return -1;
	}

	fileIndex = sPcdrvNativeFileCount++;
	sPcdrvNativeFiles[fileIndex] = file;
	*outSize = (int)fileSize;
	return fileIndex;
}

static int PCDRV_NativeSearchFile(CdlFILE *loc, const char *filename)
{
	char normalized[256];
	const char *rootless;
	int fileIndex;
	int fileSize;
	int encodedPos;

	if ((loc == NULL) || (filename == NULL))
		return 0;

	fileIndex = PCDRV_NativeOpenHostFile(filename, &fileSize);
	if (fileIndex < 0)
		return 0;

	if (PCDRV_NativeNormalizeFilename(normalized, sizeof(normalized), filename) == 0)
		return 0;

	rootless = PCDRV_NativePathAfterRoot(normalized);
	encodedPos = fileIndex << 24;

	memcpy(&loc->pos, &encodedPos, sizeof(encodedPos));
	loc->size = fileSize;
	memset(loc->name, 0, sizeof(loc->name));
	strncpy(loc->name, rootless, sizeof(loc->name) - 1);

	return 1;
}

static int PCDRV_NativePosToInt(const CdlLOC *pos)
{
	int value;

	memcpy(&value, pos, sizeof(value));
	return value;
}

static int PCDRV_NativeIntToPos(int value, CdlLOC *pos)
{
	memcpy(pos, &value, sizeof(value));
	return value;
}

static int PCDRV_NativeSetLoc(const CdlLOC *pos)
{
	int encodedPos;
	int fileIndex;
	int sector;

	encodedPos = PCDRV_NativePosToInt(pos);
	fileIndex = (encodedPos >> 24) & 0xff;
	sector = encodedPos & 0xffffff;

	if ((fileIndex < 0) || (fileIndex >= sPcdrvNativeFileCount) || (sPcdrvNativeFiles[fileIndex] == NULL))
		return 0;

	sPcdrvNativeCurrentFile = fileIndex;

	return fseek(sPcdrvNativeFiles[sPcdrvNativeCurrentFile], sector * PCDRV_NATIVE_SECTOR_SIZE, SEEK_SET) == 0;
}

static int PCDRV_NativeReadSectors(int sectors, void *dst)
{
	size_t byteCount;

	if ((sPcdrvNativeCurrentFile < 0) || (sPcdrvNativeCurrentFile >= sPcdrvNativeFileCount) || (sPcdrvNativeFiles[sPcdrvNativeCurrentFile] == NULL))
		return 0;

	byteCount = (size_t)sectors * PCDRV_NATIVE_SECTOR_SIZE;

	return fread(dst, 1, byteCount, sPcdrvNativeFiles[sPcdrvNativeCurrentFile]) == byteCount;
}
#endif
CdlCB fpReadCallback = 0;

CdlCB pcCdReadCallback(CdlCB x)
{
	CdlCB prev = fpReadCallback;
	fpReadCallback = x;
	return prev;
}

// TODO: Remove when MM_Video is done
int pcCdPosToInt(const CdlLOC *p)
{
#ifdef CTR_NATIVE
	return PCDRV_NativePosToInt(p);
#else
	return *(int *)p;
#endif
}

// TODO: Remove when MM_Video is done
int pcCdIntToPos(int val, const CdlLOC *p)
{
#ifdef CTR_NATIVE
	return PCDRV_NativeIntToPos(val, (CdlLOC *)p);
#else
	*(int *)p = val;
	return val;
#endif
}

CdlFILE *pcCdSearchFile(CdlFILE *loc, const char *filename)
{
#ifdef CTR_NATIVE
	if (PCDRV_NativeSearchFile(loc, filename) == 0)
		return NULL;

	return loc;
#else
	// because this API is STRANGE
	register int v1 asm("v1");

	// Turn "\\BIGFILE.BIG;1" into "BIGFILE.BIG"

	char *str = filename;
	while (*str != 0)
	{
		if (*str == ';')
			*str = '\0';
		str++;
	}

	v1 = PCopen(&filename[1], PCDRV_MODE_READ);

	fileFD[fileCount] = fileCount;

	// max of 256 files
	// CTR has 40 files,
	// top 1 byte is fd
	// bottom 3 bytes is sectorIndex
	*(int *)&loc->pos = fileCount << 24;

	fileCount++;

	return loc;
#endif
}

// TODO, put his decoding in vsync callback
int boolDecodeXaDuringVsyncCallback = 0;

int pcCdControl(u8 com, u_long *buf, u8 *result)
{
#ifndef CTR_NATIVE
	// because this API is STRANGE
	register int v1 asm("v1");
#endif

	// Seek for File IO (CdlSetloc)
	if (com == CdlSetloc)
	{
		// On PS1 Disc, variable names have literal meanings
		//	bigfile->cdpos / cdlFileHWL->pos = disc pos of Bigfile
		//	sector offset eOffs / firstSector = offset from start of file

		// On PCDRV, we encode a bitmask hack
		//	Any CdlFile "Pos" = currFD << 24
		//	The "+ offset" is then stored in bottom 24 bits
		//	CdIntToPos encodes this, for CdControl input parameter

#ifdef CTR_NATIVE
		PCDRV_NativeSetLoc((const CdlLOC *)buf);
#else
		currFD = *(int *)buf >> 24;
		int sector = *(int *)buf & 0xffffff;

		v1 = PClseek(fileFD[currFD], sector * 0x800, PCDRV_SEEK_SET);
#endif
	}

#ifdef CTR_NATIVE

	// Step 1: CdlSetmode XA Stream (ignore)
	// Step 2: CdlSetfilter Goto Sector (ignore)
	// Step 3: CdIntToPos
	//	-- can I reverse "sum" into XA filename?
	//	-- just like how I hook cdIntToPos for bigfile?
	// Step 4: CdlReadS Start Reading XA
	//	-- replace cdPos with CategoryID, XaID, xaChannel(=0)

	// Or should I just add to XAPlay "if pc, start pcdrv xa code"?
	// then that "start pcdrv xa cdoe" can enable boolDecodeXaDuringVsyncCallback

	// Mode for DATA or XA Stream
	if (com == CdlSetmode)
	{
		// 0xE8 =
		//	CdlModeSpeed (double speed)
		//	CdlModeRT (ADPCM xa streaming)
		//	CdlModeSize1 (sector size = 2340, not 2048)
		//	CdlModeSF (subheader filter)
		if (buf[0] == 0xE8)
		{
			// note to self, can use sdata->discMode
			boolDecodeXaDuringVsyncCallback = 1;
		}

		if (buf[0] == CdlModeSpeed)
		{
			// note to self, can use sdata->discMode
			boolDecodeXaDuringVsyncCallback = 0;
		}
	}

#endif

	// TODO: handle v1 failures
	return 1;
}

int pcCdRead(int sectors, u_long *buf, int mode)
{
#ifndef CTR_NATIVE
	// because this API is STRANGE
	register int v1 asm("v1");
#endif

#ifdef CTR_NATIVE
	PCDRV_NativeReadSectors(sectors, buf);
#else
	v1 = PCread(fileFD[currFD], buf, sectors * 0x800);
#endif

	if (fpReadCallback != 0)
	{
		// TODO: Error handling of v1
		fpReadCallback(CdlComplete, 0);
	}

	return 1;
}

int pcCdReadSync(int mode, u8 *result)
{
	// do nothing, pcCdRead() already finished

	// zero sectors remain
	return 0;
}

// PS1
#ifndef CTR_NATIVE

// Copied from here:
// https://github.com/Lameguy64/PSn00bSDK/blob/master/libpsn00b/psxapi/_syscalls.s
asm("## PCDRV (host file access) API                                   \n"
    ".section .text.PCinit, \"ax\", @progbits                          \n"
    ".global PCinit                                                    \n"
    ".type PCinit, @function                                           \n"
    "                                                                  \n"
    "PCinit:                                                           \n"
    "	break 0, 0x101 # () -> error                                   \n"
    "                                                                  \n"
    "	jr    $ra                                                      \n"
    "	nop                                                            \n"
    "                                                                  \n"
    ".section .text.PCcreat, \"ax\", @progbits                         \n"
    ".global PCcreat                                                   \n"
    ".type PCcreat, @function                                          \n"
    "                                                                  \n"
    "PCcreat:                                                          \n"
    "	li    $a2, 0                                                   \n"
    "	move  $a1, $a0                                                 \n"
    "	break 0, 0x102 # (path, path, 0) -> error, fd                  \n"
    "                                                                  \n"
    "	bgez  $v0, .Lcreate_ok # if (error < 0) fd = error             \n"
    "	nop                                                            \n"
    "	move  $v1, $v0                                                 \n"
    ".Lcreate_ok:                                                      \n"
    "	jr    $ra # return fd                                          \n"
    "	move  $v0, $v1                                                 \n"
    "                                                                  \n"
    ".section .text.PCopen, \"ax\", @progbits                          \n"
    ".global PCopen                                                    \n"
    ".type PCopen, @function                                           \n"
    "                                                                  \n"
    "PCopen:                                                           \n"
    "	move  $a2, $a1                                                 \n"
    "	move  $a1, $a0                                                 \n"
    "	break 0, 0x103 # (path, path, mode) -> error, fd               \n"
    "                                                                  \n"
    "	bgez  $v0, .Lopen_ok # if (error < 0) fd = error               \n"
    "	nop                                                            \n"
    "	move  $v1, $v0                                                 \n"
    ".Lopen_ok:                                                        \n"
    "	jr    $ra # return fd                                          \n"
    "	move  $v0, $v1                                                 \n"
    "                                                                  \n"
    ".section .text.PCclose, \"ax\", @progbits                         \n"
    ".global PCclose                                                   \n"
    ".type PCclose, @function                                          \n"
    "                                                                  \n"
    "PCclose:                                                          \n"
    "	move  $a1, $a0                                                 \n"
    "	break 0, 0x104 # (fd, fd) -> error                             \n"
    "                                                                  \n"
    "	jr    $ra                                                      \n"
    "	nop                                                            \n"
    "                                                                  \n"
    ".section .text.PCread, \"ax\", @progbits                          \n"
    ".global PCread                                                    \n"
    ".type PCread, @function                                           \n"
    "                                                                  \n"
    "PCread:                                                           \n"
    "	move  $a3, $a1                                                 \n"
    "	move  $a1, $a0                                                 \n"
    "	break 0, 0x105 # (fd, fd, length, data) -> error, length       \n"
    "                                                                  \n"
    "	bgez  $v0, .Lread_ok # if (error < 0) length = error           \n"
    "	nop                                                            \n"
    "	move  $v1, $v0                                                 \n"
    ".Lread_ok:                                                        \n"
    "	jr    $ra # return length                                      \n"
    "	move  $v0, $v1                                                 \n"
    "                                                                  \n"
    ".section .text.PCwrite, \"ax\", @progbits                         \n"
    ".global PCwrite                                                   \n"
    ".type PCwrite, @function                                          \n"
    "                                                                  \n"
    "PCwrite:                                                          \n"
    "	move  $a3, $a1                                                 \n"
    "	move  $a1, $a0                                                 \n"
    "	break 0, 0x106 # (fd, fd, length, data) -> error, length       \n"
    "                                                                  \n"
    "	bgez  $v0, .Lwrite_ok # if (error < 0) length = error          \n"
    "	nop                                                            \n"
    "	move  $v1, $v0                                                 \n"
    ".Lwrite_ok:                                                       \n"
    "	jr    $ra # return length                                      \n"
    "	move  $v0, $v1                                                 \n"
    "                                                                  \n"
    ".section .text.PClseek, \"ax\", @progbits                         \n"
    ".global PClseek                                                   \n"
    ".type PClseek, @function                                          \n"
    "                                                                  \n"
    "PClseek:                                                          \n"
    "	move  $a3, $a2                                                 \n"
    "	move  $a2, $a1                                                 \n"
    "	move  $a1, $a0                                                 \n"
    "	break 0, 0x107 # (fd, fd, offset, mode) -> error, offset       \n"
    "                                                                  \n"
    "	bgez  $v0, .Lseek_ok # if (error < 0) offset = error           \n"
    "	nop                                                            \n"
    "	move  $v1, $v0                                                 \n"
    ".Lseek_ok:                                                        \n"
    "	jr    $ra # return offset                                      \n"
    "	move  $v0, $v1                                                 \n");
#endif
