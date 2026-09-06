#ifndef _HUBOMAN_H
#define _HUBOMAN_H

#include <string.h>
#include <conio.h>
#include "tmpltdll.h"

class HuboBase
{
public:
	HuboBase(int8 *text=0,WORD number=0,WORD adddata=0,WORD isenable=TRUE);
	virtual ~HuboBase();

	void SetText(int8 *text);

	BOOL IsEqual(HuboBase *node,WORD compkind=0x000f)
	{
		if(!node) return FALSE;
		BOOL isequal=TRUE;
		if(isequal&&compkind&0x0001&&(!TextBuff||!node->TextBuff||strcmp(TextBuff,node->TextBuff))) isequal=FALSE;
		if(isequal&&compkind&0x0002&& Number!=node->Number) isequal=FALSE;
		if(isequal&&compkind&0x0004&& AddData!=node->AddData) isequal=FALSE;
		if(isequal&&compkind&0x0008&& IsEnable!=node->IsEnable) isequal=FALSE;

		return isequal;
	}

	void Assign(HuboBase& other)
	{
		SetText(other.TextBuff);
		Number  = other.Number;
		AddData = other.AddData;
		IsEnable= other.IsEnable;
	}


	HuboBase *Duplicate()
	{
		return new HuboBase(TextBuff,Number,AddData,IsEnable);
	}

	int8 *TextBuff;
	WORD Number;
	WORD AddData;
	WORD IsEnable;

	HuboBase *prev;
	HuboBase *next;
};

class HuboManager : public DLLFrame<HuboBase>
{
public:

	HuboManager();
 ~HuboManager();

	virtual BOOL Add(uint16 index,HuboBase *hubo);
	virtual BOOL DestroyAll();
	virtual BOOL Destroy(int16 index);

	BOOL ChangeHuboBase(uint16 index,HuboBase *hubo);

	int16 GetIndexByNumber(WORD cmdid);
	int16 GetIndexByText(int8 *str);

	HuboManager *Duplicate()
	{
		HuboManager *huboman = new HuboManager();
		if(huboman){
			*huboman=*this;
		}
		return huboman;
	}

	HuboBase *GetHuboBase(uint16 index)
	{
		return GetNode(index);
	}
	HuboBase *operator[](uint16 index) { return GetHuboBase(index);	}
};

#endif