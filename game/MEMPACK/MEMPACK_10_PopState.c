#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e9d0-0x8003ea08.
void MEMPACK_PopState()
{
	struct Mempack *ptrMempack = sdata->PtrMempack;
	int numBookmarks;

	numBookmarks = ptrMempack->numBookmarks;
	if (numBookmarks > 0)
	{
		numBookmarks--;
		ptrMempack->firstFreeByte = ptrMempack->bookmarks[numBookmarks];
		ptrMempack->numBookmarks = numBookmarks;
	}
}
