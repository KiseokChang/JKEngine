#ifndef _VECTWIN_H
#define _VECTWIN_H

#include "bezplane.h"
#include "jkwindow.h"

class VectorWindow:public JKWindow
{
public:
	VectorWindow();
 ~VectorWindow();
	void InitWindow();
	void SetupWindow();

	void PaintClient(JKDC& dc,JKRect& rect);

	void EvLeftClick(JKPoint& p);
	void EvLeftRelease(JKPoint& p);
	void EvKeyDown(uint16 key,WORD);

	void EvButtonClick(WORD ctrlid);

	int16 CheckControlPoint(JKPoint& point);

	JKPoint CtrlPoint[4][4];
	BOOL IsKBezMode;
	BezierPlane BezEngin;

	WORD BezWorkID;
	DWORD BezWorkData;
};

#endif