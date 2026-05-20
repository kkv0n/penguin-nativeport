#include <common.h>

#ifdef CTR_INTERNAL
volatile int gCtrDebugTires = 0;
volatile int gCtrDebugTireBudget = 0;
volatile int gCtrDebugTireLevel = -1;

enum
{
	CTR_TIREDBG_RENDERBUCKET = 1 << 0,
	CTR_TIREDBG_SOLID_STAGE = 1 << 1,
	CTR_TIREDBG_SOLID_PRIM = 1 << 2,
	CTR_TIREDBG_REFLECT_STAGE = 1 << 3,
	CTR_TIREDBG_REFLECT_PRIM = 1 << 4,
	CTR_TIREDBG_RENDERBUCKET_PRIM = 1 << 5,
	CTR_TIREDBG_RENDERBUCKET_REJECT = 1 << 6,
};

static int CtrTireDebug_ShouldLog(int mask)
{
	if ((gCtrDebugTires & mask) == 0)
		return 0;

	if (gCtrDebugTireBudget == 0)
		return 0;

	if (gCtrDebugTireLevel >= 0)
	{
		struct GameTracker *gGT = sdata->gGT;
		if ((gGT == 0) || (gGT->levelID != gCtrDebugTireLevel))
			return 0;
	}

	if (gCtrDebugTireBudget > 0)
		gCtrDebugTireBudget--;

	return 1;
}
#endif

struct RenderBucketEntry
{
	struct Instance *inst;
	struct Instance *instPlayerBase;
};

typedef struct
{
	u_char x;
	u_char y;
	u_char z;
} RenderBucketCompVertex;

typedef struct
{
	u_char x;
	u_char y;
	u_char z;
	u_char w;
} RenderBucketVertex;

struct RenderBucketBounds
{
	int minX;
	int maxX;
	int minY;
	int maxY;
	int minZ;
	int maxZ;
};

struct RenderBucketDrawContext
{
	struct Instance *inst;
	struct InstDrawPerPlayer *idpp;
	struct PushBuffer *pb;
	struct PrimMem *primMem;
	struct ModelHeader *mh;
	struct ModelFrame *mf;
	struct ModelAnim *anim;
	char *vertData;
	RenderBucketVertex tempCoords[4];
	int tempColor[4];
	RenderBucketVertex stack[256];
	int bitIndex;
	int x_alu;
	int y_alu;
	int z_alu;
	int stripLength;
	int vertexIndex;
};

static int RenderBucket_GetSignedBits(unsigned int *vertData, int *bitIndex, int bits)
{
	int const b = *bitIndex >> 5;
	int const e = 32 - bits;
	int const s = e - (*bitIndex & 31);
	int const ret = s < 0 ? (vertData[b] << -s) | (vertData[b + 1] >> (s & 31)) : vertData[b] >> s;

	*bitIndex += bits;
	return (ret << e) >> e;
}

static struct ModelAnim *RenderBucket_GetAnim(struct Instance *inst, struct ModelHeader *mh)
{
	if (mh->ptrAnimations == 0)
		return 0;

	if (mh->numAnimations == 0)
		return 0;

	if (inst->animIndex >= mh->numAnimations)
		return 0;

	return mh->ptrAnimations[inst->animIndex];
}

static unsigned int RenderBucket_PackXY(int x, int y)
{
	return ((unsigned int)(unsigned short)x) | ((unsigned int)(unsigned short)y << 16);
}

static int RenderBucket_SignExtendByte(u_char value)
{
	return ((value & 0x80) != 0) ? (int)value - 0x100 : value;
}

static unsigned int RenderBucket_OTAddress(void *ptr)
{
	return (unsigned int)ptr & 0xffffff;
}

static int RenderBucket_ClampOTByteOffset(int depthBin)
{
	int byteOffset = depthBin << 2;

	if (depthBin < 0)
		return 0;

	if (byteOffset > 0xffc)
		return 0xffc;

	return byteOffset;
}

static void RenderBucket_BoundsInit(struct RenderBucketBounds *bounds, int sxy, int depth)
{
	int x = (short)sxy;
	int y = (short)(sxy >> 16);

	bounds->minX = x;
	bounds->maxX = x;
	bounds->minY = y;
	bounds->maxY = y;
	bounds->minZ = depth;
	bounds->maxZ = depth;
}

static void RenderBucket_BoundsUpdate(struct RenderBucketBounds *bounds, int sxy, int depth)
{
	int x = (short)sxy;
	int y = (short)(sxy >> 16);

	// NOTE(aalhendi): PSX-backfeed blocker: retail helper 0x80071524 consumes
	// packed SXY in t1 and depth in t2, then updates t3/t4/t5/t6/t7/s0.
	// Native uses explicit bounds storage until that register ABI is restored.
	if (y < bounds->minY)
		bounds->minY = y;
	if (y > bounds->maxY)
		bounds->maxY = y;

	if (x < bounds->minX)
		bounds->minX = x;
	if (x > bounds->maxX)
		bounds->maxX = x;

	if (depth < bounds->minZ)
		bounds->minZ = depth;
	if (depth > bounds->maxZ)
		bounds->maxZ = depth;
}

static void RenderBucket_ProjectBoundsPoint(struct RenderBucketBounds *bounds, int x, int y, int z, int firstPoint)
{
	SVECTOR point;
	int sxy;
	int depth;

	point.vx = x;
	point.vy = y;
	point.vz = z;
	point.pad = 0;

	gte_ldv0(&point);
	gte_rtps();
	gte_stsxy0(&sxy);
	gte_stsz(&depth);

	if (firstPoint != 0)
		RenderBucket_BoundsInit(bounds, sxy, depth);
	else
		RenderBucket_BoundsUpdate(bounds, sxy, depth);
}

