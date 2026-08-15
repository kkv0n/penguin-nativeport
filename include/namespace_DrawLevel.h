#ifndef CTR_NATIVE_NAMESPACE_DRAWLEVEL_H
#define CTR_NATIVE_NAMESPACE_DRAWLEVEL_H

enum DrawLevelOvrRenderListConstant
{
	DRAW_LEVEL_OVR_RENDER_LIST_SLOT_COUNT = 5,
};

struct DrawLevelOvrRenderListSlot
{
	struct QuadBlock **ptrQuadBlocksRendered;
	struct VisMemBspListNode *bspListStart;
};

struct DrawLevelOvrRenderList
{
	struct DrawLevelOvrRenderListSlot list[DRAW_LEVEL_OVR_RENDER_LIST_SLOT_COUNT];
	struct VisMemBspListNode *bspListStart_FullDynamic;
	struct QuadBlock **ptrQuadBlocksRendered_FullDynamic;
};

#define DRAW_LEVEL_OVR_LIST_OFFSET(INDEX, MEMBER)                                                                                 \
	(offsetof(struct DrawLevelOvrRenderList, list) + (INDEX) * sizeof(struct DrawLevelOvrRenderListSlot) +                       \
	 offsetof(struct DrawLevelOvrRenderListSlot, MEMBER))

enum DrawLevelOvrRenderListOffset
{
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_4X4_RENDERED = DRAW_LEVEL_OVR_LIST_OFFSET(0, ptrQuadBlocksRendered),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_4X4_LIST = DRAW_LEVEL_OVR_LIST_OFFSET(0, bspListStart),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_DYNAMIC_RENDERED = DRAW_LEVEL_OVR_LIST_OFFSET(1, ptrQuadBlocksRendered),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_DYNAMIC_LIST = DRAW_LEVEL_OVR_LIST_OFFSET(1, bspListStart),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_4X2_RENDERED = DRAW_LEVEL_OVR_LIST_OFFSET(2, ptrQuadBlocksRendered),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_4X2_LIST = DRAW_LEVEL_OVR_LIST_OFFSET(2, bspListStart),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_4X1_RENDERED = DRAW_LEVEL_OVR_LIST_OFFSET(3, ptrQuadBlocksRendered),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_4X1_LIST = DRAW_LEVEL_OVR_LIST_OFFSET(3, bspListStart),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_WATER_RENDERED = DRAW_LEVEL_OVR_LIST_OFFSET(4, ptrQuadBlocksRendered),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_WATER_LIST = DRAW_LEVEL_OVR_LIST_OFFSET(4, bspListStart),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_FULL_DYNAMIC_LIST = offsetof(struct DrawLevelOvrRenderList, bspListStart_FullDynamic),
	DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_FULL_DYNAMIC_RENDERED = offsetof(struct DrawLevelOvrRenderList, ptrQuadBlocksRendered_FullDynamic),
};

#undef DRAW_LEVEL_OVR_LIST_OFFSET

enum DrawLevelOvrBucketKind
{
	DRAW_LEVEL_OVR_BUCKET_QUADBLOCKS_RENDERED,
	DRAW_LEVEL_OVR_BUCKET_BSP_LIST,
};

enum DrawLevelOvrBucketRole
{
	DRAW_LEVEL_OVR_BUCKET_4X4_RENDERED,
	DRAW_LEVEL_OVR_BUCKET_4X4_LIST,
	DRAW_LEVEL_OVR_BUCKET_DYNAMIC_RENDERED,
	DRAW_LEVEL_OVR_BUCKET_DYNAMIC_LIST,
	DRAW_LEVEL_OVR_BUCKET_4X2_RENDERED,
	DRAW_LEVEL_OVR_BUCKET_4X2_LIST,
	DRAW_LEVEL_OVR_BUCKET_4X1_RENDERED,
	DRAW_LEVEL_OVR_BUCKET_4X1_LIST,
	DRAW_LEVEL_OVR_BUCKET_WATER_RENDERED,
	DRAW_LEVEL_OVR_BUCKET_WATER_LIST,
	DRAW_LEVEL_OVR_BUCKET_FULL_DYNAMIC_LIST,
};

