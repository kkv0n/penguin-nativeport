// On-screen PS1-style touch controls for Android. Geometry is generated as
// untextured alpha-blended triangles each frame (positions derive from the
// live window size, so rotation/resizes just work) and drawn through
// NativeRenderer_DrawOverlayTriangles. Hit-testing re-evaluates every active
// finger against every button, so multi-touch and sliding between zones
// behave like a real pad.

#ifdef __ANDROID__

#include "platform/native_touch.h"

#include "platform/native_input.h"
#include "platform/native_renderer.h"

#include <math.h>

extern int g_windowWidth;
extern int g_windowHeight;

#define NATIVE_TOUCH_MAX_FINGERS  10
#define NATIVE_TOUCH_MAX_VERTS    4096
#define NATIVE_TOUCH_CIRCLE_SEGS  20

// PSX pad bits (active low), matching native_input.c
#define TOUCH_PAD_SELECT   0x0001
#define TOUCH_PAD_START    0x0008
#define TOUCH_PAD_UP       0x0010
#define TOUCH_PAD_RIGHT    0x0020
#define TOUCH_PAD_DOWN     0x0040
#define TOUCH_PAD_LEFT     0x0080
#define TOUCH_PAD_L2       0x0100
#define TOUCH_PAD_R2       0x0200
#define TOUCH_PAD_L1       0x0400
#define TOUCH_PAD_R1       0x0800
#define TOUCH_PAD_TRIANGLE 0x1000
#define TOUCH_PAD_CIRCLE   0x2000
#define TOUCH_PAD_CROSS    0x4000
#define TOUCH_PAD_SQUARE   0x8000

struct NativeTouchFinger
{
	SDL_FingerID id;
	float x; // pixels
	float y;
	int active;
};

struct NativeTouchButton
{
	float cx;
	float cy;
	float hw; // half extents of the touch zone
	float hh;
	int round; // circle-ish zone (use radial test)
	u16 bit;
};

enum
{
	TB_UP,
	TB_DOWN,
	TB_LEFT,
	TB_RIGHT,
	TB_TRIANGLE,
	TB_CROSS,
	TB_SQUARE,
	TB_CIRCLE,
	TB_L1,
	TB_L2,
	TB_R1,
	TB_R2,
	TB_START,
	TB_SELECT,
	TB_COUNT
};

global_variable struct NativeTouchFinger s_fingers[NATIVE_TOUCH_MAX_FINGERS];
global_variable struct NativeTouchButton s_buttons[TB_COUNT];
global_variable u16 s_touchPadMask = 0xffff;

// interleaved x,y,r,g,b,a
global_variable float s_verts[NATIVE_TOUCH_MAX_VERTS * 6];
global_variable int s_vertCount = 0;