static int RenderBucket_ProjectFrameBounds(struct ModelFrame *frame, struct PushBuffer *pb, struct InstDrawPerPlayer *idpp, struct RenderBucketBounds *bounds)
{
	int minX = ((int)frame->pos[0]) << 2;
	int minY = ((int)frame->pos[1]) << 2;
	int minZ = ((int)frame->pos[2]) << 2;
	int maxX = minX + 0x3fc;
	int maxY = minY + 0x3fc;
	int maxZ = minZ + 0x3fc;

	// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw projects frame
	// bounds through live GTE state at 0x80071054-0x80071164 and folds them with
	// helper 0x80071524. Native uses the queued MVP and explicit bounds storage
	// until the exact register sequence is restored.
	gte_SetRotMatrix(&idpp->mvp);
	gte_SetTransMatrix(&idpp->mvp);
	gte_SetGeomOffset(pb->rect.w >> 1, pb->rect.h >> 1);
	gte_SetGeomScreen(pb->distanceToScreen_PREV);

	RenderBucket_ProjectBoundsPoint(bounds, minX, minY, minZ, 1);
	RenderBucket_ProjectBoundsPoint(bounds, maxX, minY, minZ, 0);
	RenderBucket_ProjectBoundsPoint(bounds, maxX, maxY, minZ, 0);
	RenderBucket_ProjectBoundsPoint(bounds, minX, maxY, minZ, 0);
	RenderBucket_ProjectBoundsPoint(bounds, minX, minY, maxZ, 0);
	RenderBucket_ProjectBoundsPoint(bounds, maxX, minY, maxZ, 0);
	RenderBucket_ProjectBoundsPoint(bounds, maxX, maxY, maxZ, 0);
	RenderBucket_ProjectBoundsPoint(bounds, minX, maxY, maxZ, 0);

	if (bounds->maxX < 0)
		return 0;

	if (bounds->maxY < 0)
		return 0;

	if (bounds->maxZ < 0)
		return 0;

	if (bounds->minX > pb->rect.w)
		return 0;

	if (bounds->minY > pb->rect.h)
		return 0;

	return 1;
}

static int RenderBucket_GetViewDepth(struct Instance *inst, struct PushBuffer *pb)
{
	VECTOR pos;
	VECTOR viewPos;

	// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw derives this
	// camera-relative depth from scratchpad/GTE register state at
	// 0x80070a1c-0x80070ae0. Native keeps the same ViewProj input explicit
	// until the full QueueLev/QueueNonLev scratchpad entry protocol is restored.
	if ((inst->flags & SCREENSPACE_INSTANCE) != 0)
		return inst->matrix.t[2];

	pos.vx = inst->matrix.t[0] - pb->matrix_Camera.t[0];
	pos.vy = inst->matrix.t[1] - pb->matrix_Camera.t[1];
	pos.vz = inst->matrix.t[2] - pb->matrix_Camera.t[2];
	ApplyMatrixLV(&pb->matrix_ViewProj, &pos, &viewPos);

	return viewPos.vz;
}

static struct ModelHeader *RenderBucket_SelectModelHeader(struct Instance *inst, struct PushBuffer *pb, int *lodIndexOut, int *viewDepthOut)
{
	struct ModelHeader *mh;
	int viewDepth;
	int projectedDistance;

	*lodIndexOut = 0;
	*viewDepthOut = 0;

	if (inst->model->numHeaders <= 0)
		return 0;

	if (pb->distanceToScreen_PREV == 0)
		return 0;

	viewDepth = RenderBucket_GetViewDepth(inst, pb);
	*viewDepthOut = viewDepth;
	// NOTE(aalhendi): Retail keeps the low 32 bits of this product before dividing by GTE H.
	projectedDistance = (int)(unsigned int)((long long)(pb->rect.w >> 1) * viewDepth) / pb->distanceToScreen_PREV;
	mh = inst->model->headers;

	// NOTE(aalhendi): PSX-backfeed blocker: retail LOD selection at
	// 0x80070ae4-0x80070b34 uses transformed GTE depth in scratchpad 0x8c,
	// divides by GTE H, and walks ModelHeader entries through s4/s5. Native C
	// preserves the projected-distance comparison explicitly until the register
	// walk is restored.
	for (int lodIndex = 0; lodIndex < inst->model->numHeaders; lodIndex++, mh++)
	{
		if ((projectedDistance - (unsigned short)mh->maxDistanceLOD) < 0)
		{
			*lodIndexOut = lodIndex;
			return mh;
		}
	}

	return 0;
}

static void RenderBucket_GteLoadRotMatrixWords(unsigned int m0, unsigned int m1, unsigned int m2, unsigned int m3, unsigned int m4)
{
	CTC2(m0, 0);
	CTC2(m1, 1);
	CTC2(m2, 2);
	CTC2(m3, 3);
	CTC2(m4, 4);
}

