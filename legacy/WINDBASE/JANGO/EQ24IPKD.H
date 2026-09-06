#ifndef _EQ24IPKD_H
#define _EQ24IPKD_H

#include "eq24kind.h"
#include "jkdialog.h"
#include "eqp24glb.h"

class Eqp24InputKindDialog:public JKDialog
{
public:
	Eqp24InputKindDialog(Equip24Kind &eq24kind,RPTR modify=0,WORD ctrlid=0,JKControl *parent=0);
	~Eqp24InputKindDialog();

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
	Equip24Kind& Eqp24Kind;
	KIND24 Kind24;
	RPTR Modify;
};

#endif