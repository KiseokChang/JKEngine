#include "eq24totl.h"

Equip24Base::Equip24Base(KIND24& kind):prev(0),next(0)
{
	Kind24=kind;
	Kind24.Inga=0;
	Kind24.A=0;
	Kind24.B=0;
	Kind24.C=0;
}

Equip24Base::~Equip24Base()
{
}

void Equip24Base::SetKind(KIND24& kind)
{
	Kind24=kind;
}

//=============================================================



Equip24Manager::Equip24Manager():
	DLLFrame<Equip24Base>()
{
}

Equip24Manager::~Equip24Manager()
{}

void Equip24Manager::Add(KIND24& kind)
{
	Equip24Base *findnode = new Equip24Base(kind);
	if(findnode){
		Equip24Base *base =GetNode(findnode,0x0003);
		if(base){
			base->Kind24.Inga+=kind.Inga;
			base->Kind24.A+=kind.A;
			base->Kind24.B+=kind.B;
			base->Kind24.C+=kind.C;
			delete findnode;
		}
		else{
			findnode->Kind24.Inga=kind.Inga;
			findnode->Kind24.A=kind.A;
			findnode->Kind24.B=kind.B;
			findnode->Kind24.C=kind.C;
			PutNodeLast(findnode);
		}
	}
}

