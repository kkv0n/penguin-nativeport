#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e94c-0x8003e978.
void *MEMPACK_ReallocMem(int allocSize)
{
	int newAllocSize;
	struct Mempack *ptrMempack = sdata->PtrMempack;

	newAllocSize = (allocSize + 3) & 0xfffffffc;
	ptrMempack->firstFreeByte = (void *)((int)ptrMempack->firstFreeByte - ptrMempack->sizeOfPrevAllocation + newAllocSize);
	ptrMempack->sizeOfPrevAllocation = newAllocSize;

	return ptrMempack->firstFreeByte;
}
