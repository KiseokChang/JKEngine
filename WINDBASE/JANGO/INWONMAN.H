#ifndef _INWONMAN_H
#define _INWONMAN_H

#include <stdio.h>
#include <string.h>
#include "tmpltdll.h"

class InwonBase
{
public:
	InwonBase(uint16 len=0,char *text=0,uint16 context=-1,uint16 *ingacount=0);
 ~InwonBase();

	void SetText(char *text);

	BOOL IsEqual(InwonBase *node,uint16 compkind=0x003f)
	{
		if(!node) return FALSE;
		BOOL isequal=TRUE;

		if(isequal&&compkind&0x0001&& strcmp(Text,node->Text)) isequal=FALSE;
		if(isequal&&compkind&0x0002&& Context!=node->Context) isequal=FALSE;
		if(isequal&&compkind&0x0004&& InwonCount[0]!=node->InwonCount[0]) isequal=FALSE;
		if(isequal&&compkind&0x0008&& InwonCount[1]!=node->InwonCount[1]) isequal=FALSE;
		if(isequal&&compkind&0x0010&& InwonCount[2]!=node->InwonCount[2]) isequal=FALSE;
		if(isequal&&compkind&0x0020&& InwonCount[3]!=node->InwonCount[3]) isequal=FALSE;

		return isequal;
	}

	void Assign(InwonBase& other)
	{
		SetText(other.Text);
		Context=other.Context;
		memcpy(InwonCount,other.InwonCount,4*sizeof(uint16));
	}


	InwonBase *Duplicate()
	{
		return new InwonBase(InwonTextLen,Text,Context,InwonCount);
	}

	BOOL IsEmpty(){
		uint16 total=InwonCount[0]+InwonCount[1]+InwonCount[2]+InwonCount[3];
		return (!total)? TRUE:FALSE;
	}

	void Get(FILE *fp);
	void Put(FILE *fp);

	uint16 InwonTextLen;
	char *Text;

	uint16 Context;
	uint16 InwonCount[4];

	InwonBase *prev;
	InwonBase *next;
};



class InwonManager : public DLLFrame<InwonBase>
{
public:
	InwonManager(uint16 len);
 ~InwonManager();

	InwonBase *GetInwonBase(char *text,uint16 context);
	InwonBase *GetInwonBaseByContext(uint16 context);
	InwonBase *GetInwonBaseByText(char* text);

	InwonBase *GetContext(uint16 context);
	void Add(uint16 budae,char *text,uint16 context);
	void Add(uint16 budae,uint16 context);
	void Add(uint16 budae,char *text);

	void GetInwon(char *fname);
	void PutInwon(char *fname);

	uint16 InwonTextLen;
};
#endif