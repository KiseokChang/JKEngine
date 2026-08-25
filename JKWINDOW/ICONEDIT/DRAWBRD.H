#ifndef _DRAWBRD_H
#define _DRAWBRD_H

#include "control.h"

class DrawBoard:public JKControl
{
public:
	DrawBoard(JKRect& rect,WORD ctlid,uint16& width,uint16& height,BYTE*&imgbuf,BYTE*&maskbuf,JKControl *parent=0);
 ~DrawBoard();
	void PaintClient(JKDC& dc,JKRect& paintrect);
	void PutPoint(JKDC& dc,int16 xpos,int16 ypos,int16 color);

	void EvLeftClick(JKPoint& p);
	void EvLeftDragging(JKPoint& p);
	void EvRightClick(JKPoint& p);

	uint16& IconWidth;
	uint16& IconHeight;
	BYTE*& ImageBuff;
	BYTE*& MaskBuff;
	BYTE CurrentColor;
};
#endif