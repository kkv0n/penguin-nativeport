#include <common.h>

static void DrawLevelOvr2P_CopyClipRecordJumpTable(void);

CTR_STATIC_ASSERT(sizeof(struct OverlayRDATA_227) == 0x564);
CTR_STATIC_ASSERT(sizeof(struct DrawLevelOvrBucketSetupRecord) == 0x64);
CTR_STATIC_ASSERT(sizeof(((struct OverlayRDATA_227 *)0)->scratchInitTable) == 0x60);
CTR_STATIC_ASSERT(sizeof(((struct OverlayRDATA_227 *)0)->clipRecordJumpTable) == 0x60);

static u32 DrawLevelOvr2P_TranslateCopiedLabel(u32 address)
{
	for (s32 bucketIndex = 0; bucketIndex < OVR227_BUCKET_COUNT; bucketIndex++)
	{
		for (s32 copyWordIndex = 0; copyWordIndex < OVR227_SETUP_COPY0_WORD_COUNT; copyWordIndex++)
		{
			if (R227.bucketSetups[bucketIndex].copy0[copyWordIndex] == address)
			{
				return R226.bucketSetups[bucketIndex].copy0[copyWordIndex];
			}
		}

		for (s32 copyWordIndex = 0; copyWordIndex < OVR227_SETUP_COPY1_WORD_COUNT; copyWordIndex++)
		{
			if (R227.bucketSetups[bucketIndex].copy1[copyWordIndex] == address)
			{
				return R226.bucketSetups[bucketIndex].copy1[copyWordIndex];
			}
		}
	}

	for (s32 jumpWordIndex = 0; jumpWordIndex < OVR227_CLIP_RECORD_JUMP_WORD_COUNT; jumpWordIndex++)
	{
		if (R227.clipRecordJumpTable[jumpWordIndex] == address)
		{
			return R226.clipRecordJumpTable[jumpWordIndex];
		}
	}

	return address;
}

static void DrawLevelOvr2P_CopyScratchWordsTranslated(const u32 *source, const struct DrawLevelOvrBucketSetupCopy *copy)
{
	u32 *scratch = CTR_SCRATCHPAD_PTR(u32, copy->scratchOffset);

	for (u32 scratchWordIndex = 0; scratchWordIndex <= copy->lastWordIndex; scratchWordIndex++)
	{
		scratch[scratchWordIndex] = DrawLevelOvr2P_TranslateCopiedLabel(source[scratchWordIndex]);
	}
}

static const struct DrawLevelOvrBucketSetupRecord *DrawLevelOvr2P_FindBucketSetupRecord(u32 setupAddress)
{
	for (s32 bucketIndex = 0; bucketIndex < OVR227_BUCKET_COUNT; bucketIndex++)
	{
		u32 recordAddress = OVR227_RDATA_BUCKET_SETUP_BASE + (u32)(bucketIndex * sizeof(R227.bucketSetups[0]));

		if (recordAddress == setupAddress)
		{
			return &R227.bucketSetups[bucketIndex];
		}
	}

	return NULL;
}

static void DrawLevelOvr2P_ApplyBucketSetup(u32 setupAddress, u32 handlerAddress)
{
	const struct DrawLevelOvrBucketSetupRecord *setup = DrawLevelOvr2P_FindBucketSetupRecord(setupAddress);

	if (setup == NULL)
	{
		DrawLevelOvr2P_Scratch()->currentHandlerAddress = handlerAddress;
		return;
	}

	DrawLevelOvr2P_CopyScratchWordsTranslated(setup->copy0, &setup->copies[0]);
	DrawLevelOvr2P_CopyScratchWordsTranslated(setup->copy1, &setup->copies[1]);
	DrawLevelOvr2P_Scratch()->currentHandlerAddress = handlerAddress;
}

static void DrawLevelOvr2P_CopyScratchInitTable(void)
{
	u32 *scratch = CTR_SCRATCHPAD_PTR(u32, DRAW_LEVEL_OVR_SCRATCH_INIT_TABLE_OFFSET);

	for (s32 scratchWordIndex = 0; scratchWordIndex < OVR227_SCRATCH_INIT_WORD_COUNT; scratchWordIndex++)
	{
		scratch[scratchWordIndex] = R227.scratchInitTable[scratchWordIndex];
	}
}

static const struct DrawLevelOvrBucket *DrawLevelOvr2P_FindBucketByHandler(u32 handlerAddress)
{
	for (s32 bucketIndex = 0; bucketIndex < OVR227_BUCKET_COUNT; bucketIndex++)
	{
		if (R227.bucketHandlerAddresses[bucketIndex] == handlerAddress)
		{
			return &sDrawLevelOvr2PBuckets[bucketIndex];
		}
	}

	return NULL;
}

