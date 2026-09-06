#ifndef _BOMBMAN_H
#define _BOMBMAN_H

#include <stdio.h>
#include <string.h>
#include "tmpltdll.h"

class BombBase
{
public:
	BombBase(uint16 len=0,char *text=0,uint16 count=1);
 ~BombBase();

	void SetText(char *text);

	BOOL IsEqual(BombBase *node,uint16 compkind=0x0001)
	{
	 if(!node) return FALSE;
	 BOOL isequal=TRUE;

	 if(isequal&&compkind&0x0001&& strcmp(Text,node->Text)) isequal=FALSE;
	 if(isequal&&compkind&0x0002&& Context!=node->Context) isequal=FALSE;
	 if(isequal&&compkind&0x0004&& BombCount[0]!=node->BombCount[0]) isequal=FALSE;
	 if(isequal&&compkind&0x0008&& BombCount[1]!=node->BombCount[1]) isequal=FALSE;
	 if(isequal&&compkind&0x0010&& BombCount[2]!=node->BombCount[2]) isequal=FALSE;
	 if(isequal&&compkind&0x0020&& BombCount[3]!=node->BombCount[3]) isequal=FALSE;

	 return isequal;
	}

	void Assign(BombBase& other)
	{
		SetText(other.Text);
		Context=other.Context;
		memcpy(BombCount,other.BombCount,4*sizeof(uint16));
	}


	BombBase *Duplicate()
	{
		return new BombBase(InwonTextLen,Text,4);
	}

	BOOL IsEmpty(){
		uint16 total=BombCount[0]+BombCount[1]+BombCount[2]+BombCount[3];
		return (!total)? TRUE:FALSE;
	}

	uint16 InwonTextLen;
	char *Text;

	uint16 Context;
	uint16 BombCount[4];

	BombBase *prev;
	BombBase *next;
};



class BombManager : public DLLFrame<BombBase>
{
public:
	BombManager(uint16 len);
 ~BombManager();

	BombBase *GetBombBaseByText(char* text);

	void Add(uint16 budae,char *text,uint16 context);

	uint16 InwonTextLen;
};
#endif