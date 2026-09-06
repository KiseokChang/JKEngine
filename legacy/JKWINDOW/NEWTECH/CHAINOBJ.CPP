#include "chainobj.h"

ChainObject::ChainObject(WORD refid,WORD objtype):
	DLLFrame<ChainObject>(),prev(0),next(0),ObjectType(objtype),ObjectID(0),
	Parent(0),ReferenceID(refid),ObjectText(0),
	IsInit(FALSE),IsRemoveable(TRUE),
	CreationError(FALSE)
{
	ObjectID = (DWORD)this;
}

ChainObject::~ChainObject()
{
	DestroyAllSubObject();
	if(ObjectText) delete[] ObjectText;
}


void ChainObject::InitObject()
{
	if(!IsInit){
		OpenObject();
		SetupObject();
		IsInit=TRUE;
	}
}

BOOL ChainObject::IsEqual(ChainObject *node,WORD compkind)
{
	if(node) {
		if(compkind&0x01 && ObjectID!=node->ObjectID) return FALSE;
		if(compkind&0x02 && ReferenceID!=node->ReferenceID) return FALSE;
		if(compkind&0x04 && ObjectType!=node->ObjectType) return FALSE;
		return TRUE;
	}
	return FALSE;
}
ChainObject *ChainObject::Duplicate()
{
	return 0;
}

void ChainObject::OpenObject()
{}

void ChainObject::SetupObject()
{}

void ChainObject::CloseObject()
{}

BOOL ChainObject::SetText(char *buf)
{
	if(buf){
		if(ObjectText) {
			delete[] ObjectText;
			ObjectText=0;
		}
		ObjectText = new char[strlen(buf)+1];
		if(ObjectText) {
			strcpy(ObjectText,buf);
			return TRUE;
		}
	}
	return FALSE;
}

//===========================================================

BOOL ChainObject::AddObject(ChainObject *obj)
{
	if(obj) {
		if(!CreationError && !obj->CreationError){
			obj->Parent=this;
			PutNodeLast(obj);
			obj->IsRemoveable=TRUE;
			if(!obj->IsInit) obj->InitObject();
			return TRUE;
		}
		else delete obj;
	}
	return FALSE;
}

ChainObject *ChainObject::GetObject(WORD refid)
{
	ChainObject *find = new ChainObject(refid);
	if(find) {
		ChainObject *obj=GetNode(find,0x02);
		delete find;
		return obj;
	}
	return 0;
}

void ChainObject::DestroySubObject(DWORD objid)
{
	ChainObject *obj;
	ChainObject *find = new ChainObject(0);
	if(find) {
		find->ObjectID=objid;
		obj=GetNode(find,0x01,TRUE);
		if(obj&&obj->IsRemoveable) delete obj;
		find->ObjectID=DWORD(find);
		delete find;
	}
}

void ChainObject::DestroyAllSubObject()
{
	ChainObject *obj = TailNode,*temp;
	while(obj) {
		temp=obj;
		obj=obj->prev;
		if(temp->IsRemoveable) delete temp;
	}
	NodeCount=0;
	TailNode=HeadNode=0;
}

BOOL ChainObject::FindSubObject(DWORD objid,WORD& refid,BOOL isall)
{
	if(!objid) return 0;
	ChainObject *find = new ChainObject(0);
	if(find) {
		find->ObjectID=objid;
		ChainObject *obj=GetNode(find,0x01);
		find->ObjectID=DWORD(find);
		delete find;
		if(obj) {
			refid=obj->ReferenceID;
			return TRUE;
		}
		if(isall){
			obj =(ChainObject *)HeadNode;
			while(obj) {
				if(obj->TestSubObject(objid,isall)){
					refid=obj->ReferenceID;
					return TRUE;
				}
				obj=obj->next;
			}
		}
	}
	return FALSE;
}

BOOL ChainObject::TestSubObject(DWORD objid,BOOL isall)
{
	if(!objid) return 0;
	ChainObject *find = new ChainObject(0);
	if(find) {
		find->ObjectID=objid;
		ChainObject *obj=GetNode(find,0x01);
		find->ObjectID=DWORD(find);
		delete find;
		if(obj) return TRUE;
		if(isall){
			obj =(ChainObject *)HeadNode;
			while(obj) {
				if(obj->TestSubObject(objid,isall)) return TRUE;
				obj=obj->next;
			}
		}
	}
	return FALSE;
}

/*
	WORD ReferenceID;

	WORD ObjectType;

	DWORD ObjectID;
	ChainObject *Parent;
	ChainObject *prev;
	ChainObject *next;

	BOOL IsInit;
	char *ObjectText;
	BOOL IsRemoveable;
	BOOL CreationError;

*/
BOOL ChainObject::PutData(FILE *fp)
{
	if(fp) return TRUE;
	else return FALSE;
}
BOOL ChainObject::GetData(FILE *fp)
{
	if(fp) return TRUE;
	else return FALSE;
}

BOOL ChainObject::Put(FILE *fp)
{
	if(fp) return TRUE;
	else return FALSE;
}
BOOL ChainObject::Get(FILE *fp)
{
	if(fp) return TRUE;
	else return FALSE;
}

