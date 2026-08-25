#ifndef _GETWIDTH_H
#define _GETWIDTH_H

#include "numentry.h"
#include "jkdialog.h"

class GetWidthDialog:public JKDialog
{
public:
	GetWidthDialog(JKPoint p,int32& width,JKControl *parent=0);
 ~GetWidthDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	int32& RefWidth;
	NumberEntry NumEnt;
};
#endif