static void RenderBucket_GteScaleMatrixColumns(unsigned int *m0, unsigned int *m1, unsigned int *m2, unsigned int *m3, unsigned int *m4)
{
	unsigned int v0;
	unsigned int v1;
	unsigned int v2;
	unsigned int t0;
	unsigned int t1;
	unsigned int t2;
	unsigned int t3;
	unsigned int t4;

	// NOTE(aalhendi): PSX-backfeed blocker: retail helper 0x8006c49c consumes
	// matrix words through t3/t4/t5/t6/t7 and live GTE color/rotation state.
	// Native mirrors the opcode sequence with explicit C parameters until that
	// register-entry ABI is restored.
	MTC2((*m0 & 0xffff) | (*m1 & 0xffff0000), 0);
	MTC2(*m3, 1);
	doCOP2(0x0486012);

	MTC2((*m0 >> 16) | (*m2 << 16), 2);
	MTC2(*m3 >> 16, 3);
	t0 = MFC2(9);
	t1 = MFC2(10);
	t3 = MFC2(11);
	doCOP2(0x048e012);

	MTC2((*m1 & 0xffff) | (*m2 & 0xffff0000), 4);
	MTC2(*m4, 5);
	t0 &= 0xffff;
	t1 <<= 16;
	t3 &= 0xffff;

	v0 = MFC2(9);
	v1 = MFC2(10);
	v2 = MFC2(11);
	doCOP2(0x0496012);

	v0 <<= 16;
	*m0 = t0 | v0;
	v1 &= 0xffff;
	v2 <<= 16;
	*m3 = t3 | v2;

	v0 = MFC2(9);
	t2 = MFC2(10);
	t4 = MFC2(11);
	v0 &= 0xffff;
	*m1 = t1 | v0;
	t2 <<= 16;
	*m2 = v1 | t2;
	*m4 = t4;

	RenderBucket_GteLoadRotMatrixWords(*m0, *m1, *m2, *m3, *m4);
}

static int RenderBucket_GetScaledMatrixElem(struct Instance *inst, int scale[3], int row, int col)
{
	return (inst->matrix.m[row][col] * scale[col]) >> 8;
}

static void RenderBucket_BuildM3x3(struct Instance *inst, struct ModelHeader *mh, int viewDepth, struct InstDrawPerPlayer *idpp)
{
	unsigned int m0;
	unsigned int m1;
	unsigned int m2;
	unsigned int m3;
	unsigned int m4;
	unsigned int packedScaleXY;
	int depthShift;
	int scaleXYShift;
	int scaleZShift;
	int scaleX;
	int scaleY;
	int scaleZ;

	// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw produces IDPP
	// m3x3 at 0x80070b38-0x80070c18 by loading the instance matrix through
	// t3/t4/t5/t6/t7, setting the GTE color matrix from ModelHeader scale, then
	// calling helper 0x8006c49c. Native preserves the same field and GTE helper
	// sequence through explicit C parameters until the exact register/scratchpad
	// boundary is restored.
	// NOTE(aalhendi): Retail treats MATRIX halfwords as packed GTE register words here.
	m0 = *(unsigned int *)&inst->matrix.m[0][0];
	m1 = *(unsigned int *)&inst->matrix.m[0][2];
	m2 = *(unsigned int *)&inst->matrix.m[1][1];
	m3 = *(unsigned int *)&inst->matrix.m[2][0];
	m4 = *(unsigned int *)&inst->matrix.m[2][2];
	RenderBucket_GteLoadRotMatrixWords(m0, m1, m2, m3, m4);

	depthShift = (viewDepth < 0x1000) ? 2 : 0;
	scaleXYShift = 0x12 - depthShift;
	scaleZShift = 2 - depthShift;
	packedScaleXY = *(unsigned int *)&mh->scale[0];

	CTC2((packedScaleXY << 16) >> scaleXYShift, 16);
	CTC2(0, 17);
	CTC2(packedScaleXY >> scaleXYShift, 18);
	CTC2(0, 19);
	CTC2((unsigned short)mh->scale[2] >> scaleZShift, 20);

	scaleX = inst->scale[0];
	scaleY = inst->scale[1];
	scaleZ = inst->scale[2];
	if ((inst->flags & PIXEL_LOD) != 0)
	{
		int pixelScale = (viewDepth >> 1) + 0x1000;

		scaleX = (pixelScale * scaleX) >> 12;
		scaleY = (pixelScale * scaleY) >> 12;
		scaleZ = (pixelScale * scaleZ) >> 12;
	}

	MTC2(RenderBucket_PackXY(scaleX, scaleY), 0);
	MTC2(scaleZ, 1);
	doCOP2(0x04c6012);

	RenderBucket_GteScaleMatrixColumns(&m0, &m1, &m2, &m3, &m4);
	*(unsigned int *)&idpp->m3x3.m[0][0] = m0;
	*(unsigned int *)&idpp->m3x3.m[0][2] = m1;
	*(unsigned int *)&idpp->m3x3.m[1][1] = m2;
	*(unsigned int *)&idpp->m3x3.m[2][0] = m3;
	*(unsigned int *)&idpp->m3x3.m[2][2] = m4;
}

