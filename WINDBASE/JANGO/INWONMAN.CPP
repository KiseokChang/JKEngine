#include "inwonman.h"

InwonBase::InwonBase(uint16 len,char *text,uint16 context,uint16 *ingacount):
	InwonTextLen(len),Text(0),prev(0),next(0)
{
	if(!InwonTextLen) InwonTextLen=8;
	Text=new char[InwonTextLen+1];
	SetText(text);
	Context=context;
	if(ingacount)	memcpy(InwonCount,ingacount,4*sizeof(uint16));
	else					memset(InwonCount,0,4*sizeof(uint16));
}

InwonBase::~InwonBase()
{
	if(Text) delete[] Text;
}

void InwonBase::SetText(char *text)
{
	if(text){
		strncpy(Text,text,InwonTextLen);
		Text[InwonTextLen]=0;
		uint16 len=strlen(Text);
		if(len<InwonTextLen)
			memset(Text+len,' ',InwonTextLen-len);
	}
	else Text[0]=0;
}

void InwonBase::Get(FILE *fp)
{
	uint16 len;
	fread(&len,sizeof(uint16),1,fp);
	if(len){
		InwonTextLen=len;
		char *temp=new char[InwonTextLen+1];
		fread(temp,InwonTextLen,1,fp);
		temp[InwonTextLen]=0;
		SetText(temp);
		delete[] temp;
	}
	else{
		SetText(0);
	}
	fread(&Context,sizeof(uint16),1,fp);
	fread(InwonCount,4*sizeof(uint16),1,fp);
}

void InwonBase::Put(FILE *fp)
{
	fwrite(&InwonTextLen,sizeof(uint16),1,fp);
	fwrite(Text,InwonTextLen*sizeof(char),1,fp);
	fwrite(&Context,sizeof(uint16),1,fp);
	fwrite(InwonCount,4*sizeof(uint16),1,fp);
}

//=============================================================



InwonManager::InwonManager(uint16 len):
	DLLFrame<InwonBase>()
{
	InwonTextLen=len? len:8;
}

InwonManager::~InwonManager()
{}

InwonBase *InwonManager::GetInwonBase(char *text,uint16 context)
{
	InwonBase *findnode = new InwonBase(InwonTextLen,text,context);
	InwonBase *base =GetNode(findnode,0x0003);
	delete findnode;
	return base;
}

InwonBase *InwonManager::GetInwonBaseByContext(uint16 context)
{
	InwonBase *findnode = new InwonBase(InwonTextLen,0,context);
	InwonBase *base =GetNode(findnode,0x0002);
	delete findnode;
	return base;
}

InwonBase *InwonManager::GetInwonBaseByText(char* text)
{
	InwonBase *findnode = new InwonBase(InwonTextLen,text);
	InwonBase *base =GetNode(findnode,0x0001);
	delete findnode;
	return base;
}

void InwonManager::GetInwon(char *fname)
{
	if(!fname) return;
	ClearNode();
	FILE *fp=fopen(fname,"rb");
	if(fp){
		uint16 count;
		fread(&count,sizeof(uint16),1,fp);
		while(count){
			InwonBase *findnode = new InwonBase(InwonTextLen);
			if(findnode){
				findnode->Get(fp);
				PutNodeLast(findnode);
			}
			count--;
		}
		fclose(fp);
	}
}

void InwonManager::PutInwon(char *fname)
{
	if(!fname) return;
	FILE *fp=fopen(fname,"wb");
	if(fp){
		fwrite(&NodeCount,sizeof(uint16),1,fp);
		InwonBase *context=HeadNode;
		while(context){
			context->Put(fp);
			context=context->next;
		}
		fclose(fp);
	}
}

void InwonManager::Add(uint16 budae,char* text,uint16 context)
{
	InwonBase *findnode = new InwonBase(InwonTextLen,text,context);
	if(findnode){
		InwonBase *base =GetNode(findnode,0x0003);
		if(base){
			base->InwonCount[budae]++;
			delete findnode;
		}
		else{
			findnode->InwonCount[budae]++;
			PutNodeLast(findnode);
		}
	}
}

void InwonManager::Add(uint16 budae,uint16 context)
{
	InwonBase *findnode = new InwonBase(InwonTextLen,0,context);
	if(findnode){
		InwonBase *base =GetNode(findnode,0x0002);
		if(base){
			base->InwonCount[budae]++;
			delete findnode;
		}
		else{
			findnode->InwonCount[budae]++;
			PutNodeLast(findnode);
		}
	}
}

void InwonManager::Add(uint16 budae,char* text)
{
	InwonBase *findnode = new InwonBase(InwonTextLen,text);
	if(findnode){
		InwonBase *base =GetNode(findnode,0x0001);
		if(base){
			base->InwonCount[budae]++;
			delete findnode;
		}
		else{
			findnode->InwonCount[budae]++;
			PutNodeLast(findnode);
		}
	}
}