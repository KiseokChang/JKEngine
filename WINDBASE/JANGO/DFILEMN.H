#ifndef _DFILEMN_H
#define _DFILEMN_H

#include <sys\stat.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <alloc.h>
#include <stdlib.h>
#include <dos.h>
#include <dir.h>
#include "jkwindef.h"

#define OPENMODE O_RDWR+O_BINARY
#define CREATMODE S_IWRITE

#define RPTR	int32

#define MAXFILS	11
#define MAXELEMENT		100
#define MAXINDEX	5

#define MAXKEYLEN	80

#define FLOCATE(r,l)	((int32)(sizeof(FHEADER)+(((r)-1)*(l))+(r)))
#define USEFULLOCATE(r,l)	((FLOCATE(r,l)-1))

typedef struct fhdr	 {
	RPTR Delete_Record;
	RPTR Record_Count;
	int16 Record_Length;
}	FHEADER;

class DataFileManager {
public:
	DataFileManager();
	~DataFileManager();

	void FileCreate(char *name,int16 len);
	int16 FileOpen(char *name);
	void FileClose(int16 fp);
	RPTR NewRecord(int16 fp,char *bf);
	int16 GetRecord(int16 fp,RPTR rcdno,char *bf,int16 length);
	int16 PutRecord(int16 fp,RPTR rcdno,char *bf,int16 length);
	int16 DeleteRecord(int16 fp,RPTR rcdno);
	int16 ChangeFileName(char *name,int16 len);
	void FileAllClose();
	void InitHandle();
	int16 GetHead(int16 fp);
	int16 PutHead(int16 fp);
	RPTR GetRecordingPosition(int16 fp);
	BOOL SetUseFul(int16 fp,RPTR rcdno,BOOL isuseful=TRUE);
	BOOL IsUseFul(int16 fp,RPTR rcdno);

public:
	int16 Handle[MAXFILS];
	FHEADER FileHeader[MAXFILS];
};

#endif



