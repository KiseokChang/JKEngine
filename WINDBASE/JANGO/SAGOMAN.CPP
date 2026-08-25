#include "joglobal.h"
#include "string.h"
#include "sagoman.h"

SagoNode::SagoNode(uint16 index):
	prev(0),next(0)
{
	Index=index;
}

SagoNode::~SagoNode()
{}

BOOL SagoNode::IsEqual(SagoNode*node,uint16 /*compkind*/)
{
	if(!node) return FALSE;
	if(Index!=node->Index) return FALSE;
	return TRUE;
}

void SagoNode::Assign(SagoNode& other)
{
	Index=other.Index;
}


SagoNode *SagoNode::Duplicate()
{
	return new SagoNode(Index);
}

void SagoNode::Get(FILE *fp)
{
	if(fp) fread(&Index,sizeof(Index),1,fp);
}
void SagoNode::Put(FILE *fp)
{
	if(fp) fwrite(&Index,sizeof(Index),1,fp);
}


//============================================================================
//============================================================================

SagoBase::SagoBase(char *title):
	DLLFrame<SagoNode>(),prev(0),next(0)
{
	SetText(title);
	memset(SagoCount,0,4*2*sizeof(uint16));
}

SagoBase::~SagoBase()
{}

void SagoBase::SetText(char *str)
{
	if(str){
		strncpy(Text,str,8);
		if(strlen(str)<8) memset(Text+strlen(str),' ',8-strlen(str));
		Text[8]=0;
	}
	else {
		memset(Text,' ',8);
		Text[8]=0;
	}
}

SagoNode *SagoBase::GetSagoNode(uint16 index)
{
	SagoNode *findnode = new SagoNode(index);
	SagoNode *node =GetNode(findnode,0x01);
	delete findnode;
	return node;
}

BOOL SagoBase::DeleteSagoNode(uint16 budae,uint16 index,BOOL isganbu)
{
	if(budae>3||!SagoCount[budae][isganbu? 1:0]) return FALSE;
	SagoNode *findnode = new SagoNode(index);
	BOOL result=DeleteNode(findnode,0x01);
	delete findnode;
	if(result)
		SagoCount[budae][isganbu? 1:0]--;
	return result;
}