internal void NativeTouch_ComputeLayout(void)
{
	const float w = (float)g_windowWidth;
	const float h = (float)g_windowHeight;
	const float u = (h < w ? h : w) * 0.01f; // 1% of the short screen edge

	const float dpadCX = 16.0f * u;
	const float dpadCY = h - 18.0f * u;
	const float dpadOfs = 8.5f * u;
	const float faceCX = w - 16.0f * u;
	const float faceCY = h - 18.0f * u;
	const float faceOfs = 8.5f * u;
	const float btnR = 4.6f * u;

	int i;

	for (i = 0; i < TB_COUNT; i++)
	{
		s_buttons[i].round = 1;
		s_buttons[i].hw = btnR * 1.35f; // generous touch zones
		s_buttons[i].hh = btnR * 1.35f;
	}

	s_buttons[TB_UP].cx = dpadCX;
	s_buttons[TB_UP].cy = dpadCY - dpadOfs;
	s_buttons[TB_UP].bit = TOUCH_PAD_UP;
	s_buttons[TB_DOWN].cx = dpadCX;
	s_buttons[TB_DOWN].cy = dpadCY + dpadOfs;
	s_buttons[TB_DOWN].bit = TOUCH_PAD_DOWN;
	s_buttons[TB_LEFT].cx = dpadCX - dpadOfs;
	s_buttons[TB_LEFT].cy = dpadCY;
	s_buttons[TB_LEFT].bit = TOUCH_PAD_LEFT;
	s_buttons[TB_RIGHT].cx = dpadCX + dpadOfs;
	s_buttons[TB_RIGHT].cy = dpadCY;
	s_buttons[TB_RIGHT].bit = TOUCH_PAD_RIGHT;

	s_buttons[TB_TRIANGLE].cx = faceCX;
	s_buttons[TB_TRIANGLE].cy = faceCY - faceOfs;
	s_buttons[TB_TRIANGLE].bit = TOUCH_PAD_TRIANGLE;
	s_buttons[TB_CROSS].cx = faceCX;
	s_buttons[TB_CROSS].cy = faceCY + faceOfs;
	s_buttons[TB_CROSS].bit = TOUCH_PAD_CROSS;
	s_buttons[TB_SQUARE].cx = faceCX - faceOfs;
	s_buttons[TB_SQUARE].cy = faceCY;
	s_buttons[TB_SQUARE].bit = TOUCH_PAD_SQUARE;
	s_buttons[TB_CIRCLE].cx = faceCX + faceOfs;
	s_buttons[TB_CIRCLE].cy = faceCY;
	s_buttons[TB_CIRCLE].bit = TOUCH_PAD_CIRCLE;

	// shoulders: rectangles in the top corners (L2/R2 above L1/R1)
	for (i = TB_L1; i <= TB_R2; i++)
	{
		s_buttons[i].round = 0;
		s_buttons[i].hw = 7.0f * u;
		s_buttons[i].hh = 3.2f * u;
	}
	s_buttons[TB_L2].cx = 9.5f * u;
	s_buttons[TB_L2].cy = 5.0f * u;
	s_buttons[TB_L2].bit = TOUCH_PAD_L2;
	s_buttons[TB_L1].cx = 9.5f * u;
	s_buttons[TB_L1].cy = 12.5f * u;
	s_buttons[TB_L1].bit = TOUCH_PAD_L1;
	s_buttons[TB_R2].cx = w - 9.5f * u;
	s_buttons[TB_R2].cy = 5.0f * u;
	s_buttons[TB_R2].bit = TOUCH_PAD_R2;
	s_buttons[TB_R1].cx = w - 9.5f * u;
	s_buttons[TB_R1].cy = 12.5f * u;
	s_buttons[TB_R1].bit = TOUCH_PAD_R1;

	// start/select: small bars at the bottom centre
	s_buttons[TB_SELECT].round = 0;
	s_buttons[TB_SELECT].hw = 5.5f * u;
	s_buttons[TB_SELECT].hh = 2.4f * u;
	s_buttons[TB_SELECT].cx = w * 0.5f - 8.0f * u;
	s_buttons[TB_SELECT].cy = h - 4.0f * u;
	s_buttons[TB_SELECT].bit = TOUCH_PAD_SELECT;
	s_buttons[TB_START].round = 0;
	s_buttons[TB_START].hw = 5.5f * u;
	s_buttons[TB_START].hh = 2.4f * u;
	s_buttons[TB_START].cx = w * 0.5f + 8.0f * u;
	s_buttons[TB_START].cy = h - 4.0f * u;
	s_buttons[TB_START].bit = TOUCH_PAD_START;
}

internal int NativeTouch_HitButton(const struct NativeTouchButton *btn, float x, float y)
{
	const float dx = x - btn->cx;
	const float dy = y - btn->cy;

	if (btn->round)
	{
		return (dx * dx + dy * dy) <= (btn->hw * btn->hw);
	}
	return (dx >= -btn->hw) && (dx <= btn->hw) && (dy >= -btn->hh) && (dy <= btn->hh);
}

internal void NativeTouch_UpdateMask(void)
{
	u16 mask = 0xffff;
	int f;
	int b;

	NativeTouch_ComputeLayout();

	for (f = 0; f < NATIVE_TOUCH_MAX_FINGERS; f++)
	{
		if (!s_fingers[f].active)
		{
			continue;
		}
		for (b = 0; b < TB_COUNT; b++)
		{
			if (NativeTouch_HitButton(&s_buttons[b], s_fingers[f].x, s_fingers[f].y))
			{
				mask &= (u16)~s_buttons[b].bit;
			}
		}
	}

	s_touchPadMask = mask;
}