static void RenderBucket_BuildMvp(struct Instance *inst, struct ModelHeader *mh, struct PushBuffer *pb, MATRIX *mvp)
{
	int scale[3];
	VECTOR pos;

	// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw owns IDPP matrix
	// production in 0x80070b38-0x80071054 through MATRIX_SET/FUN_8006c49c/
	// FUN_8006c558 and live GTE register state. Native currently preserves MVP
	// producer ownership with explicit C math; restore the exact GTE helper
	// choreography before PSX backfeed or an ASM-verified stamp.
	scale[0] = (mh->scale[0] * inst->scale[0]) >> 12;
	scale[1] = (mh->scale[1] * inst->scale[1]) >> 12;
	scale[2] = (mh->scale[2] * inst->scale[2]) >> 12;

#define RB_MVP(row, col, index) ((pb->matrix_ViewProj.m[row][index] * RenderBucket_GetScaledMatrixElem(inst, scale, index, col)) >> 0x10)

	mvp->m[0][0] = RB_MVP(0, 0, 0) + RB_MVP(0, 0, 1) + RB_MVP(0, 0, 2);
	mvp->m[0][1] = RB_MVP(0, 1, 0) + RB_MVP(0, 1, 1) + RB_MVP(0, 1, 2);
	mvp->m[0][2] = RB_MVP(0, 2, 0) + RB_MVP(0, 2, 1) + RB_MVP(0, 2, 2);
	mvp->m[1][0] = RB_MVP(1, 0, 0) + RB_MVP(1, 0, 1) + RB_MVP(1, 0, 2);
	mvp->m[1][1] = RB_MVP(1, 1, 0) + RB_MVP(1, 1, 1) + RB_MVP(1, 1, 2);
	mvp->m[1][2] = RB_MVP(1, 2, 0) + RB_MVP(1, 2, 1) + RB_MVP(1, 2, 2);
	mvp->m[2][0] = RB_MVP(2, 0, 0) + RB_MVP(2, 0, 1) + RB_MVP(2, 0, 2);
	mvp->m[2][1] = RB_MVP(2, 1, 0) + RB_MVP(2, 1, 1) + RB_MVP(2, 1, 2);
	mvp->m[2][2] = RB_MVP(2, 2, 0) + RB_MVP(2, 2, 1) + RB_MVP(2, 2, 2);

#undef RB_MVP

	pos.vx = inst->matrix.t[0] - pb->matrix_Camera.t[0];
	pos.vy = inst->matrix.t[1] - pb->matrix_Camera.t[1];
	pos.vz = inst->matrix.t[2] - pb->matrix_Camera.t[2];

	ApplyMatrixLV(&pb->matrix_ViewProj, &pos, &mvp->t[0]);
}

static u_long *RenderBucket_AllocateOTRange(struct OTMem *otMem, struct PushBuffer *pb, int minDepth, int maxDepth, int viewDepth, int depthBias,
                                            int usePushBuffer)
{
	u_long *rangeStart;
	u_long *rangeEnd;
	u_long *newCurr;
	u_long *otSlot;
	int range;
	int byteOffset;

	if (otMem == 0)
		return 0;

	if (pb->ptrOT == 0)
		return 0;

	range = maxDepth - minDepth;
	if (range < 0)
		return 0;

	rangeStart = otMem->curr;
	rangeEnd = rangeStart + range;
	newCurr = rangeEnd + 1;

	if (newCurr >= (otMem->end - 1))
		return 0;

	// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw links per-instance
	// OT ranges by writing raw 24-bit primitive tags at 0x80071218-0x800712f4.
	// Native uses the same tag-chain shape with explicit C pointer masking.
	otMem->curr = newCurr;

	byteOffset = RenderBucket_ClampOTByteOffset((viewDepth >> 6) + depthBias);

	if (usePushBuffer != 0)
	{
		// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw stores the
		// range start/end/byte-offset metadata at PushBuffer 0xf4/0xf8/0xfc for
		// PUSHBUFFER_EXISTS instances. Native keeps 0xf4 as ptrOT until
		// DrawInstPrim/PushBuffer ownership is restored, so the range start is
		// held by IDPP unkE4 and the extra metadata is stored in named fields.
		rangeStart[0] = 0;
		pb->renderBucketOTRangeEnd = rangeEnd;
		pb->renderBucketOTByteOffset = byteOffset;
	}
	else
	{
		otSlot = (u_long *)((char *)pb->ptrOT + byteOffset);
		rangeStart[0] = otSlot[0];
		otSlot[0] = RenderBucket_OTAddress(rangeEnd);
	}

	for (u_long *entry = rangeStart; entry != rangeEnd; entry++)
	{
		entry[1] = RenderBucket_OTAddress(entry);
	}

	return rangeStart - minDepth;
}

static void RenderBucket_UpdatePushBufferMetadata(struct PushBuffer *pb, const struct RenderBucketBounds *bounds, unsigned int *instFlags)
{
	int width;
	int height;
#ifdef CTR_INTERNAL
	unsigned int beforeFlags;
#endif

	if ((*instFlags & PUSHBUFFER_EXISTS) == 0)
		return;

#ifdef CTR_INTERNAL
	beforeFlags = *instFlags;
#endif

	// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw writes projected
	// screen pos/size to PushBuffer 0x100/0x104 and clears PUSHBUFFER_EXISTS
	// when the projected bounds exceed the small-buffer constraints at
	// 0x80071164-0x800711c8. Native mirrors the dataflow explicitly until the
	// exact scratchpad/register path is restored.
	width = bounds->maxX - bounds->minX;
	height = bounds->maxY - bounds->minY;
	pb->renderBucketScreenPos = RenderBucket_PackXY(bounds->minX, bounds->minY);
	pb->renderBucketScreenSize = RenderBucket_PackXY(width, height);

	if (bounds->minX < 0 || bounds->minY < 0 || height > 0x40 || width > 0x60 || bounds->maxX >= pb->rect.w || bounds->maxY >= pb->rect.h)
	{
		*instFlags &= ~PUSHBUFFER_EXISTS;
	}

#ifdef CTR_INTERNAL
	if (CtrTireDebug_ShouldLog(CTR_TIREDBG_RENDERBUCKET) != 0)
	{
		fprintf(stderr, "[TIREDBG][rb-pb] flags=%08x->%08x min=(%d,%d) max=(%d,%d) size=%dx%d rect=%dx%d pb=%p\n", beforeFlags, *instFlags, bounds->minX,
		        bounds->minY, bounds->maxX, bounds->maxY, width, height, pb->rect.w, pb->rect.h, (void *)pb);
	}
#endif
}

