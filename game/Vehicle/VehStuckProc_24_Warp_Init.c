#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80068e04-0x80068f90.
void VehStuckProc_Warp_Init(struct Thread *th, struct Driver *d)
{
	if (d->kartState == KS_WARP_PAD)
		return;

	// If you are not in a warp pad

	d->KartStates.Warp.timer = 0x3c;
	d->KartStates.Warp.heightOffset = 0;
	d->KartStates.Warp.quadHeight = d->quadBlockHeight;

	// Warp sound?
	OtherFX_Play(0x97, 1);

	OtherFX_Stop1((int)d->driverAudioPtrs[1]);
	d->driverAudioPtrs[1] = NULL;
	OtherFX_Stop1((int)d->driverAudioPtrs[2]);
	d->driverAudioPtrs[2] = NULL;
	OtherFX_Stop1((int)d->driverAudioPtrs[0]);
	d->driverAudioPtrs[0] = NULL;

	u8 playerID = d->driverID;

	int engine = data.MetaDataCharacters[data.characterIDs[playerID]].engineID;

	EngineAudio_Stop((engine * 4) + playerID);

	// CameraDC, freecam mode
	sdata->gGT->cameraDC[playerID].cameraMode = 3;

	// driver -> instSelf
	struct Instance *inst = d->instSelf;

	// instance flags, now reflective
	inst->flags |= 0x4000;

	// vertical line for split or reflection
	inst->vertSplit = (s16)(d->quadBlockHeight >> 8);

	// you are now in a warp pad
	d->kartState = KS_WARP_PAD;

	d->speed = 0;
	d->speedApprox = 0;

	d->funcPtrs[0] = NULL;
	d->funcPtrs[1] = NULL;
	d->funcPtrs[2] = NULL;
	d->funcPtrs[3] = VehPhysProc_Driving_Audio;
	d->funcPtrs[4] = VehStuckProc_Warp_PhysAngular;
	d->funcPtrs[5] = NULL;
	d->funcPtrs[6] = NULL;
	d->funcPtrs[7] = NULL;
	d->funcPtrs[8] = NULL;
	d->funcPtrs[9] = NULL;
	d->funcPtrs[10] = VehPhysForce_TranslateMatrix;
	d->funcPtrs[11] = VehFrameProc_Driving;
	d->funcPtrs[12] = VehEmitter_DriverMain;

	// driver is warping
	d->actionsFlagSet |= 0x4000;
}