void NativeTouch_HandleFingerEvent(const SDL_Event *event)
{
	const SDL_FingerID id = event->tfinger.fingerID;
	const float x = event->tfinger.x * (float)g_windowWidth;
	const float y = event->tfinger.y * (float)g_windowHeight;
	int f;
	int freeSlot = -1;

	for (f = 0; f < NATIVE_TOUCH_MAX_FINGERS; f++)
	{
		if (s_fingers[f].active && (s_fingers[f].id == id))
		{
			break;
		}
		if (!s_fingers[f].active && (freeSlot < 0))
		{
			freeSlot = f;
		}
	}

	switch (event->type)
	{
	case SDL_EVENT_FINGER_DOWN:
		if (f == NATIVE_TOUCH_MAX_FINGERS)
		{
			f = freeSlot;
		}
		if (f >= 0 && f < NATIVE_TOUCH_MAX_FINGERS)
		{
			s_fingers[f].id = id;
			s_fingers[f].x = x;
			s_fingers[f].y = y;
			s_fingers[f].active = 1;
		}
		break;
	case SDL_EVENT_FINGER_MOTION:
		if (f < NATIVE_TOUCH_MAX_FINGERS)
		{
			s_fingers[f].x = x;
			s_fingers[f].y = y;
		}
		break;
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED:
		if (f < NATIVE_TOUCH_MAX_FINGERS)
		{
			s_fingers[f].active = 0;
		}
		break;
	default:
		break;
	}

	NativeTouch_UpdateMask();
}

u16 NativeTouch_GetPadMask(void)
{
	if (Platform_InputAnyGamepadConnected())
	{
		return 0xffff;
	}
	return s_touchPadMask;
}

//--------------------------------------------------------------------------------------------
// overlay geometry

internal void NativeTouch_PushVert(float x, float y, float r, float g, float b, float a)
{
	float *v;

	if (s_vertCount >= NATIVE_TOUCH_MAX_VERTS)
	{
		return;
	}
	v = &s_verts[s_vertCount * 6];
	v[0] = x;
	v[1] = y;
	v[2] = r;
	v[3] = g;
	v[4] = b;
	v[5] = a;
	s_vertCount++;
}

internal void NativeTouch_PushTri(float x0, float y0, float x1, float y1, float x2, float y2, const float *c)
{
	NativeTouch_PushVert(x0, y0, c[0], c[1], c[2], c[3]);
	NativeTouch_PushVert(x1, y1, c[0], c[1], c[2], c[3]);
	NativeTouch_PushVert(x2, y2, c[0], c[1], c[2], c[3]);
}

internal void NativeTouch_PushRect(float cx, float cy, float hw, float hh, const float *c)
{
	NativeTouch_PushTri(cx - hw, cy - hh, cx + hw, cy - hh, cx + hw, cy + hh, c);
	NativeTouch_PushTri(cx - hw, cy - hh, cx + hw, cy + hh, cx - hw, cy + hh, c);
}

