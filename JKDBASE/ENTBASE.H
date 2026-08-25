#ifndef _ENTBASE_H
#define _ENTBASE_H
#include <stdio.h>
#include <string.h>
#include "huboman.h"

#define MAXNAME	20

class EntryBase
{
public:
	EntryBase(WORD entid,char *name="EntryBase",uint32 bufsize=0);
	virtual ~EntryBase();

//////////////////////////////////////////////////////////////////////
///  needless of overridding
	BOOL CopyDataBuffer(BYTE *dest,BYTE *src,uint32 buffsize);

	EntryBase *CreateEqualEntry(EntryBase *ent,BOOL isdel=FALSE);

	BOOL SetHuboManager(HuboManager *huboman,BOOL huboownership);
	int16 GetHuboIndex(BYTE *data);
	BOOL SetHuboIndex(int16 index,BYTE *data);
	virtual int16 GetHuboIndex();
	virtual BOOL SetHuboIndex(int16 index);

/////////////////////////////////////////////
///  little need of overridding
	virtual void InitData();
	virtual BOOL IsEmpty();
	virtual BYTE *GetInitData(){return SpaceBuffer;}
	virtual BOOL Assign(EntryBase *ent);
	virtual void Get(FILE *fp);
	virtual void Put(FILE *fp);
/////////////////////////////////////////////////////////
///  For taking role of 'Class T' for 'Template DLLFrame<Class T>'
	virtual BOOL IsEqual(EntryBase *ent,uint16 compkind=0);
	virtual EntryBase *Duplicate();
/////////////////////////////////////////////////////////
///  must override
	//return value=> 1:bigger than, -1: smaller than, 0:Equal
	//							 2:NotEqual(Not Bigger or Smaller)
	virtual int16 Compare(EntryBase *ent,uint16 compkind=0xffffffff);
	virtual BOOL HasValidData(){ return TRUE;}
	virtual BOOL SetData(BYTE *);
	virtual BOOL GetData(BYTE *);
/////////////////////////////////////////////////////////

	WORD EntryID;
	BYTE *DataBuffer;
	BYTE *SpaceBuffer;
	char EntryName[MAXNAME+1];

	HuboManager *HuboMan;
	BOOL HuboOwnership;

	uint32 BufferSize;
	uint32 FileSize;


	EntryBase *prev;
	EntryBase *next;
};
#endif