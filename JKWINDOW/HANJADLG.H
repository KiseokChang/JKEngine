#ifndef _HANJADLG_H
#define _HANJADLG_H

#include "jkdialog.h"

class HanjaSelectDialog:public JKDialog
{
public:
	HanjaSelectDialog(JKPoint& p,WORD& hangulcode);
 ~HanjaSelectDialog();

	void PaintClient(JKDC& dc,JKRect& rect);

	void EvCharDown(WORD key,WORD);
	void EvKeyDown(WORD key,WORD);
	void EvDlgOk();

	WORD Position;
	WORD& HanjaCode;
	WORD MaxHanjaCount;
	WORD *HanjaCodeSet;
};
#endif