static int DrawLevelOvr2P_DispatchBucketHandler(u32 handlerAddress, void *bucketValue, struct PushBuffer *pb, struct mesh_info *mesh, struct PrimMem *primMem,
                                                const int *visFaceList)
{
	const struct DrawLevelOvrBucket *bucket = DrawLevelOvr2P_FindBucketByHandler(handlerAddress);

	if (bucket == NULL)
	{
		return 0;
	}

	if (bucket->kind == DRAW_LEVEL_OVR_BUCKET_QUADBLOCKS_RENDERED)
	{
		return DrawLevelOvr2P_DrawRenderedQuadBlocks((struct QuadBlock **)bucketValue, pb, mesh, primMem, bucket->role);
	}

	if (bucket->role == DRAW_LEVEL_OVR_BUCKET_FULL_DYNAMIC_LIST)
	{
		return Ovr227_800a1010_DrawFullDynamicBspList((struct VisMemBspListNode *)bucketValue, pb, mesh, primMem, visFaceList);
	}

	if (bucket->role == DRAW_LEVEL_OVR_BUCKET_WATER_LIST)
	{
		return Ovr227_800a1f34_DrawWaterBspList((struct VisMemBspListNode *)bucketValue, pb, mesh, primMem, visFaceList);
	}

	switch (bucket->role)
	{
	case DRAW_LEVEL_OVR_BUCKET_4X1_LIST:
	case DRAW_LEVEL_OVR_BUCKET_4X2_LIST:
	case DRAW_LEVEL_OVR_BUCKET_DYNAMIC_LIST:
	case DRAW_LEVEL_OVR_BUCKET_4X4_LIST:
		return DrawLevelOvr2P_DrawBspListQuadBlocks((struct VisMemBspListNode *)bucketValue, pb, mesh, primMem, visFaceList, bucket->role);

	default:
		return 0;
	}
}

static int DrawLevelOvr2P_DrawViewportBucket(struct DrawLevelOvrRenderList *renderList, s32 renderListOffset, struct PushBuffer *pb, struct mesh_info *mesh,
                                             struct PrimMem *primMem, const int *visFaceList, u8 **clipCursor, int playerIndex, int applySetup)
{
	u32 bucketIndex = (u32)renderListOffset / sizeof(u32);
	const struct DrawLevelOvrBucket *bucket = &sDrawLevelOvr2PBuckets[bucketIndex];
	void *bucketValue = DrawLevelOvr2P_GetRenderListBucketValue(renderList, bucket);
	u32 setupAddress = R227.bucketSetupAddresses[bucketIndex];
	u32 handlerAddress = R227.bucketHandlerAddresses[bucketIndex];
	struct QuadBlock **renderedOverflowBase = (struct QuadBlock **)data.ptrRenderedQuadblockDestination_forEachPlayer[playerIndex];

	if (bucketValue == NULL)
	{
		DrawLevelOvr2P_ClearRenderedOverflowBase(playerIndex);
		return 1;
	}

	if (applySetup)
	{
		DrawLevelOvr2P_ApplyBucketSetup(setupAddress, handlerAddress);
	}

	DrawLevelOvr2P_SetViewportScratchContext(pb, visFaceList, data.PtrClipBuffer[playerIndex], *clipCursor, renderedOverflowBase);
	if (!DrawLevelOvr2P_DispatchBucketHandler(handlerAddress, bucketValue, pb, mesh, primMem, visFaceList))
	{
		return 0;
	}

	*clipCursor = DrawLevelOvr2P_GetClipRecordCursor();
	return 1;
}

static int DrawLevelOvr2P_DispatchBucketTable(struct DrawLevelOvrRenderList *renderLists, struct PushBuffer *pushBuffers, struct mesh_info *mesh,
                                              struct PrimMem *primMem, const int *visFaceList0, const int *visFaceList1, u8 **clipCursors)
{
	for (s32 renderListOffset = DRAW_LEVEL_OVR_RENDER_LIST_OFFSET_FULL_DYNAMIC_LIST; renderListOffset >= 0; renderListOffset -= (s32)sizeof(u32))
	{
		u32 bucketIndex = (u32)renderListOffset / sizeof(u32);
		const struct DrawLevelOvrBucket *bucket = &sDrawLevelOvr2PBuckets[bucketIndex];
		void *viewport0BucketValue = DrawLevelOvr2P_GetRenderListBucketValue(&renderLists[0], bucket);

		DrawLevelOvr2P_Scratch()->currentBucketOffset = (u32)renderListOffset;

		if (!DrawLevelOvr2P_DrawViewportBucket(&renderLists[0], renderListOffset, &pushBuffers[0], mesh, primMem, visFaceList0, &clipCursors[0], 0, 1))
		{
			return 0;
		}

		if (!DrawLevelOvr2P_DrawViewportBucket(&renderLists[1], renderListOffset, &pushBuffers[1], mesh, primMem, visFaceList1, &clipCursors[1], 1,
		                                       viewport0BucketValue == NULL))
		{
			return 0;
		}
	}

