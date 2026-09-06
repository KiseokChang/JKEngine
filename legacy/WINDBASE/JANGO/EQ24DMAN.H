#ifndef _EQ24DMAN_H
#define _EQ24DMAN_H

#include "eq24name.h"
#include "eq24kind.h"
#include "eqp24def.h"

class Equip24DataManager
{
public:
	Equip24DataManager();
	~Equip24DataManager();
	RPTR GetFirstIndex(char *s,WORD compkind);
	RPTR GetFirstIndex(NAME24& name,WORD compkind);
	RPTR GetNameIndex(KIND24& name,WORD compkind);
	BOOL PutRecord(EQP24& eqp24,WORD compname=0x000F,WORD compkind=0x0003);
	BOOL AppendKind(KIND24& kind,RPTR index);
	BOOL DeleteRecord(RPTR index,int16 kind);
	BOOL DeleteName(RPTR index);
	BOOL DeleteKind(RPTR index);
public:
	Equip24Kind Eqp24Kind;
	Equip24Name Eqp24Name;
};

#endif
