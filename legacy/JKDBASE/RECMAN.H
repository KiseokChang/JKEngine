#ifndef _RECMAN_H
#define _RECMAN_H

#include <dir.h>
#include "fheader.h"
#include "recbase.h"

class RecordManager
{
public:
	RecordManager(RecordBase*& rec,uint16& alloccount,uint16& realcount,uint16& recsize);
	virtual ~RecordManager();

	BOOL ChangeFileName(char *fname);

	void PutHeader(FILE *fp=0);

	uint16 ExpandSpace(uint16 count);
	BOOL MakeEmptyFile();

	BOOL GetRecord(uint16 index,RecordBase& rec);
	BOOL PutRecord(uint16 index,RecordBase& rec);


	RecordBase*& RefRecord;
	uint16 &AllocCount;
	uint16 &RealCount;
	uint16& RecordSize;
	uint16 HeaderSize;
	char DataFile[MAXPATH];
};
#endif

