#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e938-0x8003e94c.
void MEMPACK_ClearHighMem()
{
	sdata->PtrMempack->lastFreeByte = sdata->PtrMempack->endOfAllocator;
}
