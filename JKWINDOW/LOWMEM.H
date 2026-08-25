/*
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */
#ifndef _LOWMEM_H
#define _LOWMEM_H
#include <dos.h>

extern void copyToLow(unsigned short dst_seg, unsigned short dst_offset, void * src, unsigned int size);
extern void copyFromLow(void * dst, unsigned short src_seg, unsigned short src_offset, unsigned int size);

extern unsigned long  getLowMemDword(unsigned short seg, unsigned short offset);
extern unsigned short getLowMemWord(unsigned short seg, unsigned short offset);
extern unsigned char  getLowMemByte(unsigned short seg, unsigned short offset);

extern void setLowMemDword(unsigned short seg, unsigned short offset, unsigned long dw);
extern void setLowMemWord (unsigned short seg, unsigned short offset, unsigned short w);
extern void setLowMemByte (unsigned short seg, unsigned short offset, unsigned char  c);
#endif
