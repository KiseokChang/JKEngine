#ifndef _SHOWCUR_H
#define _SHOWCUR_H

#include "real2pix.h"
#include "jkdialog.h"

class ShowCurInfom : public JKDialog
{
public:
	ShowCurInfom(JKRect& rect,RealCoordManager& coordman,JKPoint32& blocksize,JKControl* parent=0);
 ~ShowCurInfom();

	JKRect CalcClientRect();
	void SetupWindow();

	void PaintWindow(JKDC& dc,JKRect& paintrect);
	void PaintClient(JKDC& dc,JKRect&);
	void EvLeftClick(JKPoint& p);

	RealCoordManager& RealCoordMan;
	JKPoint32& BlockSize;
};
#endif