static int RenderBucket_ShouldAllocateSecondaryRange(unsigned int instFlags)
{
	if ((instFlags & PUSHBUFFER_EXISTS) != 0)
		return 0;

	if ((instFlags & REFLECTIVE) != 0)
		return 1;

	// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw can also allocate
	// the secondary OT range for SPLIT_LINE when scratchpad words 0x68/0x6c
	// were produced by the earlier split-line matrix path. Native does not
	// source-back that side-effect path yet, so only the unconditional
	// reflective branch is represented here.
	return 0;
}

static int RenderBucket_BuildDepthRange(struct Instance *inst, struct ModelFrame *frame, struct PushBuffer *pb, struct InstDrawPerPlayer *idpp,
                                        struct OTMem *otMem, int viewDepth, unsigned int *instFlags)
{
	struct RenderBucketBounds bounds;
	u_long *secondaryRange;
	int minDepth;
	int maxDepth;

	if (RenderBucket_ProjectFrameBounds(frame, pb, idpp, &bounds) == 0)
		return 0;

	RenderBucket_UpdatePushBufferMetadata(pb, &bounds, instFlags);
	minDepth = (bounds.minZ >> 5) - 2;
	maxDepth = (bounds.maxZ >> 5) + 1;

	idpp->depthOffset[0] = minDepth;
	idpp->depthOffset[1] = maxDepth;
	idpp->unkE4 = (int)RenderBucket_AllocateOTRange(otMem, pb, minDepth, maxDepth, viewDepth, RenderBucket_SignExtendByte(inst->unk50),
	                                                (*instFlags & PUSHBUFFER_EXISTS) != 0);
	idpp->unkE8 = idpp->unkE4;

	if (idpp->unkE4 == 0)
		return 0;

	if (RenderBucket_ShouldAllocateSecondaryRange(*instFlags) != 0)
	{
		// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw allocates this
		// distinct reflected/split OT range at 0x80071320-0x800713b4 with
		// signed Instance.unk51 as the second depth bias. Native mirrors the
		// reflective branch explicitly; the split-line scratchpad gate remains
		// pending.
		secondaryRange = RenderBucket_AllocateOTRange(otMem, pb, minDepth, maxDepth, viewDepth, RenderBucket_SignExtendByte(inst->unk51), 0);
		if (secondaryRange == 0)
			return 0;

		idpp->unkE8 = (int)secondaryRange;
	}

	return 1;
}

static struct ModelFrame *RenderBucket_GetFrame(struct Instance *inst, struct ModelHeader *mh, struct ModelFrame **nextFrameOut, int *deltaArrayOut)
{
	struct ModelAnim *anim;
	int frameIndex;
	int lastFrame;
	int hasNextFrame;
	char *firstFrame;

	*nextFrameOut = 0;
	*deltaArrayOut = 0;

	if (mh->ptrFrameData != 0)
	{
		*deltaArrayOut = mh->unk3;
		return mh->ptrFrameData;
	}

	anim = RenderBucket_GetAnim(inst, mh);
	if (anim == 0)
		return 0;

	if (anim->numFrames == 0)
		return 0;

	// NOTE(aalhendi): PSX-backfeed blocker: retail frame selection at
	// 0x80070ca0-0x80070dfc uses stack fields and returns current/next frame
	// through s6/s1 plus ptrDeltaArray through IDPP 0xd4. Native keeps the same
	// frame-selection rules as explicit return values until the register ABI is
	// restored.
	*deltaArrayOut = (int)anim->ptrDeltaArray;
	frameIndex = (unsigned short)inst->animFrame;
	lastFrame = (anim->numFrames & 0x7fff) - 1;
	hasNextFrame = 0;

	if ((short)anim->numFrames < 0)
	{
		lastFrame >>= 1;
		hasNextFrame = frameIndex & 1;
		frameIndex >>= 1;
	}

	if (frameIndex > lastFrame)
		frameIndex = lastFrame;

	firstFrame = (char *)MODELANIM_GETFRAME(anim);

	if (hasNextFrame != 0)
		*nextFrameOut = (struct ModelFrame *)(firstFrame + (anim->frameSize * (frameIndex + 1)));

	return (struct ModelFrame *)(firstFrame + (anim->frameSize * frameIndex));
}

