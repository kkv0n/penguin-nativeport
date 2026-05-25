#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e830-0x8003e85c.
void MEMPACK_NewPack(void *start, int size)
{
	struct Mempack *ptrMempack = sdata->PtrMempack;
	void *end = (void *)((u32)start + size);

	ptrMempack->packSize = size;
	ptrMempack->start = start;
	ptrMempack->lastFreeByte = end;
	ptrMempack->endOfAllocator = end;
	ptrMempack->endOfMemory = end;
	ptrMempack->firstFreeByte = start;
	ptrMempack->numBookmarks = 0;
}
