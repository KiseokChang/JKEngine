#ifndef _CHAINOBJ_H
#define _CHAINOBJ_H

#include <stdio.h>
#include <string.h>
#include "tmpltdll.h"

class ChainObject : public DLLFrame<ChainObject>
{
public:
	ChainObject(WORD refid,WORD objtype=0);
	virtual ~ChainObject();

	virtual BOOL IsEqual(ChainObject *node,WORD compkind);
	virtual ChainObject *Duplicate();

	virtual	void InitObject();
	virtual	void OpenObject();
	virtual	void SetupObject();
	virtual	void CloseObject();

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

	virtual	BOOL SetText(char *buf);
	virtual	char *GetText()	{	return ObjectText;	}

	virtual	BOOL AddObject(ChainObject *obj);
	virtual	ChainObject *GetObject(WORD refid);

	virtual	BOOL FindSubObject(DWORD winid,WORD& refid,BOOL isall=FALSE);
	virtual	BOOL TestSubObject(DWORD winid,BOOL isall=FALSE);

	BOOL PutData(FILE *fp);
	BOOL GetData(FILE *fp);

	virtual	BOOL Put(FILE *fp);
	virtual	BOOL Get(FILE *fp);
protected:
	virtual void DestroySubObject(DWORD winid);
	virtual void DestroyAllSubObject();
};
#endif

