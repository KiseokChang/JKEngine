#ifndef _EQP24WIN_H
#define _EQP24WIN_H

#define SAVE_FINISH 		USERMESSAGE
#define DELETEMSG   		USERMESSAGE+1
#define REDRAWLISTBOX   USERMESSAGE+2
#define CHANGETITLE     USERMESSAGE+3

#include "jkdialog.h"
#include "eq24dman.h"

class Equip24Window:public JKDialog
{
public:
	Equip24Window(WORD ctrlid);
 ~Equip24Window();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);
	void EvNcOnlyMove(JKPoint& p);
	void EvOnlyMove(JKPoint& p);
	void EvLBTouchEntry(WORD /*ctrlid*/,uint16 /*index*/);
	void EvLBSelectEntry(WORD /*ctrlid*/,uint16 /*index*/);
  void EvShowWindow();
	void EvCtrlOnlyMove(WORD ctrlid);
	void ChangeDate();
	void ChangeTitleStat();
	void SellectInput(RPTR modify);
	void WriteConfig();
	BOOL InitConfig();
	void ListBoxHide();
	void FindName(char *title);
	void FindCClass();
	void Print();
	void HeadForm(int16 capt);

public:
	Equip24Global Eqp24GLB;
	Equip24DataManager Eqp24DMan;

	char ObjectDiscern[5];
	char BudaeDiscern[7];
	BOOL C_Value;
	WORD ShowNumber;
};

#endif