	return 1;
}

static int DrawLevelOvr2P_ConsumeClipRecordsForViewport(struct PushBuffer *pb, struct PrimMem *primMem, u8 *clipCursor, int playerIndex)
{
	u8 *start = data.PtrClipBuffer[playerIndex];

	DrawLevelOvr2P_SetClipRecordStart(start);
	DrawLevelOvr2P_SetClipRecordCursor(clipCursor);
	DrawLevelOvr2P_CopyClipRecordJumpTable();
	return DrawLevelOvr2P_ConsumeClipRecords(pb, primMem);
}

void DrawLevelOvr2P(void *LevRenderList, struct PushBuffer *pb, struct BSP *bspList, struct PrimMem *primMem, const int *visFaceList0, const int *visFaceList1,
                    const struct TextureLayout *waterEnvMap)
{
	struct DrawLevelOvrRenderList *renderLists = LevRenderList;
	struct mesh_info *mesh = (struct mesh_info *)bspList;
	u8 *clipCursors[2] = {data.PtrClipBuffer[0], data.PtrClipBuffer[1]};
	u32 hostStackAnchor;

	// NOTE(aalhendi): ASM-audited against NTSC-U 926 227 entry/setup
	// 0x800a0cbc-0x800a1010. Native keeps explicit host pointers while
	// preserving the retail scratch ownership and two-viewport ordering.
	DrawLevelOvr2P_Scratch()->savedStackPtr32 = (u32)(uintptr_t)&hostStackAnchor;
	DrawLevelOvr2P_Scratch()->primMemEndPtr32 = (u32)(uintptr_t)primMem->end;
	DrawLevelOvr2P_Scratch()->visFaceListArgPtr32[0] = (u32)(uintptr_t)visFaceList0;
	DrawLevelOvr2P_Scratch()->visFaceListArgPtr32[1] = (u32)(uintptr_t)visFaceList1;

	if ((visFaceList0 == NULL) || (visFaceList1 == NULL))
	{
		return;
	}

	DrawLevelOvr2P_Scratch()->waterEnvMapPtr32 = (u32)(uintptr_t)waterEnvMap;

	if (mesh->ptrQuadBlockArray == NULL)
	{
		return;
	}

	// NOTE: retail 0x800a0d24 stores waterEnvMap in the branch delay slot, so it
	// lands even on the empty-mesh exit, but the push-buffer and clip-cursor
	// words are only written past the test (0x800a0d2c and 0x800a0d48).
	DrawLevelOvr2P_Scratch()->pushBufferPtr32[0] = (u32)(uintptr_t)&pb[0];
	DrawLevelOvr2P_Scratch()->pushBufferPtr32[1] = (u32)(uintptr_t)&pb[1];
	DrawLevelOvr2P_Scratch()->playerClipCursorPtr32[0] = (u32)(uintptr_t)clipCursors[0];
	DrawLevelOvr2P_Scratch()->playerClipCursorPtr32[1] = (u32)(uintptr_t)clipCursors[1];

	DrawLevelOvr2P_SetListHandlersSeedRenderedCursor(0);
	Ovr227_800a0d50_ClearProjectedScratch();
	DrawLevelOvr2P_CopyScratchInitTable();
	DrawLevelOvr2P_Scratch()->renderListPtr32 = (u32)(uintptr_t)LevRenderList;

	if (!DrawLevelOvr2P_DispatchBucketTable(renderLists, pb, mesh, primMem, visFaceList0, visFaceList1, clipCursors))
	{
		return;
	}

	DrawLevelOvr2P_Scratch()->playerClipCursorPtr32[0] = (u32)(uintptr_t)clipCursors[0];
	DrawLevelOvr2P_Scratch()->playerClipCursorPtr32[1] = (u32)(uintptr_t)clipCursors[1];

	if (!DrawLevelOvr2P_ConsumeClipRecordsForViewport(&pb[0], primMem, clipCursors[0], 0))
	{
		return;
	}

	if (!DrawLevelOvr2P_ConsumeClipRecordsForViewport(&pb[1], primMem, clipCursors[1], 1))
	{
		return;
	}
}

static void DrawLevelOvr2P_CopyClipRecordJumpTable(void)
{
	u32 *clipRecordJumpTable = CTR_SCRATCHPAD_PTR(u32, DRAW_LEVEL_OVR_GT3_CLIP_RECORD_JUMP_TABLE_OFFSET);

	for (s32 jumpWordIndex = 0; jumpWordIndex < OVR227_CLIP_RECORD_JUMP_WORD_COUNT; jumpWordIndex++)
	{
		clipRecordJumpTable[jumpWordIndex] = DrawLevelOvr2P_TranslateCopiedLabel(R227.clipRecordJumpTable[jumpWordIndex]);
	}
}