static struct RenderBucketEntry *RenderBucket_QueueDraw(struct Instance *inst, struct RenderBucketEntry *rbi, int playerIndex, unsigned int lodMask,
                                                        int gameMode1, struct OTMem *otMem)
{
	struct ModelHeader *mh;
	struct ModelFrame *frame;
	struct ModelFrame *nextFrame;
	struct InstDrawPerPlayer *idpp;
	struct Instance *instPlayerBase;
	struct PushBuffer *pb;
	unsigned int queuedFlags;
	int deltaArray;
	int lodIndex;
	int viewDepth;

	// NOTE(aalhendi): PSX-backfeed blocker: retail QueueDraw consumes implicit
	// scratchpad/register state from QueueLev/QueueNonLev. This native helper
	// spells that state out as C parameters until the full ASM audit is done.
	(void)gameMode1;

	if (inst == 0)
		return rbi;

	if (inst->model == 0)
		return rbi;

	if (inst->model->headers == 0)
		return rbi;

	if ((inst->flags & lodMask) == 0)
		return rbi;

	instPlayerBase = (struct Instance *)((char *)inst + (playerIndex * sizeof(struct InstDrawPerPlayer)));
	idpp = INST_GETIDPP(instPlayerBase);
	pb = idpp->pushBuffer;

	if (pb == 0)
		return rbi;

	mh = RenderBucket_SelectModelHeader(inst, pb, &lodIndex, &viewDepth);
	if (mh == 0)
		return rbi;

	frame = RenderBucket_GetFrame(inst, mh, &nextFrame, &deltaArray);
	if (frame == 0)
		return rbi;

	queuedFlags = inst->flags;
	idpp->instFlags = queuedFlags & ~DRAW_SUCCESSFUL;
	idpp->unkbc = inst->alphaScale;
	idpp->ptrCurrFrame = frame;
	idpp->ptrNextFrame = nextFrame;
	idpp->ptrCommandList = mh->ptrCommandList;
	idpp->ptrTexLayout = mh->ptrTexLayout;
	idpp->ptrColorLayout = (unsigned int)mh->ptrColors;
	idpp->ptrDeltaArray = deltaArray;
	idpp->lodIndex = lodIndex;
	idpp->mh = mh;
	RenderBucket_BuildM3x3(inst, mh, viewDepth, idpp);
	RenderBucket_BuildMvp(inst, mh, pb, &idpp->mvp);
	idpp->unkE4 = 0;
	idpp->unkE8 = 0;
	idpp->unkEC = 0;
	idpp->unkF0 = 0;

	if (RenderBucket_BuildDepthRange(inst, frame, pb, idpp, otMem, viewDepth, &queuedFlags) == 0)
		return rbi;

	idpp->instFlags = queuedFlags | DRAW_SUCCESSFUL;
	rbi->inst = inst;
	rbi->instPlayerBase = instPlayerBase;
	return rbi + 1;
}

void *RenderBucket_QueueLevInstances(struct CameraDC *cDC, u_long *otMem, void *rbi, char *lod, char numPlyr, int gameMode1)
{
	struct RenderBucketEntry *entry = (struct RenderBucketEntry *)rbi;
	unsigned int lodMask = (unsigned int)(unsigned char)(unsigned int)lod;
	int count = (int)(unsigned char)numPlyr;

	// NOTE(aalhendi): PSX-backfeed blocker: native C context replaces the retail
	// scratchpad register-save frame at 0x1f800000.
	// TODO(aalhendi): Restore retail table-driven RenderBucket dispatch. Retail
	// copies 0x8008a428/0x8008a444 to scratchpad 0x1f800094/0x1f8000c4 here;
	// native direct dispatch must eventually translate those labels to handlers.

	for (int player = count - 1; player >= 0; player--)
	{
		struct Instance **visInstSrc = cDC[player].visInstSrc;

		if (visInstSrc == 0)
			continue;

		for (; *visInstSrc != 0; visInstSrc++)
		{
			entry = RenderBucket_QueueDraw(*visInstSrc, entry, player, lodMask, gameMode1, (struct OTMem *)otMem);
		}
	}

	return entry;
}

void *RenderBucket_QueueNonLevInstances(struct Item *item, u_long *otMem, void *rbi, char *lod, char numPlyr, int gameMode1)
{
	struct RenderBucketEntry *entry = (struct RenderBucketEntry *)rbi;
	unsigned int lodMask = (unsigned int)(unsigned char)(unsigned int)lod;
	int count = (int)(unsigned char)numPlyr;

	// NOTE(aalhendi): PSX-backfeed blocker: native C context replaces the retail
	// scratchpad register-save frame at 0x1f800000.
	// TODO(aalhendi): Restore retail table-driven RenderBucket dispatch. Retail
	// copies 0x8008a428/0x8008a444 to scratchpad 0x1f800094/0x1f8000c4 here;
	// native direct dispatch must eventually translate those labels to handlers.

	for (int player = count - 1; player >= 0; player--)
	{
		for (struct Item *curr = item; curr != 0; curr = curr->next)
		{
			entry = RenderBucket_QueueDraw((struct Instance *)curr, entry, player, lodMask, gameMode1, (struct OTMem *)otMem);
		}
	}

	return entry;
}

void RenderBucket_UncompressAnimationFrame(struct RenderBucketDrawContext *ctx, u_short stackIndex)
{
	if ((ctx->anim != 0) && (ctx->anim->ptrDeltaArray != 0))
	{
		u_int temporal = ctx->anim->ptrDeltaArray[ctx->vertexIndex];
		u_char xBits = (temporal >> 6) & 7;
		u_char yBits = (temporal >> 3) & 7;
		u_char zBits = temporal & 7;
		u_char bx = (temporal >> 0x19) << 1;
		u_char by = (temporal << 7) >> 0x18;
		u_char bz = (temporal << 0xf) >> 0x18;

		// NOTE(aalhendi): PSX-backfeed blocker: retail consumes command state
		// from registers and scratchpad. This native form keeps the same delta
		// decode semantics with explicit C context.
		if (xBits == 7)
			ctx->x_alu = 0;
		if (yBits == 7)
			ctx->y_alu = 0;
		if (zBits == 7)
			ctx->z_alu = 0;

		ctx->x_alu += RenderBucket_GetSignedBits((unsigned int *)ctx->vertData, &ctx->bitIndex, xBits + 1) + bx;
		ctx->y_alu += RenderBucket_GetSignedBits((unsigned int *)ctx->vertData, &ctx->bitIndex, yBits + 1) + by;
		ctx->z_alu += RenderBucket_GetSignedBits((unsigned int *)ctx->vertData, &ctx->bitIndex, zBits + 1) + bz;

		ctx->stack[stackIndex].x = ctx->x_alu;
		ctx->stack[stackIndex].y = ctx->z_alu;
		ctx->stack[stackIndex].z = ctx->y_alu;
	}
	else
	{
		RenderBucketCompVertex *ptrVerts = (RenderBucketCompVertex *)ctx->vertData;

		ctx->stack[stackIndex].x = ptrVerts[ctx->vertexIndex].x;
		ctx->stack[stackIndex].y = ptrVerts[ctx->vertexIndex].y;
		ctx->stack[stackIndex].z = ptrVerts[ctx->vertexIndex].z;
	}
}

