#include "ctrlres.h"

CtrlResManager::CtrlResManager():
	CtrlResCount(0),Head(0),Tail(0)
{}

CtrlResManager::~CtrlResManager()
{
	DeleteAll();
}

WORD CtrlResManager::GetResourceID(WORD ctrlid)
{
	CtrlResBase *ctrlresres = Get(ctrlid);
	if(ctrlresres) return ctrlresres->ResourceID;
	return 0;
}

BOOL CtrlResManager::GetRect(WORD ctrlid,JKRect& rect)
{
	CtrlResBase *ctrlresres = Get(ctrlid);
	if(ctrlresres) {
		rect = ctrlresres->Rect;
		return TRUE;
	}
	return FALSE;
}

BOOL CtrlResManager::Add(CtrlResBase *ctrlresres)
{
	if(ctrlresres) {
		ctrlresres->prev=ctrlresres->next=0;
		if(Tail){
			Tail->next=ctrlresres;
			ctrlresres->prev=Tail;
			Tail=ctrlresres;
		}
		else{
			Head=ctrlresres;
			Tail=ctrlresres;
		}
		CtrlResCount++;
		return TRUE;
	}
	return FALSE;
}

CtrlResBase *CtrlResManager::Get(WORD ctrlid)
{
	CtrlResBase *ctrlres = Head;
	while(ctrlres) {
		if(ctrlres->ControlID==ctrlid)	return ctrlres;
		ctrlres=ctrlres->next;
	}
	return 0;
}

void CtrlResManager::DeleteAll()
{
	CtrlResBase *ctrlres = Tail,*temp;
	while(ctrlres) {
		temp=ctrlres;
		ctrlres=ctrlres->prev;
		delete temp;
		CtrlResCount--;
	}
	Tail=Head=0;
}

BOOL CtrlResManager::Delete(WORD ctrlid)
{
	CtrlResBase *ctrlres = Tail;
	while(ctrlres) {
		if(ctrlres->ControlID==ctrlid)	{
			if(ctrlres->prev)
				ctrlres->prev->next=ctrlres->next;
			else
				Head=ctrlres->next;
			if(ctrlres->next)
				ctrlres->next->prev=ctrlres->prev;
			else
				Tail=ctrlres->prev;
			delete ctrlres;
			CtrlResCount--;
			return TRUE;
		}
		ctrlres=ctrlres->prev;
	}
	return FALSE;
}

