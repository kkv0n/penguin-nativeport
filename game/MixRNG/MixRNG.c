#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003ea28-0x8003ea6c
int MixRNG_Scramble()
{
	// random algorithm for seemingly-random numbers
	sdata->randomNumber = (sdata->randomNumber * 0x6255 + 0x3619) & 0xffff;
	return sdata->randomNumber;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003ea6c-0x8003eaac
int MixRNG_Particles(int param_1)
{
	u32 uVar1;

	uVar1 = RngDeadCoed(&sdata->gGT->deadcoed_struct);
	return (int)((uVar1 & 0xffff) * param_1) >> 0x10;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003eaac-0x8003eae0
// The result is signed: retail's only caller shifts it with sra, same as it does
// with MixRNG_Scramble. The 0xffff is an explicit mask in retail (andi between a
// lw/sw pair), not a narrower storage type.
int MixRNG_GetValue(int param_1)
{
	return (param_1 * 0x6255 + 0x3619U) & 0xffff;
}
