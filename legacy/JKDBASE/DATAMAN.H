#ifndef _DATAMAN_H
#define _DATAMAN_H

#include "recman.h"
#include "tableman.h"


class DataManager
{
public:
	DataManager();
	virtual ~DataManager();

	BOOL ChangeFileName(char *fname,RecordBase& rec);

	BOOL MakeEmptyFile(uint16 alloccount);

	int16 FindEmptyIndex();
	int16 AllocIndex();

	virtual BOOL DeleteRecord(uint16 index);
	virtual BOOL DeleteAll();
	virtual int16 AddRecord(RecordBase& rec);
	virtual int16 InsertRecord(uint16 index,RecordBase& rec);
	virtual BOOL SetRecord(uint16 index,RecordBase& rec);

	int16 Search(EntryBase *ent,int16 findstart=-1);
	int16 SearchScope(EntryBase *min,EntryBase *max,int16 findstart=-1);
	int16 Search(RecordBase *rec,uint16 index,int16 findstart=-1);
	int16 SearchScope(RecordBase *min,RecordBase *max,uint16 index,int16 findstart=-1);
	int16 Search(RecordBase *rec,WORD *compoder,uint16 ordercount,int16 findstart=-1);
	int16 SearchScope(RecordBase *min,RecordBase *max,WORD *compoder,uint16 ordercount,int16 findstart=-1);

	BOOL Sort(uint16 *recarray,uint16 reccount,uint16 index);
	BOOL Sort(uint16 *recarray,uint16 reccount,WORD *compoder,uint16 ordercount);
	BOOL FileSort(uint16 entid);
	BOOL FileSort(WORD *compoder,uint16 ordercount);

	uint16 AllocCount;
	uint16 RealCount;
  uint16 RecordSize;
	RecordManager RecordMan;
	TableManager 	TableMan;
	char RecFileName[MAXPATH];

	BOOL CreationError;

	RecordBase *WorkRecord;

//	BOOL IsEqual(DataManager& other,uint16 compkind=0x00);
//	DataManager *prev,*next;
};

#endif
