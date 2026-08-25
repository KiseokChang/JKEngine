#include "objman.h"

JKObject::JKObject(WORD objid):
	DLLFrame<JKObject>(),prev(0),next(0),ObjectID(0),
	Parent(0),ReferenceID(objid),ObjectText(0),
	IsInit(FALSE),IsRemoveable(TRUE),
	CreationError(FALSE)
{
	ObjectID = (DWORD)this;
}

JKObject::~JKObject()
{
	if(ObjectText) delete[] ObjectText;
	DestroyAllSubObject();
}


void JKObject::InitObject()
{
	if(!IsInit){
		OpenObject();
		SetupObject();
		IsInit=TRUE;
	}
}

BOOL JKObject::IsEqual(JKObject *node,WORD compkind)
{
	if(node) {
		if(compkind&0x01 && ObjectID!=node->ObjectID) return FALSE;
		if(compkind&0x02 && ReferenceID!=node->ReferenceID) return FALSE;
		return TRUE;
	}
	return FALSE;
}

JKObject *JKObject::Duplicate()
{
	return 0;
}


void JKObject::OpenObject()
{}

void JKObject::SetupObject()
{}

void JKObject::CloseObject()
{}

BOOL JKObject::SetText(int8 *buf)
{
	if(buf){
		if(ObjectText) {
			delete[] ObjectText;
			ObjectText=0;
		}
		ObjectText = new int8[strlen(buf)+1];
		if(ObjectText) {
			strcpy(ObjectText,buf);
			return TRUE;
		}
	}
	return FALSE;
}

//===========================================================

BOOL JKObject::AddObject(JKObject *obj)
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

JKObject *JKObject::GetObject(WORD objid)
{
	JKObject *find = new JKObject(objid);
	if(find) {
		JKObject *obj=GetNode(find,0x02);
		delete find;
		return obj;
	}
	return 0;
}

void JKObject::DestroySubObject(DWORD winid)
{
	JKObject *obj;
	JKObject *find = new JKObject(0);
	if(find) {
		find->ObjectID=winid;
		obj=GetNode(find,0x01,TRUE);
		if(obj&&obj->IsRemoveable) delete obj;
		find->ObjectID=DWORD(find);
		delete find;
	}
}

void JKObject::DestroyAllSubObject()
{
	JKObject *obj = TailNode,*temp;
	while(obj) {
		temp=obj;
		obj=obj->prev;
		if(temp->IsRemoveable) delete temp;
	}
	NodeCount=0;
	TailNode=HeadNode=0;
}

BOOL JKObject::FindSubObject(DWORD winid,WORD& objid,BOOL isall)
{
	if(!winid) return 0;
	JKObject *find = new JKObject(0);
	if(find) {
		find->ObjectID=winid;
		JKObject *obj=GetNode(find,0x01);
		find->ObjectID=DWORD(find);
		delete find;
		if(obj) {
			objid=obj->ReferenceID;
			return TRUE;
		}
		if(isall){
			obj =(JKObject *)HeadNode;
			while(obj) {
				if(obj->TestSubObject(winid,isall)){
					objid=obj->ReferenceID;
					return TRUE;
				}
				obj=obj->next;
			}
		}
	}
	return FALSE;
}

BOOL JKObject::TestSubObject(DWORD winid,BOOL isall)
{
	if(!winid) return 0;
	JKObject *find = new JKObject(0);
	if(find) {
		find->ObjectID=winid;
		JKObject *obj=GetNode(find,0x01);
		find->ObjectID=DWORD(find);
		delete find;
		if(obj) return TRUE;
		if(isall){
			obj =(JKObject *)HeadNode;
			while(obj) {
				if(obj->TestSubObject(winid,isall)) return TRUE;
				obj=obj->next;
			}
		}
	}
	return FALSE;
}

