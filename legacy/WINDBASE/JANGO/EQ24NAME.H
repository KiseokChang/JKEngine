#ifndef _EQ24NAME_H
#define _EQ24NAME_H

#include "eqp24def.h"
#include "eqp24glb.h"
#include "dfilemn.h"

class Equip24Name
{
public:

	Equip24Name(char *fname);
	~Equip24Name();

	BOOL GetRecord(RPTR index);
	RPTR PutRecord();
	BOOL PutRecord(NAME24& buf,RPTR index);
	BOOL GetRecord(NAME24& buf,RPTR index);
	RPTR PutRecord(NAME24& buf);
	RPTR DeleteRecord(RPTR index);
	void ChangeFileName(char *name);
	RPTR RecordingPosition();

	BOOL Compare(NAME24& name1,char *buf,WORD compkind=0x0002);
	BOOL Compare(NAME24& name1,NAME24& name2,WORD compkind=0x0003);
	RPTR Search(char *buf,RPTR index,WORD compkind=0x0002);
	RPTR Search(NAME24& name2,RPTR index,WORD compkind=0x0003);

	void InitFile(char *fname);

public:
	NAME24 NameRecord;
	NAME24 WorkRecord;
	DataFileManager DFileMan;

	char FileName[13];
	int16 FilePoint;
	BOOL CreationError;
};
#endif