struct DrawLevelOvrBucket
{
	u8 renderListOffset;
	u8 kind;
	u8 role;
	u8 lodMode;
};

CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrRenderListSlot) == 0x8);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrRenderListSlot, ptrQuadBlocksRendered) == 0x0);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrRenderListSlot, bspListStart) == 0x4);
CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrRenderList) == 0x30);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrRenderList, list) == 0x0);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrRenderList, bspListStart_FullDynamic) == 0x28);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrRenderList, ptrQuadBlocksRendered_FullDynamic) == 0x2c);

enum DrawLevelOvrUvScratchSlot
{
	DRAW_LEVEL_OVR_UV_SCRATCH_SLOT_0,
	DRAW_LEVEL_OVR_UV_SCRATCH_SLOT_1,
	DRAW_LEVEL_OVR_UV_SCRATCH_SLOT_2,
};

enum DrawLevelOvrSharedConstant
{
	DRAW_LEVEL_OVR_SPLIT_GROUND_MOSAIC_RELOAD_SPAN = 0xc0,
};

enum DrawLevelOvrCopiedSetupConstant
{
	DRAW_LEVEL_OVR_COPIED_SETUP0_WORD_COUNT = 15,
	DRAW_LEVEL_OVR_COPIED_SETUP1_WORD_COUNT = 3,
	DRAW_LEVEL_OVR_COPIED_SETUP0_LAST_WORD_INDEX = DRAW_LEVEL_OVR_COPIED_SETUP0_WORD_COUNT - 1,
	DRAW_LEVEL_OVR_COPIED_SETUP1_LAST_WORD_INDEX = DRAW_LEVEL_OVR_COPIED_SETUP1_WORD_COUNT - 1,
	DRAW_LEVEL_OVR_COPIED_SETUP0_SCRATCH_OFFSET = 0x14c,
	DRAW_LEVEL_OVR_COPIED_SETUP1_SCRATCH_OFFSET = 0x188,
};

struct DrawLevelOvrBucketSetupCopy
{
	u32 lastWordIndex;
	u32 sourceAddress;
	u32 scratchOffset;
};

struct DrawLevelOvrBucketSetupRecord
{
	struct DrawLevelOvrBucketSetupCopy copies[2];
	u32 padding;
	u32 copy0[DRAW_LEVEL_OVR_COPIED_SETUP0_WORD_COUNT];
	u32 copy1[DRAW_LEVEL_OVR_COPIED_SETUP1_WORD_COUNT];
};

CTR_STATIC_ASSERT(DRAW_LEVEL_OVR_COPIED_SETUP0_WORD_COUNT == 15);
CTR_STATIC_ASSERT(DRAW_LEVEL_OVR_COPIED_SETUP1_WORD_COUNT == 3);
CTR_STATIC_ASSERT(DRAW_LEVEL_OVR_COPIED_SETUP0_LAST_WORD_INDEX == 0xe);
CTR_STATIC_ASSERT(DRAW_LEVEL_OVR_COPIED_SETUP1_LAST_WORD_INDEX == 0x2);
CTR_STATIC_ASSERT(DRAW_LEVEL_OVR_COPIED_SETUP0_SCRATCH_OFFSET == 0x14c);
CTR_STATIC_ASSERT(DRAW_LEVEL_OVR_COPIED_SETUP1_SCRATCH_OFFSET == 0x188);
CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrBucketSetupCopy) == 0xc);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrBucketSetupCopy, lastWordIndex) == 0x0);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrBucketSetupCopy, sourceAddress) == 0x4);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrBucketSetupCopy, scratchOffset) == 0x8);
CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrBucketSetupRecord) == 0x64);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrBucketSetupRecord, copies) == 0x0);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrBucketSetupRecord, padding) == 0x18);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrBucketSetupRecord, copy0) == 0x1c);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrBucketSetupRecord, copy1) == 0x58);


