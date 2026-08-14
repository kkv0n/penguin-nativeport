#include <common.h>

enum ElimBGPauseState
{
	ELIM_BG_PAUSE_STATE_NONE = 0,
	ELIM_BG_PAUSE_STATE_CAPTURE = 1,
	ELIM_BG_PAUSE_STATE_DRAW = 2,
	ELIM_BG_PAUSE_STATE_RESTORE = 3,
};

enum ElimBGVramSlot
{
	ELIM_BG_SLOT_TEXTURE_DB0 = 0,
	ELIM_BG_SLOT_TEXTURE_DB1 = 1,
	ELIM_BG_SLOT_RAW_STRIP_DB0 = 2,
	ELIM_BG_SLOT_RAW_STRIP_DB1 = 3,
	ELIM_BG_SLOT_PACKED_STRIP_DB0 = 4,
	ELIM_BG_SLOT_PACKED_STRIP_DB1 = 5,
};

enum ElimBGConstants
{
	ELIM_BG_TEXTURE_LEFT_X = 0x200,
	ELIM_BG_TEXTURE_RIGHT_X = 0x240,
	ELIM_BG_TEXTURE_BACKUP_W = 0x40,
	ELIM_BG_TEXTURE_BACKUP_H = 0x100,
	// All three slot pairs hang off `primMem.end`: retail computes the texture
	// backup base first and derives the other two by subtracting from it.
	ELIM_BG_TEXTURE_BACKUP_OFFSET = 0x8000,
	ELIM_BG_RAW_STRIP_OFFSET = 0x4000,
	ELIM_BG_PACKED_STRIP_OFFSET = 0x4800,
	ELIM_BG_SCREEN_W = 0x200,
	ELIM_BG_SCREEN_H = 0xd8,
	ELIM_BG_SWAPCHAIN_Y_STRIDE = 0x128,
	ELIM_BG_STRIP_H = 8,
	ELIM_BG_CAPTURE_VRAM_X = 0x200,
	ELIM_BG_CAPTURE_VRAM_W = 0x80,
	ELIM_BG_FINAL_STRIP_Y = 0xff,
	ELIM_BG_FINAL_STRIP_W = 0x10,
	ELIM_BG_CHUNK_SOURCE_PIXELS = 0x1000,
	ELIM_BG_TILE_W = 0x80,
	ELIM_BG_TILE_H = 0x10,
	ELIM_BG_TILE_COLOR = 0x80,
	ELIM_BG_TILE_CLUT = 0x3fe0,
	ELIM_BG_U_LIMIT = 0x100,
	ELIM_BG_VRAM_U_WRAP = 0x80,
};

