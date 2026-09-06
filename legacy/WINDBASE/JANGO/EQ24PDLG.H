#ifndef _EQ24PDLG_H
#define _EQ24PDLG_H

#include "eq24dman.h"
#include "jkdialog.h"
#include "eqp24glb.h"

class Eqp24InputPublicDialog:public JKDialog
{
public:
	Eqp24InputPublicDialog(Equip24DataManager &e24dm,char* Budae,RPTR modify=0,WORD ctrlid=0,JKControl *parent=0);
	~Eqp24InputPublicDialog();

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