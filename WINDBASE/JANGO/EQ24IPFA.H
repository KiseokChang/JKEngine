#ifndef _EQ24IPFA_H
#define _EQ24IPFA_H

#include "eq24dman.h"
#include "eqp24glb.h"
#include "jkdialog.h"

class Equip24InputFA:public JKDialog
{
public:
	Equip24InputFA(Equip24DataManager& eqp24dm,WORD sellect=0,char *text=0,char *budae=0,WORD ctrlid=0,JKControl *parent=0);
	~Equip24InputFA();

	void InitWindow();
	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void PutRecord(EQP24& eqp24d);
	void ClearData();
	void Junib();
	void JunChul();
	void JunYouk();

	BOOL ChangeJaego(char *nnaem,char *kname,int16 A,int16 B,int16 C,BOOL add=FALSE);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

public:
	Equip24DataManager& Eqp24DMan;
	EQP24 Equip24Data;
	Equip24Kind Eqp24Kind;
	int16 Sellect;
	char* BudaeDiscern;
};

#endif