static u_long *RenderBucket_GetNormalOTEntry(struct RenderBucketDrawContext *ctx, int otZ)
{
	int depthBin = otZ >> 5;

	if (ctx->idpp->unkE4 == 0)
		return 0;

	if (depthBin < ctx->idpp->depthOffset[0])
		return 0;

	if (depthBin > ctx->idpp->depthOffset[1])
		return 0;

	// NOTE(aalhendi): PSX-backfeed blocker: retail DrawInstPrim_Normal indexes
	// scratchpad active OT base 0x38 with scratchpad depth 0x2c >> 17 at
	// 0x8006ad88-0x8006ad98. Native consumes the QueueDraw-produced IDPP
	// normal range explicitly until the register/scratchpad entry ABI is
	// restored.
	return (u_long *)ctx->idpp->unkE4 + depthBin;
}

int RenderBucket_DrawInstPrim_Normal(struct RenderBucketDrawContext *ctx, u_int command)
{
	short posWorld1[4];
	short posWorld2[4];
	short posWorld3[4];
	u_short flags = (command >> 24) & 0xff;
	u_short texIndex = command & 0x1ff;
	u_long *otEntry;
	int boolPassCull;
	int otZ;

	// NOTE(aalhendi): PSX-backfeed blocker: retail DrawInstPrim_Normal is a
	// register-entry leaf. Native uses explicit context until the register and
	// scratchpad protocol is audited symbol-by-symbol.
	posWorld1[0] = ctx->mf->pos[0] + ctx->tempCoords[1].x;
	posWorld1[1] = ctx->mf->pos[1] + ctx->tempCoords[1].z;
	posWorld1[2] = ctx->mf->pos[2] + ctx->tempCoords[1].y;
	posWorld1[3] = 0;
	gte_ldv0(&posWorld1[0]);

	posWorld2[0] = ctx->mf->pos[0] + ctx->tempCoords[2].x;
	posWorld2[1] = ctx->mf->pos[1] + ctx->tempCoords[2].z;
	posWorld2[2] = ctx->mf->pos[2] + ctx->tempCoords[2].y;
	posWorld2[3] = 0;
	gte_ldv1(&posWorld2[0]);

	posWorld3[0] = ctx->mf->pos[0] + ctx->tempCoords[3].x;
	posWorld3[1] = ctx->mf->pos[1] + ctx->tempCoords[3].z;
	posWorld3[2] = ctx->mf->pos[2] + ctx->tempCoords[3].y;
	posWorld3[3] = 0;
	gte_ldv2(&posWorld3[0]);

	gte_rtpt();

	boolPassCull = ((flags & 0x10) == 0);
	if (!boolPassCull)
	{
		int opZ;

		gte_nclip();
		gte_stopz(&opZ);
		boolPassCull = (opZ >= 0);

		if ((flags & 0x20) != 0)
			boolPassCull = !boolPassCull;

		if ((ctx->inst->flags & REVERSE_CULL_DIRECTION) != 0)
			boolPassCull = !boolPassCull;
	}

	if (!boolPassCull)
		return 0;

	gte_avsz3();
	gte_stotz(&otZ);

	otEntry = RenderBucket_GetNormalOTEntry(ctx, otZ);
	if (otEntry == 0)
		return 0;

	if ((char *)ctx->primMem->curr + sizeof(POLY_GT3) >= (char *)ctx->primMem->endMin100)
		return -1;

	if (texIndex == 0)
	{
		POLY_G3 *p = ctx->primMem->curr;

		*(int *)&p->r0 = ctx->tempColor[1];
		*(int *)&p->r1 = ctx->tempColor[2];
		*(int *)&p->r2 = ctx->tempColor[3];
		setPolyG3(p);
		gte_stsxy3(&p->x0, &p->x1, &p->x2);
#ifdef CTR_INTERNAL
		if (CtrTireDebug_ShouldLog(CTR_TIREDBG_RENDERBUCKET_PRIM) != 0)
		{
			fprintf(stderr, "[TIREDBG][rb-prim] kind=G3 frame=%d level=%d inst=%p flags=%08x cmd=%08x code=%02x rgb0=%02x,%02x,%02x ot=%p\n",
			        sdata->gGT != 0 ? sdata->gGT->framesInThisLEV : -1, sdata->gGT != 0 ? sdata->gGT->levelID : -1, (void *)ctx->inst, ctx->idpp->instFlags,
			        command, p->code, p->r0, p->g0, p->b0, (void *)otEntry);
		}
#endif
		AddPrim(otEntry, p);
		ctx->primMem->curr = p + 1;
	}
	else
	{
		struct TextureLayout *tex;
		POLY_GT3 *p;

		if (ctx->mh->ptrTexLayout == 0)
			return 0;

		tex = ctx->mh->ptrTexLayout[texIndex - 1];
		if (tex == 0)
			return 0;

		p = ctx->primMem->curr;
		*(int *)&p->r0 = ctx->tempColor[1];
		*(int *)&p->r1 = ctx->tempColor[2];
		*(int *)&p->r2 = ctx->tempColor[3];
		*(int *)&p->u0 = *(int *)&tex->u0;
		*(int *)&p->u1 = *(int *)&tex->u1;
		*(short *)&p->u2 = *(short *)&tex->u2;
		setPolyGT3(p);
		gte_stsxy3(&p->x0, &p->x1, &p->x2);
#ifdef CTR_INTERNAL
		if (CtrTireDebug_ShouldLog(CTR_TIREDBG_RENDERBUCKET_PRIM) != 0)
		{
			fprintf(stderr,
			        "[TIREDBG][rb-prim] kind=GT3 frame=%d level=%d inst=%p flags=%08x cmd=%08x code=%02x rgb0=%02x,%02x,%02x rgb1=%02x,%02x,%02x "
			        "rgb2=%02x,%02x,%02x tpage=%04x blend=%d clut=%04x tex=%u ot=%p\n",
			        sdata->gGT != 0 ? sdata->gGT->framesInThisLEV : -1, sdata->gGT != 0 ? sdata->gGT->levelID : -1, (void *)ctx->inst, ctx->idpp->instFlags,
			        command, p->code, p->r0, p->g0, p->b0, p->r1, p->g1, p->b1, p->r2, p->g2, p->b2, p->tpage, (p->tpage >> 5) & 3, p->clut, texIndex,
			        (void *)otEntry);
		}
#endif
		AddPrim(otEntry, p);
		ctx->primMem->curr = p + 1;
	}

	return 0;
}