internal void NativeTouch_PushLine(float x0, float y0, float x1, float y1, float th, const float *c)
{
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float len = sqrtf(dx * dx + dy * dy);
	float nx;
	float ny;

	if (len < 0.0001f)
	{
		return;
	}
	nx = -dy / len * th * 0.5f;
	ny = dx / len * th * 0.5f;

	NativeTouch_PushTri(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, c);
	NativeTouch_PushTri(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
}

internal void NativeTouch_PushCircle(float cx, float cy, float radius, const float *c)
{
	int i;

	for (i = 0; i < NATIVE_TOUCH_CIRCLE_SEGS; i++)
	{
		const float a0 = (float)i * (6.2831853f / NATIVE_TOUCH_CIRCLE_SEGS);
		const float a1 = (float)(i + 1) * (6.2831853f / NATIVE_TOUCH_CIRCLE_SEGS);

		NativeTouch_PushTri(cx, cy, cx + cosf(a0) * radius, cy + sinf(a0) * radius, cx + cosf(a1) * radius, cy + sinf(a1) * radius, c);
	}
}

internal void NativeTouch_PushRing(float cx, float cy, float rInner, float rOuter, const float *c)
{
	int i;

	for (i = 0; i < NATIVE_TOUCH_CIRCLE_SEGS; i++)
	{
		const float a0 = (float)i * (6.2831853f / NATIVE_TOUCH_CIRCLE_SEGS);
		const float a1 = (float)(i + 1) * (6.2831853f / NATIVE_TOUCH_CIRCLE_SEGS);
		const float c0 = cosf(a0);
		const float s0 = sinf(a0);
		const float c1 = cosf(a1);
		const float s1 = sinf(a1);

		NativeTouch_PushTri(cx + c0 * rInner, cy + s0 * rInner, cx + c0 * rOuter, cy + s0 * rOuter, cx + c1 * rOuter, cy + s1 * rOuter, c);
		NativeTouch_PushTri(cx + c0 * rInner, cy + s0 * rInner, cx + c1 * rOuter, cy + s1 * rOuter, cx + c1 * rInner, cy + s1 * rInner, c);
	}
}

internal float NativeTouch_ButtonAlpha(u16 bit)
{
	return ((s_touchPadMask & bit) == 0) ? 0.75f : 0.38f;
}

void NativeTouch_Render(void)
{
	const float u = ((g_windowHeight < g_windowWidth ? g_windowHeight : g_windowWidth)) * 0.01f;
	const float btnR = 4.6f * u;
	const float sym = btnR * 0.45f;
	const float th = 0.9f * u;
	float base[4] = {0.10f, 0.10f, 0.13f, 0.0f};
	float symCol[4];
	int i;

	if (Platform_InputAnyGamepadConnected())
	{
		return;
	}

	if ((g_windowWidth <= 0) || (g_windowHeight <= 0))
	{
		return;
	}

	NativeTouch_ComputeLayout();
	s_vertCount = 0;

	// button plates
	for (i = 0; i < TB_COUNT; i++)
	{
		base[3] = NativeTouch_ButtonAlpha(s_buttons[i].bit);
		if (s_buttons[i].round)
		{
			NativeTouch_PushCircle(s_buttons[i].cx, s_buttons[i].cy, btnR, base);
		}
		else
		{
			NativeTouch_PushRect(s_buttons[i].cx, s_buttons[i].cy, s_buttons[i].hw, s_buttons[i].hh, base);
		}
	}

	// d-pad arrows (white)
	symCol[0] = 0.85f;
	symCol[1] = 0.85f;
	symCol[2] = 0.88f;
	for (i = TB_UP; i <= TB_RIGHT; i++)
	{
		const struct NativeTouchButton *b = &s_buttons[i];
		const float dxs[4] = {0.0f, 0.0f, -1.0f, 1.0f};
		const float dys[4] = {-1.0f, 1.0f, 0.0f, 0.0f};
		const float dx = dxs[i - TB_UP];
		const float dy = dys[i - TB_UP];

		symCol[3] = NativeTouch_ButtonAlpha(b->bit) + 0.15f;
		NativeTouch_PushTri(b->cx + dx * sym * 1.5f, b->cy + dy * sym * 1.5f, b->cx - dy * sym - dx * sym * 0.4f, b->cy - dx * sym - dy * sym * 0.4f,
		                    b->cx + dy * sym - dx * sym * 0.4f, b->cy + dx * sym - dy * sym * 0.4f, symCol);
	}

	// triangle (green outline)
	{
		const struct NativeTouchButton *b = &s_buttons[TB_TRIANGLE];
		symCol[0] = 0.30f;
		symCol[1] = 0.85f;
		symCol[2] = 0.55f;
		symCol[3] = NativeTouch_ButtonAlpha(b->bit) + 0.15f;
		NativeTouch_PushLine(b->cx, b->cy - sym * 1.2f, b->cx + sym * 1.1f, b->cy + sym * 0.8f, th, symCol);
		NativeTouch_PushLine(b->cx + sym * 1.1f, b->cy + sym * 0.8f, b->cx - sym * 1.1f, b->cy + sym * 0.8f, th, symCol);
		NativeTouch_PushLine(b->cx - sym * 1.1f, b->cy + sym * 0.8f, b->cx, b->cy - sym * 1.2f, th, symCol);
	}

	// cross (blue X)
	{
		const struct NativeTouchButton *b = &s_buttons[TB_CROSS];
		symCol[0] = 0.45f;
		symCol[1] = 0.60f;
		symCol[2] = 0.95f;
		symCol[3] = NativeTouch_ButtonAlpha(b->bit) + 0.15f;
		NativeTouch_PushLine(b->cx - sym, b->cy - sym, b->cx + sym, b->cy + sym, th, symCol);
		NativeTouch_PushLine(b->cx - sym, b->cy + sym, b->cx + sym, b->cy - sym, th, symCol);
	}

	// square (pink outline)
	{
		const struct NativeTouchButton *b = &s_buttons[TB_SQUARE];
		symCol[0] = 0.90f;
		symCol[1] = 0.45f;
		symCol[2] = 0.70f;
		symCol[3] = NativeTouch_ButtonAlpha(b->bit) + 0.15f;
		NativeTouch_PushLine(b->cx - sym, b->cy - sym, b->cx + sym, b->cy - sym, th, symCol);
		NativeTouch_PushLine(b->cx + sym, b->cy - sym, b->cx + sym, b->cy + sym, th, symCol);
		NativeTouch_PushLine(b->cx + sym, b->cy + sym, b->cx - sym, b->cy + sym, th, symCol);
		NativeTouch_PushLine(b->cx - sym, b->cy + sym, b->cx - sym, b->cy - sym, th, symCol);
	}

	// circle (red ring)
	{
		const struct NativeTouchButton *b = &s_buttons[TB_CIRCLE];
		symCol[0] = 0.90f;
		symCol[1] = 0.35f;
		symCol[2] = 0.30f;
		symCol[3] = NativeTouch_ButtonAlpha(b->bit) + 0.15f;
		NativeTouch_PushRing(b->cx, b->cy, sym * 0.75f, sym * 0.75f + th, symCol);
	}

	// shoulder dots: one for L1/R1, two for L2/R2
	symCol[0] = 0.85f;
	symCol[1] = 0.85f;
	symCol[2] = 0.88f;
	for (i = TB_L1; i <= TB_R2; i++)
	{
		const struct NativeTouchButton *b = &s_buttons[i];
		const int two = (i == TB_L2) || (i == TB_R2);

		symCol[3] = NativeTouch_ButtonAlpha(b->bit) + 0.15f;
		if (two)
		{
			NativeTouch_PushCircle(b->cx - 1.2f * u, b->cy, 0.8f * u, symCol);
			NativeTouch_PushCircle(b->cx + 1.2f * u, b->cy, 0.8f * u, symCol);
		}
		else
		{
			NativeTouch_PushCircle(b->cx, b->cy, 0.8f * u, symCol);
		}
	}

	// start: play triangle / select: small square
	{
		const struct NativeTouchButton *b = &s_buttons[TB_START];
		symCol[3] = NativeTouch_ButtonAlpha(b->bit) + 0.15f;
		NativeTouch_PushTri(b->cx - 0.9f * u, b->cy - 1.2f * u, b->cx + 1.3f * u, b->cy, b->cx - 0.9f * u, b->cy + 1.2f * u, symCol);
	}
	{
		const struct NativeTouchButton *b = &s_buttons[TB_SELECT];
		symCol[3] = NativeTouch_ButtonAlpha(b->bit) + 0.15f;
		NativeTouch_PushRect(b->cx, b->cy, 1.1f * u, 1.1f * u, symCol);
	}

	NativeRenderer_DrawOverlayTriangles(s_verts, s_vertCount);
}

#endif // __ANDROID__
