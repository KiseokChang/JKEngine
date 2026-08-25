#ifndef _EQ24SDLG_H
#define _EQ24SDLG_H

#include "eq24dman.h"
#include "jkdialog.h"
#include "eqp24glb.h"

class Eqp24InputSomoDialog:public JKDialog
{
public:
	Eqp24InputSomoDialog(Equip24DataManager &e24dm,RPTR modify=0,BOOL somoflag=0,WORD ctrlid=0,JKControl *parent=0);
	~Eqp24InputSomoDialog();

	void InitWindow();
	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void PutRecord(EQP24& eqp24d);
	void SetValue();
	void SaveFunction();

	void ClearData();

	void EvEditFinish(WORD /*ctrlid*/);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

public:
	Equip24DataManager& Eq24DataMan;
	EQP24 Eqp24Data,Eqp24Data2;
	RPTR Modify;
	BOOL SomoFlag;
};

#endif