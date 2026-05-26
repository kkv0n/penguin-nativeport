#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031608-0x80031734. Retail ignores _objTh.
struct Instance *LinkedCollide_Hitbox(struct Instance *objInst, struct Thread *_objTh, struct Thread *thBucket, struct BoundingBox bbox)
{
	struct Instance *thInst;
	int diff_y;
	MATRIX thInstMatrix;
	SVECTOR thInstPos;
	VECTOR outVec;
	s32 flags[2];

	(void)_objTh;

	// Loop over thBucket Linked List
	for (; thBucket != 0; thBucket = thBucket->siblingThread)
	{
		thInst = thBucket->inst;

		thInstPos.vx = thInst->matrix.t[0];
		thInstPos.vy = thInst->matrix.t[1];
		thInstPos.vz = thInst->matrix.t[2];

		diff_y = thInst->matrix.t[1] - objInst->matrix.t[1];

		MATH_HitboxMatrix(&thInstMatrix, &objInst->matrix);

		SetRotMatrix(&thInstMatrix);
		SetTransMatrix(&thInstMatrix);

		RotTrans(&thInstPos, &outVec, (long *)flags);

		if ((bbox.min[0] < outVec.vx) && (outVec.vx < bbox.max[0]) && (bbox.min[2] < outVec.vz) && (outVec.vz < bbox.max[2]) && (bbox.min[1] <= diff_y) &&
		    (diff_y < bbox.max[1]))
		{
			return thInst; // collision thread instance
		}
	}
	return 0;
}
