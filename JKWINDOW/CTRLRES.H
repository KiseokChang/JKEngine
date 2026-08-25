#ifndef _CTRLRES_H
#define _CTRLRES_H

#include "jkprog.h"

class CtrlResBase
{
public:
	CtrlResBase():
		ControlID(0),ResourceID(0),prev(0),next(0)
	{}
	CtrlResBase(WORD ctrlid,WORD resid,JKRect rect):
		ControlID(ctrlid),ResourceID(resid),Rect(rect),prev(0),next(0)
	{}
	WORD ControlID;
	WORD ResourceID;
	JKRect Rect;
	CtrlResBase *prev;
	CtrlResBase *next;
};

class CtrlResManager
{
public:
	CtrlResManager();
 ~CtrlResManager();

	WORD GetResourceID(WORD ctrlid);
	BOOL GetRect(WORD ctrlid,JKRect& rect);

	CtrlResBase *Get(WORD ctrlid);
	BOOL Add(CtrlResBase *ctrlres);
	BOOL Delete(WORD ctrlid);
	void DeleteAll();

	CtrlResBase *Head;
	CtrlResBase *Tail;
	WORD CtrlResCount;
};

#endif