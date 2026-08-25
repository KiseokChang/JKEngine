#ifndef _OBJMAN_H
#define _OBJMAN_H

#include <string.h>
#include "tmpltdll.h"

class JKObject : public DLLFrame<JKObject>
{
public:
	JKObject(WORD objid);
	virtual ~JKObject();

	BOOL IsEqual(JKObject *node,WORD compkind);
	JKObject *Duplicate();

	virtual	void InitObject();
	virtual	void OpenObject();
	virtual	void SetupObject();
	virtual	void CloseObject();

	WORD ReferenceID;

	DWORD ObjectID;
	JKObject *Parent;
	JKObject *prev;
	JKObject *next;

	BOOL  IsInit;
	int8 *ObjectText;
	BOOL IsRemoveable;
	BOOL CreationError;

	virtual BOOL SetText(int8 *buf);
	virtual int8 *GetText()	{	return ObjectText;	}

	BOOL AddObject(JKObject *obj);
	JKObject *GetObject(WORD objid);

	BOOL FindSubObject(DWORD winid,WORD& objid,BOOL isall=FALSE);
	BOOL TestSubObject(DWORD winid,BOOL isall=FALSE);

protected:
	virtual void DestroySubObject(DWORD winid);
	virtual void DestroyAllSubObject();
};
#endif

