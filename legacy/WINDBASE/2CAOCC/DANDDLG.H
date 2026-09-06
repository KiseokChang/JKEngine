#ifndef _DANDDLG_H
#define _DANDDLG_H

#include "jkdialog.h"

class DandaehoDialog : public JKDialog
{
public:
	DandaehoDialog(JKPoint& p,WORD& resid,JKControl *parent=0);
 ~DandaehoDialog();

	void PaintClient(JKDC& dc,JKRect&);

	void EvLeftClick(JKPoint& p);
	void EvNcLeftClick(JKPoint& p);

  WORD& ResID;
};

#endif
