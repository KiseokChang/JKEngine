#ifndef _EQ24TOTL_H
#define _EQ24TOTL_H

#include "eqp24def.h"
#include "tmpltdll.h"
#include <stdio.h>
#include <string.h>

class Equip24Base
{
public:
	Equip24Base(KIND24& kind);
 ~Equip24Base();

	void SetKind(KIND24& kind);

	BOOL IsEqual(Equip24Base *node,uint16 compkind=0x0003)
	{
	 if(!node) return FALSE;
	 BOOL isequal=TRUE;

	 if(isequal&&compkind&0x0001&& strcmp(Kind24.Name,node->Kind24.Name)) isequal=FALSE;
	 if(isequal&&compkind&0x0002&& strcmp(Kind24.Number,node->Kind24.Number)) isequal=FALSE;
	 if(isequal&&compkind&0x0004&& Kind24.Inga!=node->Kind24.Inga) isequal=FALSE;
	 if(isequal&&compkind&0x0008&& Kind24.A!=node->Kind24.A) isequal=FALSE;
	 if(isequal&&compkind&0x0010&& Kind24.B!=node->Kind24.B) isequal=FALSE;
	 if(isequal&&compkind&0x0020&& Kind24.C!=node->Kind24.C) isequal=FALSE;

	 return isequal;
	}

	void Assign(Equip24Base& other)
	{
		Kind24=other.Kind24;
	}


	Equip24Base *Duplicate()
	{
		return new Equip24Base(Kind24);
	}

	BOOL IsEmpty(){
		uint16 total=Kind24.A+Kind24.B+Kind24.C;
		return (!total)? TRUE:FALSE;
	}

	KIND24 Kind24;

	Equip24Base *prev;
	Equip24Base *next;
};



class Equip24Manager : public DLLFrame<Equip24Base>
{
public:
	Equip24Manager();
 ~Equip24Manager();

	void Add(KIND24& Kind24);
};
#endif