void RenderBucket_DrawFunc_Normal(struct RenderBucketDrawContext *ctx)
{
	u_int *pCmd;

	// NOTE(aalhendi): Native C command-list traversal for the normal
	// RenderBucket draw function. It has the named retail boundary, but the
	// exact branch/register choreography remains a pending ASM audit.
	pCmd = (u_int *)ctx->mh->ptrCommandList;
	pCmd++;

	for (; *pCmd != 0xffffffff; pCmd++, ctx->stripLength++)
	{
		u_int command = *pCmd;
		u_short flags = (command >> 24) & 0xff;
		u_short stackIndex = (command >> 16) & 0xff;
		u_short colorIndex = (command >> 9) & 0x7f;

		if ((flags & 4) == 0)
		{
			RenderBucket_UncompressAnimationFrame(ctx, stackIndex);
			ctx->vertexIndex++;
		}

		ctx->tempCoords[0] = ctx->tempCoords[1];
		ctx->tempCoords[1] = ctx->tempCoords[2];
		ctx->tempCoords[2] = ctx->tempCoords[3];
		ctx->tempCoords[3] = ctx->stack[stackIndex];

		ctx->tempColor[0] = ctx->tempColor[1];
		ctx->tempColor[1] = ctx->tempColor[2];
		ctx->tempColor[2] = ctx->tempColor[3];
		ctx->tempColor[3] = ctx->mh->ptrColors[colorIndex];

		if ((flags & 0x40) != 0)
		{
			ctx->tempCoords[1] = ctx->tempCoords[0];
			ctx->tempColor[1] = ctx->tempColor[0];
		}

		if ((flags & 0x80) != 0)
			ctx->stripLength = 0;

		if (ctx->stripLength < 2)
			continue;

		if (RenderBucket_DrawInstPrim_Normal(ctx, command) < 0)
			return;
	}
}

static int RenderBucket_PrepareDrawContext(struct RenderBucketDrawContext *ctx, struct Instance *inst, struct Instance *instPlayerBase, struct PrimMem *primMem)
{
	struct InstDrawPerPlayer *idpp;
	struct PushBuffer *pb;
	struct ModelHeader *mh;
	struct ModelFrame *mf;
	struct ModelAnim *anim;

	if (inst == 0)
		return 0;

	if (inst->model == 0)
		return 0;

	idpp = INST_GETIDPP(instPlayerBase);
	pb = idpp->pushBuffer;
	if (pb == 0)
		return 0;

	if ((idpp->instFlags & 0x40) == 0)
		return 0;

	if ((idpp->instFlags & 0x80) != 0)
		return 0;

	mh = idpp->mh;
	if (mh == 0)
		return 0;

	if ((mh->ptrCommandList == 0) || (mh->ptrColors == 0))
		return 0;

	mf = idpp->ptrCurrFrame;
	if (mf == 0)
		return 0;

	anim = RenderBucket_GetAnim(inst, mh);

	gte_SetRotMatrix(&idpp->mvp);
	gte_SetTransMatrix(&idpp->mvp);
	gte_SetGeomOffset(pb->rect.w >> 1, pb->rect.h >> 1);
	gte_SetGeomScreen(pb->distanceToScreen_PREV);

	if ((inst->flags & 0x400) != 0)
		return 0;

	ctx->inst = inst;
	ctx->idpp = idpp;
	ctx->pb = pb;
	ctx->primMem = primMem;
	ctx->mh = mh;
	ctx->mf = mf;
	ctx->anim = anim;
	ctx->vertData = MODELFRAME_GETVERT(mf);
	return 1;
}

void RenderBucket_Execute(void *param_1, struct PrimMem *param_2)
{
	struct RenderBucketEntry *entry = (struct RenderBucketEntry *)param_1;

	// NOTE(aalhendi): Native C implementation of the RenderBucket execution
	// contract. Full scratchpad/register ASM audit is still pending.
	for (; entry->inst != 0; entry++)
	{
		struct RenderBucketDrawContext ctx = {0};

		if (RenderBucket_PrepareDrawContext(&ctx, entry->inst, entry->instPlayerBase, param_2) == 0)
			continue;

		RenderBucket_DrawFunc_Normal(&ctx);
	}
}