CTR_STATIC_ASSERT(ELIM_BG_TEXTURE_BACKUP_OFFSET == 0x8000);
CTR_STATIC_ASSERT(ELIM_BG_RAW_STRIP_OFFSET == 0x4000);
CTR_STATIC_ASSERT(ELIM_BG_PACKED_STRIP_OFFSET == 0x4800);
CTR_STATIC_ASSERT(ELIM_BG_CHUNK_SOURCE_PIXELS == 0x1000);

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80024524-0x8002459c.
void ElimBG_SaveScreenshot_Chunk(u16 *packedStrip, u16 *rawStrip, int rawPixelCount)
{
	u16 *rawGroupLast;

	if (rawPixelCount == 0)
	{
		return;
	}

	rawGroupLast = rawStrip + 3;

	// Retail accumulates straight into the destination: four stores per group,
	// not one store of a local.
	do
	{
		*packedStrip = (u16)((rawStrip[0] & 0x3e0) >> 6);
		*packedStrip |= rawGroupLast[-2] >> 2 & 0xf0;
		*packedStrip |= (u16)((rawGroupLast[-1] & 0x3c0) << 2);
		*packedStrip |= (u16)((*rawGroupLast & 0x3c0) << 6);

		rawPixelCount -= 4;
		rawStrip += 4;
		rawGroupLast += 4;
		packedStrip++;
	} while (rawPixelCount != 0);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8002459c-0x8002481c.
void ElimBG_SaveScreenshot_Full(struct GameTracker *gGT)
{
	int bufferIndex;
	int screenTopY;
	int stripY;
	RECT rSrc;
	RECT rDst;

	// Retail keeps the two backup rects as one initialized local array,
	// copied in from .rodata (NTSC-U 926 0x80011064).
	RECT rect[2] = {
	    {ELIM_BG_TEXTURE_LEFT_X, 0, ELIM_BG_TEXTURE_BACKUP_W, ELIM_BG_TEXTURE_BACKUP_H},
	    {ELIM_BG_TEXTURE_RIGHT_X, 0, ELIM_BG_TEXTURE_BACKUP_W, ELIM_BG_TEXTURE_BACKUP_H},
	};

	bufferIndex = 0;

	// vram copy, then overwrite vram with pause image

	// backups of the two VRAM pages overwritten by the pause image
	sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB0] = (char *)gGT->db[0].primMem.end - ELIM_BG_TEXTURE_BACKUP_OFFSET;
	sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB1] = (char *)gGT->db[1].primMem.end - ELIM_BG_TEXTURE_BACKUP_OFFSET;

	// double-buffered raw screenshot strips from VRAM
	sdata->PausePtrsVRAM[ELIM_BG_SLOT_RAW_STRIP_DB0] = sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB0] - ELIM_BG_RAW_STRIP_OFFSET;
	sdata->PausePtrsVRAM[ELIM_BG_SLOT_RAW_STRIP_DB1] = sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB1] - ELIM_BG_RAW_STRIP_OFFSET;

	// double-buffered packed 4bpp pause strips
	sdata->PausePtrsVRAM[ELIM_BG_SLOT_PACKED_STRIP_DB0] = sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB0] - ELIM_BG_PACKED_STRIP_OFFSET;
	sdata->PausePtrsVRAM[ELIM_BG_SLOT_PACKED_STRIP_DB1] = sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB1] - ELIM_BG_PACKED_STRIP_OFFSET;

	gGT->db[0].primMem.end = sdata->PausePtrsVRAM[ELIM_BG_SLOT_PACKED_STRIP_DB0];
	gGT->db[1].primMem.end = sdata->PausePtrsVRAM[ELIM_BG_SLOT_PACKED_STRIP_DB1];

	// copy texture vram into PrimMem
	StoreImage(&rect[0], (u32 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB0]);
	StoreImage(&rect[1], (u32 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB1]);

	// === copy screen into texture vram ===

	screenTopY = gGT->swapchainIndex * ELIM_BG_SWAPCHAIN_Y_STRIDE;

	rSrc.x = 0;
	rSrc.y = screenTopY;
	rSrc.w = ELIM_BG_SCREEN_W;
	rSrc.h = ELIM_BG_STRIP_H;

	// start the first Store
	StoreImage(&rSrc, (u32 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_RAW_STRIP_DB0]);

	for (stripY = 0; stripY + ELIM_BG_STRIP_H < ELIM_BG_SCREEN_H; stripY += ELIM_BG_STRIP_H)
	{
		bufferIndex = 1 - bufferIndex;

		// start next Store, while processing previous store
		rSrc.x = 0;
		rSrc.y = screenTopY + stripY + ELIM_BG_STRIP_H;
		rSrc.w = ELIM_BG_SCREEN_W;
		rSrc.h = ELIM_BG_STRIP_H;

		// pause until Store is done
		DrawSync(0);

		StoreImage(&rSrc, (u32 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_RAW_STRIP_DB0 + bufferIndex]);

		ElimBG_SaveScreenshot_Chunk((u16 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_PACKED_STRIP_DB0 + (1 - bufferIndex)],
		                            (u16 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_RAW_STRIP_DB0 + (1 - bufferIndex)], ELIM_BG_CHUNK_SOURCE_PIXELS);

		rDst.x = ELIM_BG_CAPTURE_VRAM_X;
		rDst.y = stripY;
		rDst.w = ELIM_BG_CAPTURE_VRAM_W;
		rDst.h = ELIM_BG_STRIP_H;
		LoadImage(&rDst, (u32 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_PACKED_STRIP_DB0 + (1 - bufferIndex)]);
	}

	// wait for last Store
	DrawSync(0);

	ElimBG_SaveScreenshot_Chunk((u16 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_PACKED_STRIP_DB0 + bufferIndex],
	                            (u16 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_RAW_STRIP_DB0 + bufferIndex], ELIM_BG_CHUNK_SOURCE_PIXELS);

	rDst.x = ELIM_BG_CAPTURE_VRAM_X;
	rDst.y = stripY;
	rDst.w = ELIM_BG_CAPTURE_VRAM_W;
	rDst.h = ELIM_BG_STRIP_H;
	LoadImage(&rDst, (u32 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_PACKED_STRIP_DB0 + bufferIndex]);

	rDst.x = ELIM_BG_CAPTURE_VRAM_X;
	rDst.y = ELIM_BG_FINAL_STRIP_Y;
	rDst.w = ELIM_BG_FINAL_STRIP_W;
	rDst.h = 1;
	LoadImage(&rDst, &data.pauseScreenStrip[0]);
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8002481c-0x80024840.
void ElimBG_Activate(struct GameTracker *gGT)
{
	sdata->pause_backup_renderFlags = gGT->renderFlags;
	sdata->pause_backup_hudFlags = gGT->hudFlags;
	sdata->pause_state = ELIM_BG_PAUSE_STATE_CAPTURE;
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80024840-0x800248bc.
void ElimBG_ToggleInstance(struct Instance *inst, b32 boolGameIsPaused)
{
	u32 flags;

	// if game is being paused
	if (boolGameIsPaused)
	{
		flags = inst->flags;

		if (!(flags & HIDE_MODEL))
		{
			flags &= ~INVISIBLE_BEFORE_PAUSE;
		}
		else
		{
			flags |= INVISIBLE_BEFORE_PAUSE;
		}

		inst->flags = flags;
		inst->flags |= (INVISIBLE_DURING_PAUSE | HIDE_MODEL);

		return;
	}

	if ((inst->flags & (INVISIBLE_BEFORE_PAUSE | INVISIBLE_DURING_PAUSE)) == INVISIBLE_DURING_PAUSE)
	{
		inst->flags &= ~(INVISIBLE_DURING_PAUSE | HIDE_MODEL);
	}
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800248bc-0x80024974.
void ElimBG_ToggleAllInstances(struct GameTracker *gGT, b32 boolGameIsPaused)
{
	struct Level *lev;
	struct Instance *inst;
	struct InstDef *ptrInstDefs;

	lev = gGT->level1;

	// Loop through all instances in level
	for (ptrInstDefs = &lev->ptrInstDefs[0]; ptrInstDefs < &lev->ptrInstDefs[lev->numInstances]; ptrInstDefs++)
	{
		inst = ptrInstDefs->ptrInstance;

		if (inst != 0)
		{
			ElimBG_ToggleInstance(inst, boolGameIsPaused);
		}
	}

	// Loop through all instances in Instance Pool
	for (inst = (struct Instance *)LIST_GetFirstItem(&gGT->JitPools.instance.taken); inst != 0;
	     inst = (struct Instance *)LIST_GetNextItem((struct Item *)inst))
	{
		ElimBG_ToggleInstance(inst, boolGameIsPaused);
	}
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80024974-0x80024c08.
void ElimBG_HandleState(struct GameTracker *gGT)
{
	int screenXForTpage;
	int textureU;
	POLY_FT4 *p;
	int textureX;
	u16 tpage;
	int textureY;
	int tileX;

	// Same initialized local array as ElimBG_SaveScreenshot_Full
	// (NTSC-U 926 0x80011064).
	RECT rect[2] = {
	    {ELIM_BG_TEXTURE_LEFT_X, 0, ELIM_BG_TEXTURE_BACKUP_W, ELIM_BG_TEXTURE_BACKUP_H},
	    {ELIM_BG_TEXTURE_RIGHT_X, 0, ELIM_BG_TEXTURE_BACKUP_W, ELIM_BG_TEXTURE_BACKUP_H},
	};

	// if this is last frame of pause
	if (sdata->pause_state == ELIM_BG_PAUSE_STATE_RESTORE)
	{
		// load from RAM, back to VRAM
		LoadImage(&rect[0], (u32 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB0]);
		LoadImage(&rect[1], (u32 *)sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB1]);

		DrawSync(0);

		// retail restores from the saved backup base, not from the current end
		gGT->db[0].primMem.end = sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB0] + ELIM_BG_TEXTURE_BACKUP_OFFSET;
		gGT->db[1].primMem.end = sdata->PausePtrsVRAM[ELIM_BG_SLOT_TEXTURE_DB1] + ELIM_BG_TEXTURE_BACKUP_OFFSET;

		// Enable all instances
		ElimBG_ToggleAllInstances(gGT, 0);

		// game is not paused anymore
		sdata->pause_state = ELIM_BG_PAUSE_STATE_NONE;
	}

	// if game is paused, but not on the restore frame
	else if (sdata->pause_state != ELIM_BG_PAUSE_STATE_NONE)
	{
		// if this is the first frame of pause
		if (sdata->pause_state == ELIM_BG_PAUSE_STATE_CAPTURE)
		{
			gGT->renderFlags = (gGT->renderFlags & RENDER_FLAG_CHECKERED_FLAG) | RENDER_FLAG_RENDER_BUCKET;

			sdata->gGT->hudFlags &= HUD_FLAG_PAUSE_SCREENSHOT_MASK;

			ElimBG_SaveScreenshot_Full(gGT);

			// Disable all instances
			// (prevent PrimMem from overwriting VRAM backup)
			ElimBG_ToggleAllInstances(gGT, 1);

			// you are now ready to draw the screenshot
			sdata->pause_state = ELIM_BG_PAUSE_STATE_DRAW;
		}
		// rest of the function is for drawing screenshot
		tileX = 0;
		do
		{
			textureY = 0;
			do
			{
				// backBuffer->primMem.cursor
				p = (POLY_FT4 *)gGT->backBuffer->primMem.cursor;

				// increment primMem by size of primitive
				gGT->backBuffer->primMem.cursor = p + 1;

				setPolyFT4(p);

				// RGB
				setRGB0(p, ELIM_BG_TILE_COLOR, ELIM_BG_TILE_COLOR, ELIM_BG_TILE_COLOR);

				// four (x,y) positions
				setXY4(p, tileX, textureY, tileX + ELIM_BG_TILE_W, textureY, tileX, textureY + ELIM_BG_TILE_H, tileX + ELIM_BG_TILE_W,
				       textureY + ELIM_BG_TILE_H);

				screenXForTpage = tileX;
				if (tileX < 0)
				{
					screenXForTpage = tileX + 3;
				}
				textureX = (screenXForTpage >> 2) + ELIM_BG_TEXTURE_LEFT_X;
				tpage = getTPage(TEXPAGE_COLOR_4BIT, TRANS_50, textureX, textureY);

				// tpage
				p->tpage = tpage;

				// clut
				p->clut = ELIM_BG_TILE_CLUT;

				textureU = (textureX - ((tpage << 6) & 0x3c0)) * 4;

				p->v0 = textureY;
				p->v1 = textureY;

				// u0
				p->u0 = textureU;

				if (textureU + ELIM_BG_VRAM_U_WRAP < ELIM_BG_U_LIMIT)
				{
					// u1
					p->u1 = textureU - ELIM_BG_VRAM_U_WRAP;
				}
				else
				{
					// u1
					p->u1 = 0xff;
				}

				// u2
				textureU = (textureX - ((tpage << 6) & 0x3c0)) * 4;

				p->u2 = textureU;

				if (textureU + ELIM_BG_VRAM_U_WRAP < ELIM_BG_U_LIMIT)
				{
					// u3
					p->u3 = textureU - ELIM_BG_VRAM_U_WRAP;
				}
				else
				{
					// u3
					p->u3 = 0xff;
				}

				// v3 = v0 + 0x10
				textureY += ELIM_BG_TILE_H;
				p->v2 = textureY;
				p->v3 = textureY;

				// pointer to OT mem, and pointer to primitive
				AddPrim(&sdata->gGT->pushBuffer_UI.ptrOT[4], p);

				// while v0 (tex coord Y) < screensize
			} while (textureY < ELIM_BG_SCREEN_H);

			// increment u0
			tileX = tileX + ELIM_BG_TILE_W;

			// while u0 (tex coord X) < screensize
		} while (tileX < ELIM_BG_SCREEN_W);
	}
}


// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80024c08-0x80024c4c.
void ElimBG_Deactivate(struct GameTracker *gGT)
{
	// it's written this way for bytebudget reasons.
	u8 backup = (u8)sdata->pause_backup_hudFlags;

	// if game is paused
	if (sdata->pause_state != ELIM_BG_PAUSE_STATE_NONE)
	{
		// request the one-frame VRAM restore path
		sdata->pause_state = ELIM_BG_PAUSE_STATE_RESTORE;

		gGT->renderFlags = (gGT->renderFlags & RENDER_FLAG_CHECKERED_FLAG) | (sdata->pause_backup_renderFlags & RENDER_FLAG_ALL_EXCEPT_CHECKERED_FLAG_MASK);

		gGT->hudFlags = backup;
	}
}
