#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800315ac-0x80031608.
struct Instance *LinkedCollide_Hitbox_Desc(struct HitboxDesc *objBoxDesc)
{
	return LinkedCollide_Hitbox(objBoxDesc->inst, objBoxDesc->thread, objBoxDesc->bucket, objBoxDesc->bbox);
}
