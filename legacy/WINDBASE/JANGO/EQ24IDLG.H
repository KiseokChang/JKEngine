#ifndef _EQ24IDLG_H
#define _EQ24IDLG_H

#include "eq24dman.h"
#include "eqp24glb.h"
#include "jkdialog.h"

class Eqp24InputDialog:public JKDialog
{
public:
	Eqp24InputDialog(Equip24DataManager &e24dm,char* Budae,RPTR modify=0,WORD ctrlid=0,JKControl *parent=0);
	~Eqp24InputDialog();

	void InitWindow();
	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void PutRecord(EQP24& eqp24d);
	void SetValue();

	void ClearData();

	void EvEditFinish(WORD /*ctrlid*/);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

public:
	Equip24DataManager& Eq24DataMan;
	EQP24 Eqp24Data;
	RPTR Modify;
	char* BudaeDiscern;
};

#endif