enum DrawLevelOvrScratchOffset
{
	DRAW_LEVEL_OVR_SCRATCH_INIT_TABLE_OFFSET = 0x0ec,
	DRAW_LEVEL_OVR_DIRECT_NEAR_HANDLER_TABLE_OFFSET = 0x148,
	DRAW_LEVEL_OVR_NEAR_SUBDIVISION_HANDLER_TABLE_OFFSET = 0x14c,
	DRAW_LEVEL_OVR_DIRECT_HANDLER_TABLE_OFFSET = 0x184,
	DRAW_LEVEL_OVR_PROJECTED_FRAME0_OFFSET = 0x1b4,
	DRAW_LEVEL_OVR_TERMINAL_CLIP_VERTEX_OFFSET = 0x204,
	DRAW_LEVEL_OVR_GT3_CLIP_RECORD_JUMP_TABLE_OFFSET = 0x240,
	DRAW_LEVEL_OVR_GT4_CLIP_RECORD_JUMP_TABLE_OFFSET = 0x260,
	DRAW_LEVEL_OVR_WATER_RENDERED_SENTINEL_OFFSET = 0x268,
	DRAW_LEVEL_OVR_TERMINAL_RETURN_PC_OFFSET = 0x2a0,
	DRAW_LEVEL_OVR_MOSAIC_SOURCE_INDEX_OFFSET = 0x320,
	DRAW_LEVEL_OVR_DEEPEST_PROJECTED_FRAME_OFFSET = 0x324,
	DRAW_LEVEL_OVR_MOSAIC_SOURCE_BIAS_OFFSET = 0x3d8,
};

struct DrawLevelOvrScratchVertex
{
	union
	{
		SVec3 posVec;
		s16 pos[3];
	};
	u16 flags;

	// NOTE: retail reads byte 0x0b (color_hi[3]) with `lb`, i.e. signed, and it
	// is not a colour channel: it is the clip-byte accumulator (`|=`, `!= 0`,
	// cleared to 0). It stays `u8` on purpose. Channels 0..2 are averaged as
	// unsigned during vertex interpolation and all four bytes are copied as one
	// word, so a signed array would sign-extend colours >= 128 and corrupt the
	// blend. Signedness of byte 3 is unobservable across its three uses.
	u8 color_hi[4];
	union
	{
		SVec2 posScreenVec;
		s16 posScreen[2];
	};
	u16 depth;

	// Retail reads 0x12 and 0x13 only with `lb`, never `lbu`, in all four
	// quadblock overlays (226: 147 reads, 227: 66, 228: 116, 229: 116, and zero
	// `lbu` in any of them), so both bytes are signed.
	s8 clipNear;
	s8 clipHalfNear;
};

struct DrawLevelOvrClipRecordVertex
{
	union
	{
		SVec3 posVec;
		s16 pos[3];
	};
	u16 flags;
	u8 color_hi[4];
};

struct DrawLevelOvrClipRecord
{
	u32 header;
	u32 otEntry;
	s16 tpage;
	s16 clut;
	struct DrawLevelOvrClipRecordVertex vertex[4];
};

struct DrawLevelOvrUvScratch
{
	union
	{
		u32 uv0;
		struct
		{
			union
			{
				u16 uv0Packed;
				s16 flag0;
			};
			s16 clut;
		};
	};
	union
	{
		u32 uv1;
		struct
		{
			union
			{
				u16 uv1Packed;
				s16 flag1;
			};
			s16 tpage;
		};
	};
	union
	{
		u32 uv2;
		struct
		{
			s16 flag2;
			s16 flag3;
		};
	};
	u32 savedUv0;
	u32 savedUv1;
};

