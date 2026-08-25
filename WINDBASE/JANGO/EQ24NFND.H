#ifndef _EQ24NFND_H
#define _EQ24NFND_H

#include "eq24dman.h"
#include "eqp24glb.h"
#include "jkdialog.h"

class Equip24NameFind:public JKDialog
{
public:
	Equip24NameFind(NAME24& eqp24dm,WORD& cmpkind,char *text=0,WORD ctrlid=0,JKControl *parent=0);
	~Equip24NameFind();

	void InitWindow();
	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void ClearData();

	void EvEditFinish(WORD /*ctrlid*/);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

public:
	NAME24& Eqp24Name;
	WORD& CompKind;
};

#endif