BOOL SagoBase::AddSagoNode(uint16 budae,uint16 index,BOOL isganbu)
{
	if(budae>3) return FALSE;
	SagoNode *node = GetSagoNode(index);
	if(!node){
		node= new SagoNode(index);
		if(PutNodeLast(node))	{
			SagoCount[budae][isganbu? 1:0]++;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL SagoBase::IsEqual(SagoBase *node,uint16 compkind)
{
	if(!node) return FALSE;
	BOOL isequal=TRUE;

	if(isequal&&compkind&0x0001&& strcmp(Text,node->Text)) isequal=FALSE;
	if(isequal&&compkind&0x0002&& SagoCount[0][0]!=node->SagoCount[0][0]) isequal=FALSE;
	if(isequal&&compkind&0x0004&& SagoCount[0][1]!=node->SagoCount[0][1]) isequal=FALSE;
	if(isequal&&compkind&0x0008&& SagoCount[1][0]!=node->SagoCount[1][0]) isequal=FALSE;
	if(isequal&&compkind&0x0010&& SagoCount[1][1]!=node->SagoCount[1][1]) isequal=FALSE;
	if(isequal&&compkind&0x0020&& SagoCount[2][0]!=node->SagoCount[2][0]) isequal=FALSE;
	if(isequal&&compkind&0x0040&& SagoCount[2][1]!=node->SagoCount[2][1]) isequal=FALSE;
	if(isequal&&compkind&0x0080&& SagoCount[3][0]!=node->SagoCount[3][0]) isequal=FALSE;
	if(isequal&&compkind&0x0100&& SagoCount[3][1]!=node->SagoCount[3][1]) isequal=FALSE;
	if(isequal&&compkind&0x0200) isequal=DLLFrame<SagoNode>::IsEqual(*node,0x02);

	return isequal;
}

void SagoBase::Assign(SagoBase& other)
{
		SetText(other.Text);
		memcpy(SagoCount,other.SagoCount,4*2*sizeof(uint16));
		*this=other;
}

SagoBase *SagoBase::Duplicate()
{
	SagoBase *base = new SagoBase(Text);
	base->Assign(*this);
	return base;
}

void SagoBase::Get(FILE *fp)
{
	if(fp){
		ClearNode();

		BYTE *temp=new BYTE[8+1];
		fread(temp,8*sizeof(BYTE),1,fp);
		temp[8]=0;
		SetText(temp);
		delete[] temp;

		fread(SagoCount,4*2*sizeof(uint16),1,fp);

		uint16 count;
		fread(&count,sizeof(uint16),1,fp);
		while(count){
			SagoNode *node = new SagoNode();
			if(node){
				node->Get(fp);
				PutNodeLast(node);
			}
			count--;
		}
	}
}

void SagoBase::Put(FILE *fp)
{
	if(fp){
		fwrite(Text,8*sizeof(BYTE),1,fp);
		fwrite(SagoCount,4*2*sizeof(uint16),1,fp);
		fwrite(&NodeCount,sizeof(uint16),1,fp);
		SagoNode *node=HeadNode;
		while(node){
			node->Put(fp);
			node=node->next;
		}
	}
}

//==========================================================================
//==========================================================================

SagoManager::SagoManager(PersonManager& recman,char *fname)
	:DLLFrame<SagoBase>(),PersonMan(recman)
{
	if(fname){
		strncpy(FileName,fname,12);
		FileName[12]=0;
	}
	else {
		strcpy(FileName,BudaeName);
		strcat(FileName,"sago.dat");
	}
	Read();
}
SagoManager::~SagoManager()
{}

SagoBase *SagoManager::GetSagoBase(char *title)
{
	SagoBase *findnode = new SagoBase(title);
	SagoBase *node =GetNode(findnode,0x0001);
	delete findnode;
	return node;
}

SagoBase *SagoManager::GetSagoBase(uint16 index)
{
	SagoNode *findnode = new SagoNode(index);
	SagoBase *base=HeadNode;
	while(base){
		SagoNode *node =base->GetNode(findnode,0x01);
		if(node) break;
		base=base->next;
	}
	delete findnode;
	return base;
}
SagoNode *SagoManager::GetSagoNode(uint16 index)
{
	SagoNode *node=0;
	SagoNode *findnode = new SagoNode(index);
	SagoBase *base=HeadNode;
	while(base){
		node =base->GetNode(findnode,0x01);
		if(node) break;
		base=base->next;
	}
	delete findnode;
	return node;
}


BOOL SagoManager::DeleteSagoBase(char *title)
{
	SagoBase *findnode = new SagoBase(title);
	BOOL result=DeleteNode(findnode,0x0001);
	delete findnode;
	return result;
}

BOOL SagoManager::DeleteSagoNode(uint16 budae,uint16 index,BOOL delempty)
{
	if(budae>3) return FALSE;
	PersonRecord TempRecord;
	PersonMan.GetRecord(index,TempRecord);
	BOOL isganbu=TempRecord.IsGanbu();
	SagoBase *base=HeadNode;
	while(base){
		if(base->DeleteSagoNode(budae,index,isganbu)){
			if(delempty&&!base->NodeCount) {
				Detach(base);
				delete base;
			}
			return TRUE;
		}
		base=base->next;
	}
	return FALSE;
}

BOOL SagoManager::AddSagoBase(char *title)
{
	SagoBase *base = GetSagoBase(title);
	if(!base){
		base= new SagoBase(title);
		if(!PutNodeLast(base)) return FALSE;
	}
	return TRUE;
}

BOOL SagoManager::AddSagoNode(char *title,uint16 budae,uint16 index)
{
	if(budae>3) return FALSE;
	PersonRecord TempRecord;
	PersonMan.GetRecord(index,TempRecord);
	BOOL isganbu=TempRecord.IsGanbu();
	SagoBase *base = GetSagoBase(title);
	if(!base){
		base= new SagoBase(title);
		if(!PutNodeLast(base)) return FALSE;
	}
	return base->AddSagoNode(budae,index,isganbu);
}

void SagoManager::Get(FILE *fp)
{
	if(fp){
		ClearNode();
		uint16 count;
		fread(&count,sizeof(uint16),1,fp);
		while(count){
			SagoBase *base = new SagoBase();
			if(base){
				base->Get(fp);
				PutNodeLast(base);
			}
			count--;
		}
	}
}
void SagoManager::Put(FILE *fp)
{
	if(fp){
		fwrite(&NodeCount,sizeof(uint16),1,fp);
		SagoBase *base=HeadNode;
		while(base){
			base->Put(fp);
			base=base->next;
		}
	}
}


void SagoManager::Read()
{
	FILE *fp=fopen(FileName,"rb");
	if(fp){
		Get(fp);
		fclose(fp);
	}
}

void SagoManager::Write()
{
	FILE *fp=fopen(FileName,"wb");
	if(fp){
		Put(fp);
		fclose(fp);
	}
}

uint16 SagoManager::GetTotalCount()
{
	uint16 total=0;
	SagoBase *base=HeadNode;
	while(base){
		total+= base->NodeCount;
		base=base->next;
	}
	return total;
}

uint16 SagoManager::GetGanbuCount()
{
	uint16 total=0;
	SagoBase *base=HeadNode;
	while(base){
		for(uint16 i=0;i<4;i++)
			total+= base->SagoCount[i][1];
		base=base->next;
	}
	return total;
}

uint16 SagoManager::GetSabyungCount()
{
	uint16 total=0;
	SagoBase *base=HeadNode;
	while(base){
		for(uint16 i=0;i<4;i++)
			total+= base->SagoCount[i][0];
		base=base->next;
	}
	return total;
}