struct DrawLevelOvrStableScratch
{
	union
	{
		struct MainRenderLevelGeometryScratch render;
		struct
		{
			u32 playerClipCursorPtr32[4];
			u32 clipCursorPtr32;
		};
	};
	u32 primMemEndPtr32;
	u32 currentBucketOffset;
	u32 savedStackPtr32;
	u8 pad_03c[0x20];
	s32 depthClipThreshold;
	u32 renderListPtr32;
	u32 renderedOverflowPtr32;
	s32 quadCount;
	u32 clipWindowPacked;
	u32 directMask;
	u32 currentHandlerAddress;
	u8 pad_078[0x04];
	u32 drawOrderOrHeader;
	u32 clipRecordHeader;
	u32 mosaicTextureWord;
	u32 waterEnvMapPtr32;
	u8 pad_08c[0x10];
	u32 previousDirectHandlerAddress;
	u8 pad_0a0[0x1c];
	s32 visibilityBitIndex;
	u32 visibilityWordPtr32;
	u32 visibilityWord;
	u32 visFaceListPtr32;
	u32 visFaceListArgPtr32[4];
	u32 pushBufferPtr32[4];
	u8 pad_0ec[0xa8];
	u32 selected4x1TableWord;
	SVec3 projectedCenter;
	u8 pad_19e[0x02];
	struct DrawLevelOvrUvScratch uv;
};

CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrScratchVertex) == 0x14);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, posVec) == 0x00);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, pos) == 0x00);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, flags) == 0x06);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, color_hi) == 0x08);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, posScreenVec) == 0x0c);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, posScreen) == 0x0c);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, depth) == 0x10);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, clipNear) == 0x12);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrScratchVertex, clipHalfNear) == 0x13);
CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrClipRecordVertex) == 0xc);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrClipRecordVertex, posVec) == 0x00);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrClipRecordVertex, pos) == 0x00);
CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrClipRecord) == 0x3c);
CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrUvScratch) == 0x14);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, uv0) == 0x00);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, flag0) == 0x00);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, clut) == 0x02);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, uv1) == 0x04);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, flag1) == 0x04);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, tpage) == 0x06);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, uv2) == 0x08);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, flag2) == 0x08);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, flag3) == 0x0a);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, savedUv0) == 0x0c);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrUvScratch, savedUv1) == 0x10);
CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrStableScratch) == 0x1b4);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, render) == 0x00);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, playerClipCursorPtr32) == 0x00);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, clipCursorPtr32) == 0x10);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, primMemEndPtr32) == 0x30);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, currentBucketOffset) == 0x34);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, savedStackPtr32) == 0x38);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, depthClipThreshold) == 0x5c);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, renderListPtr32) == 0x60);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, renderedOverflowPtr32) == 0x64);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, quadCount) == 0x68);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, clipWindowPacked) == 0x6c);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, directMask) == 0x70);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, currentHandlerAddress) == 0x74);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, drawOrderOrHeader) == 0x7c);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, clipRecordHeader) == 0x80);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, mosaicTextureWord) == 0x84);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, waterEnvMapPtr32) == 0x88);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, previousDirectHandlerAddress) == 0x9c);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, visibilityBitIndex) == 0xbc);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, visibilityWordPtr32) == 0xc0);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, visibilityWord) == 0xc4);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, visFaceListPtr32) == 0xc8);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, visFaceListArgPtr32) == 0xcc);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, pushBufferPtr32) == 0xdc);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, selected4x1TableWord) == 0x194);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, projectedCenter) == 0x198);
CTR_STATIC_ASSERT(offsetof(struct DrawLevelOvrStableScratch, uv) == 0x1a0);

// Descriptor layouts shared by the quadblock overlays (226-229). Only the
// layout is shared: each overlay owns its own tables and its own code.
struct DrawLevelOvrFaceSelector
{
	u32 selector;
	u8 drawOrderShift;
};

struct DrawLevelOvrNearSubdivisionCase
{
	u32 listHandlerAddress;
	u32 renderedHandlerAddress;
	u8 subIndices[2][4];
	u32 directMasks[2];
	u32 slotWords[2];
};

struct DrawLevelOvrFullDynamicRecursiveGate
{
	u32 directMask;
	int forceDirect;
};

#endif
