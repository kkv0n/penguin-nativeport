#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e8e8-0x8003e938.
void *MEMPACK_AllocHighMem(int allocSize)
{
	int newLastFreeByte;

	while (MEMPACK_GetFreeBytes() < allocSize)
	{
	}

	allocSize = (allocSize + 3) & 0xfffffffc;
	sdata->PtrMempack->sizeOfPrevAllocation = allocSize;

	newLastFreeByte = (int)sdata->PtrMempack->lastFreeByte - allocSize;
	sdata->PtrMempack->lastFreeByte = (void *)newLastFreeByte;

	return (void *)newLastFreeByte;
}
