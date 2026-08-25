#include "bombman.h"

BombBase::BombBase(uint16 len,char *text,uint16 context):
	InwonTextLen(len),Text(0),prev(0),next(0)
{
	if(!InwonTextLen) InwonTextLen=8;
	Text=new char[InwonTextLen+1];
	SetText(text);
	memset(BombCount,0,4*sizeof(uint16));
	Context=context;
}

BombBase::~BombBase()
{
	if(Text) delete[] Text;
}

void BombBase::SetText(char *text)
{
	if(text){
		strncpy(Text,text,InwonTextLen);
		Text[InwonTextLen]=0;
		uint16 len=strlen(Text);
		if(len<InwonTextLen)
			memset(Text+len,' ',InwonTextLen-len);
		Text[len]=0;
	}
	else Text[0]=0;
}

//=============================================================



BombManager::BombManager(uint16 len):
	DLLFrame<BombBase>()
{
	InwonTextLen=len? len:8;
}

BombManager::~BombManager()
{}

BombBase *BombManager::GetBombBaseByText(char* text)
{
	BombBase *findnode = new BombBase(InwonTextLen,text);
	BombBase *base =GetNode(findnode,0x0001);
	delete findnode;
	return base;
}

void BombManager::Add(uint16 budae,char* text,uint16 count)
{
	BombBase *findnode = new BombBase(InwonTextLen,text,count);
	if(findnode){
		BombBase *base =GetNode(findnode,0x0001);
		if(base){
			base->BombCount[budae]+=count;
			delete findnode;
		}
		else{
			findnode->BombCount[budae]+=count;
			PutNodeLast(findnode);
		}
	}
}

