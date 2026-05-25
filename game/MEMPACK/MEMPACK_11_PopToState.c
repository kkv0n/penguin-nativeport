#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003ea08-0x8003ea28.
void MEMPACK_PopToState(int id)
{
	struct Mempack *ptrMempack = sdata->PtrMempack;

	ptrMempack->numBookmarks = id;
	ptrMempack->firstFreeByte = ptrMempack->bookmarks[id];
}
