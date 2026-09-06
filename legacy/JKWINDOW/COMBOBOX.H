#ifndef _COMBOBOX_H
#define _COMBOBOX_H

#include "huboman.h"
#include "jkedit.h"

#define SHOWPOPUPLIST 0x100

class Combobox:public JKEdit
{
public:
	Combobox(JKRect& rect,WORD ctrlid,WORD bufsize,HuboManager *huboman=0,BOOL enableedit=TRUE,JKControl *parent=0);
 ~Combobox();

	JKRect CalcClientRect();
	void SetupWindow();

	void EvButtonClick(WORD);
	void EvCommand(WORD cmdid,WORD,DWORD ,WORD );
	void EvKeyDown(WORD key,WORD /*modifier*/);
	void EvCtrlKillFocus(WORD ctrlid);

	void EvLBSelectEntry(WORD ctrlid,uint16 index);
	void EvLBCharDown(WORD ctrlid,uint16 index,WORD key,WORD modifier);

	HuboManager *HuboMan;
	JKRect PopRect;
	BOOL Ownership;
};
#endif