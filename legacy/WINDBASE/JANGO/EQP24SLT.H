#ifndef _EQ24SLT_H
#define _EQ24SLT_H

#include "eqp24glb.h"
#include "jkdialog.h"

class Equip24SellectDialog:public JKDialog
{
public:
	Equip24SellectDialog(JKRect rect,char& budaedisc,WORD ctrlid=0,JKControl *parent=0);
	~Equip24SellectDialog();

	void SetupWindow();
	void EvShowWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void PutRecord(EQP24& eqp24d);
	void SetValue();

	void EvEditFinish(WORD /*ctrlid*/);
public:
	char &BudaeDiscern;
};

#endif