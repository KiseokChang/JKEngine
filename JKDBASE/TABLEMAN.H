#ifndef _TABLEMAN_H
#define _TABLEMAN_H

#include <dir.h>
#include "fheader.h"
#include "recbase.h"

typedef struct {
	BYTE HasData;
	int16 prev;
	int16 next;
} REFENTRY;

class TableManager
{
public:
	TableManager(RecordBase*& rec,uint16& alloccount,uint16& realcount);
	virtual ~TableManager();

	BOOL ChangeFileName(char *fname);

	void PutHeader(FILE *fp=0);

	uint16 ExpandSpace(uint16 count);
	BOOL MakeEmptyFile();

	int16 FindEmptyIndex();
	BOOL GetRefEntry(uint16 index,REFENTRY& refentry);
	BOOL PutRefEntry(uint16 index,REFENTRY& refentry);
	int16 GetPrevIndex(uint16 index);
	int16 GetNextIndex(uint16 index);

	void AddRefEntry(uint16 allocindex);
	void InsertRefEntry(int16 index,uint16 allocindex);

	BOOL FreeIndex(uint16 index);

	BOOL ReArrangeTable(uint16 *recarray);

	RecordBase*& RefRecord;
	uint16 &AllocCount;
	uint16 &RealCount;
	uint16 HeaderSize;
	int16 TableHead,TableTail;
	char TableFile[MAXPATH];
};

#endif