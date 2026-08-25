#include <stdio.h>
#include <string.h>
#include "huboman.h"

HuboBase::HuboBase(int8 *text,WORD number,WORD adddata,WORD isenable):
	Number(number),IsEnable(isenable),TextBuff(0),AddData(adddata),
	prev(0),next(0)
{
	SetText(text);
}

HuboBase::~HuboBase()
{
	if(TextBuff) delete[] TextBuff;
}

void HuboBase::SetText(int8 *text)
{
	if(text){
		if(TextBuff) delete[] TextBuff;
		uint16 len=strlen(text);
		TextBuff = new int8[len+1];
		if(TextBuff) strcpy(TextBuff,text);
	}
}


//======================================================
HuboManager::HuboManager():
	DLLFrame<HuboBase>()
{}

HuboManager::~HuboManager()
{}

int16 HuboManager::GetIndexByNumber(WORD cmdid)
{
	HuboBase comphubo(0,cmdid);
	return GetIndex(&comphubo,0x0002);
}

int16 HuboManager::GetIndexByText(int8 *str)
{
	HuboBase comphubo(str);
	return GetIndex(&comphubo,0x0001);
}

BOOL HuboManager::Destroy(int16 index)
{
	if(NodeCount){
		if(index==-1) index=NodeCount-1;
		return DeleteNode(index);
	}
	return FALSE;
}

BOOL HuboManager::DestroyAll()
{
	if(NodeCount){
		ClearNode();
		return TRUE;
	}
	return FALSE;
}

BOOL HuboManager::ChangeHuboBase(uint16 index,HuboBase *hubo)
{
	if(hubo){
		HuboBase *temp=GetNode(index);
		if(temp) {
			temp->Assign(*hubo);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL HuboManager::Add(uint16 index,HuboBase *hubo)
{
	return PutNode(hubo,index);
}
