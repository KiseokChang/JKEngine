#include "lowmem.h"

unsigned short lowMemSel;        // 1M selector mapping conventional memory

/*
	Function copyToLow

	memcpy function to copy a block to a specified real mode address from
	a 32-bit flat address.

*/

void copyToLow(unsigned short dst_sel, unsigned short dst_offset, void * src, unsigned int size)
{
	asm {
		xor  edi, edi
		mov  di , dst_sel
		shl  edi, 4
		xor  eax, eax
		mov  ax , dst_offset
		add  edi, eax
		mov  esi, src
		cld
		push es
		mov  es, lowMemSel
		mov  ecx, size
		shr  ecx, 2
		rep     movsd
		mov  ecx, size
		and  ecx, 3
		rep     movsb
		pop  es
	}
}

/*
	Function copyFromLow

	memcpy function to copy a block from a specified real mode address to
	a 32-bit flat address.

*/

void copyFromLow(void * dst, unsigned short src_sel, unsigned short src_offset, unsigned int size)
{
	asm     {
		xor  esi, esi
		mov  si , src_sel
		shl  esi, 4
		xor  eax, eax
		mov  ax , src_offset
		add  esi, eax
		mov  edi, dst
		cld
		push ds
		mov  ds, lowMemSel
		mov  ecx, size
		shr  ecx, 2
		rep     movsd
		mov  ecx, size
		and  ecx, 3
		rep     movsb
		pop  ds
	}
}

/*
	Function getLowMemDword
	Function getLowMemWord
	Function getLowMemByte

	Functions to read a byte/word/dword from real mode memory

*/

unsigned long getLowMemDword(unsigned short sel, unsigned short offset)
{
	unsigned long x;
	copyFromLow(&x,sel,offset, 4);
	return x;
}

unsigned short getLowMemWord(unsigned short sel, unsigned short offset)
{
	unsigned short x;
	copyFromLow(&x,sel,offset, 2);
	return x;
}


unsigned char getLowMemByte(unsigned short sel, unsigned short offset)
{
	unsigned char x;
	copyFromLow(&x,sel,offset, 1);
	return x;
}


/*
	Function setLowMemDword
	Function setLowMemWord
	Function setLowMemByte

	Functions to write a byte/word/dword from real mode memory

*/
void setLowMemDword(unsigned short sel, unsigned short offset, unsigned long d)
{
	copyToLow(sel,offset, &d, 4);
}

void setLowMemWord(unsigned short sel, unsigned short offset, unsigned short w)
{
	copyToLow(sel,offset, &w, 2);
}

void setLowMemByte(unsigned short sel, unsigned short offset, unsigned char b)
{
	copyToLow(sel,offset,&b,1);
}



/*
	Function getSel()
	Function freeSel()

	The startup and cleanup functions will get a selector from DPMI to be
	used for low memory access requests. The selector is set up to map the
	whole first megabyte.  The freeSel function     will free the selector at
	termination time.
*/


static void getSel()
{
	asm	{
		mov  cx, 1
		mov  ax, 0000h          ;; DPMI Allocate Selector
		int  31h
		mov  lowMemSel, ax

		mov  bx, ax
		lar  cx, bx
		shr  ecx, 8
		or   cx, 4000h          ;; make it 'big'
		mov  ax, 0009h          ;; DPMI Set Descriptor Access Rights
		int  31h

		mov  bx, lowMemSel
		mov  dx, 0FFFFh
		mov  cx, 0000Fh         ;; map 1M
		mov  ax, 0008h          ;; DPMI Set Segment Limit
		int  31h

		xor  dx, dx
		xor  cx, cx
		mov  bx, lowMemSel
		mov  ax, 0007h          ;; DPMI Set Segment Base Address
		int  31h
	}
}

static void freeSel()
{
	asm		{
		mov     bx, lowMemSel
		mov     ax, 0001h               ;; DPMI Free Selector
		int     31h
	}
}

#pragma startup getSel
#pragma exit		freeSel
