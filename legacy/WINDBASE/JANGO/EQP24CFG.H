#ifndef _EQP24CFG_H
#define _EQP24CFG_H

#include "eq24dman.h"
#include "eqp24glb.h"
#include "jkdialog.h"

class Equip24Config:public JKDialog
{
public:
	Equip24Config(Equip24Kind& eqp24kind,RPTR sellect=0,char *text=0,WORD ctrlid=0,JKControl *parent=0);
	~Equip24Config();

	void InitWindow();
	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void PutRecord(EQP24& eqp24d);
	void InitFile();
	void InitListBox();

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

public:
	Equip24Kind& Eqp24Kind;
	int16 Sellect;
};

#endif