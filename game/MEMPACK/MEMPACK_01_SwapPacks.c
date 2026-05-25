#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003e80c-0x8003e830.
void MEMPACK_SwapPacks(int index)
{
	sdata->PtrMempack = &sdata->mempack[index];
}
