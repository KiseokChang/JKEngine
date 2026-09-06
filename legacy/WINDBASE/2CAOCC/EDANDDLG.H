#ifndef _EDANDDLG_H
#define _EDANDDLG_H

#include "jkdialog.h"

class EDandaehoDialog : public JKDialog
{
public:
	EDandaehoDialog(JKPoint& p,WORD& resid,JKControl *parent=0);
 ~EDandaehoDialog();

	void PaintClient(JKDC& dc,JKRect&);

	void EvLeftClick(JKPoint& p);
	void EvNcLeftClick(JKPoint& p);

  WORD& ResID;
};

#endif
