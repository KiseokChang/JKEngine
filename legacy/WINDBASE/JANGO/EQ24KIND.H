#ifndef _EQ24KIND_H
#define _EQ24KIND_H

#include "eqp24def.h"
#include "eqp24glb.h"
#include "dfilemn.h"

class Equip24Kind
{
public:

	Equip24Kind(char *fname);
	~Equip24Kind();

	BOOL GetRecord(RPTR index);
	RPTR PutRecord();
	BOOL PutRecord(KIND24& buf,RPTR index);
	BOOL DeleteRecord(RPTR index);
	BOOL GetRecord(KIND24& buf,RPTR index);
	RPTR PutRecord(KIND24& buf);
	void ChangeFileName(char *name);

	RPTR RecordingPosition();

	BOOL Compare(KIND24& name1,char *buf,WORD compkind);
	BOOL Compare(KIND24& name1,KIND24& name2,WORD compkind);
	RPTR Search(char *buf,RPTR index,WORD compkind);
	RPTR Search(KIND24& name2,RPTR index,WORD compkind);

	void InitFile(char *fname);

public:
	KIND24 KindRecord;
	KIND24 WorkRecord;
	DataFileManager DFileMan;

	char FileName[13];
	int16 FilePoint;
	BOOL CreationError;
};
#endif
