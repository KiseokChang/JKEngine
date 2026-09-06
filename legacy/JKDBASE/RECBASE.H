#ifndef _RECBASE_H
#define _RECBASE_H

#include "tmpltdll.h"
#include "entbase.h"

class RecordBase : public DLLFrame<EntryBase>
{
public:
	RecordBase(WORD recid,char *name=0);
	virtual ~RecordBase();

/////////////////////////////////////////////////////////////////////
///  needless of overridding

	RecordBase *CreateEqualRecord(RecordBase *rec,BOOL isdel=FALSE);

	EntryBase *GetEntry(WORD entid);
	EntryBase *GetEntry(EntryBase *ent);
	BOOL AddEntry(EntryBase *ent);
	void DestroyEntry(WORD entid);
	void DestroyEntry(EntryBase *ent);
	void DestroyAllEntry();

	BOOL IsEmpty();
	BOOL IsFullfilled();
	uint16 GetMaxBufferSize();
	uint16 GetFileSize();

	BOOL SetData(WORD entid,BYTE *data);
	BOOL GetData(WORD entid,BYTE *data);

	virtual int16 Compare(EntryBase *ent,uint16 compkind=0);
	int16 Compare(RecordBase *base,WORD entid,uint16 compkind=0);
	BOOL CompareScope(EntryBase *min,EntryBase *max,uint16 compkind=0);
	BOOL CompareScope(RecordBase *min,RecordBase *max,WORD entid,uint16 compkind=0);
	int16 Compare(RecordBase *base,WORD *compoder=0,uint16 ordercount=0,uint16 *compkind=0);
	BOOL CompareScope(RecordBase *min,RecordBase *max,WORD *compoder=0,uint16 ordercount=0,uint16 *compkind=0);


/////////////////////////////////////////////////////////////////////
///  little need of overridding
	virtual void InitData();
	virtual BOOL Assign(EntryBase *ent);
	virtual BOOL Assign(RecordBase *base);
	virtual void Get(FILE *fp);
	virtual void Put(FILE *fp);

/////////////////////////////////////////////////////////////////////
///  For taking role of 'Class T' for 'Template DLLFrame<Class T>'
	virtual BOOL IsEqual(RecordBase *rec,uint16 compkind=0);
	virtual RecordBase *Duplicate()=0;

/////////////////////////////////////////////////////////
///  must override
/////////////////////////////////////////////////////////

	char RecordName[MAXNAME+1];
	WORD RecordID;
};
#endif