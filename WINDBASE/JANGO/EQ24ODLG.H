#ifndef _EQ24ODLG_H
#define _EQ24ODLG_H

#include "eq24dman.h"
#include "eqp24glb.h"
#include "jkdialog.h"
#include "scrllist.h"

class Eqp24OtherDialog:public JKDialog
{
public:
	Eqp24OtherDialog(Equip24DataManager& eqp24dm,int16 sel=0,char* budae=0,WORD ctrlid=0,JKControl *parent=0);
	~Eqp24OtherDialog();

	void InitWindow();
	void SetupWindow();
	void EvShowWindow();
  void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	void EvButtonClick(WORD ctrlid);
	void MakeString(KIND24& kind24,RPTR index,int16 realcount);
	void InitListBox();
	void HeadForm(int16 capt);
	void Print();
public:
	Equip24DataManager& Eqp24DataMan;
	int16 Sellect;
	char *BudaeDiscern;
};

#endif