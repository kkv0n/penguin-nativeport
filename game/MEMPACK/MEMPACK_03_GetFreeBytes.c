#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e85c-0x8003e874.
int MEMPACK_GetFreeBytes()
{
	struct Mempack *ptrMempack = sdata->PtrMempack;

	return (u32)ptrMempack->lastFreeByte - (u32)ptrMempack->firstFreeByte;
}
