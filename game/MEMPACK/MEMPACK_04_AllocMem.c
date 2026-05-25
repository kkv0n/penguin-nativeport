#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e874-0x8003e8e8.
void *MEMPACK_AllocMem(int allocSize)
{
	int firstFreeByte;
	int newAllocSize;
	struct Mempack *ptrMempack = sdata->PtrMempack;

	if (MEMPACK_GetFreeBytes() < allocSize)
	{
		CTR_ErrorScreen(0xFF, 0, 0);
		for (;;)
		{
		}
	}

	newAllocSize = (allocSize + 3) & 0xfffffffc;
	ptrMempack->sizeOfPrevAllocation = newAllocSize;

	firstFreeByte = (int)ptrMempack->firstFreeByte;
	ptrMempack->firstFreeByte = (void *)(firstFreeByte + newAllocSize);

	return (void *)firstFreeByte;
}
