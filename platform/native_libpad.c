#include <macros.h>
#include <platform/native_input.h>
#include <psx/libpad.h>

// NOTE(aalhendi): Native libpad preserves behavior from PsyCross's
// MIT-licensed LIBPAD.C while moving host ownership into ctr-native.
// See THIRD_PARTY_NOTICES.md.

s32 g_padCommEnable = 0;

void PadInitMtap(unsigned char *slot1, unsigned char *slot2)
{
	Platform_InputPadInit(0, slot1);
	Platform_InputPadInit(1, slot2);
}

void PadStartCom(void)
{
	g_padCommEnable = 1;
}

void PadStopCom(void)
{
	g_padCommEnable = 0;
}

int PadGetState(int port)
{
	return Platform_InputPadGetState(port);
}

// Retail libpad (SCUS_944.26 0x80075be0): returns the actuator count when acno
// is negative, otherwise byte (term-1) of that actuator's 5-byte info record,
// or 0 for an out-of-range actuator/term.
int PadInfoAct(int port, int acno, int term)
{
	return Platform_InputPadInfoAct(port, acno, term);
}

int PadSetActAlign(int port, unsigned char *table)
{
	(void)port;
	(void)table;
	return 1;
}

// Retail libpad (0x80075a40) queues pad command 44h "Set LED State" and returns
// 1 when the request was accepted, 0 when the port is busy. Returning 0
// unconditionally used to stall GAMEPAD_ProcessState at gamepadType 0, which is
// why PadSetAct was never reached and rumble never ran.
int PadSetMainMode(int socket, int offs, int lock)
{
	return Platform_InputPadSetMainMode(socket, offs, lock);
}

// Retail libpad (0x80075ba0) only latches the pointer and the length; the two
// MOT bytes are then transmitted inside every poll packet, not once per call.
void PadSetAct(int port, unsigned char *table, int len)
{
	Platform_InputPadVibrate(port, table, len);
}
