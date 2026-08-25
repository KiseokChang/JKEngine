#include "zerosel.h"

unsigned short ZeroSelector;

void AllocZeroSelector()
{
	asm		{
		mov  cx, 1
		mov  ax, 0000h          // DPMI Allocate Selector
		int  31h
		mov  ZeroSelector, ax

		mov  bx, ax
		lar  cx, bx
		shr  ecx, 8
		or   cx, 4000h          // make it 'big'
		mov  ax, 0009h          // DPMI Set Descriptor Access Rights
		int  31h

		mov  bx, ZeroSelector
		mov  dx, 0FFFFh
		mov  cx, 0000Fh         // map 1M
		mov  ax, 0008h          // DPMI Set Segment Limit
		int  31h

		xor  dx, dx
		xor  cx, cx
		mov  bx, ZeroSelector
		mov  ax, 0007h          // DPMI Set Segment Base Address
		int  31h
	}
}


void FreeZeroSelector()
{
	asm		{
		mov     bx, ZeroSelector
		mov     ax, 0001h               // DPMI Free ZeroSelector
		int     31h
	}
}
