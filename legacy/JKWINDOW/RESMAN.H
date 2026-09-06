#ifndef _RESMAN_H
#define _RESMAN_H

#include "tmpltdll.h"

#define RES_IMAGE 		0x0000
#define RES_STRING		0x0001
#define RES_CURSOR		0x0002
#define RES_PALETTE		0x0003

#define RT_DYNAMIC  	0x01
#define RT_FILE				0x02
#define RT_PACKFILE		0x04

class ResourceBase
{
public:
	ResourceBase(WORD resid,uint16 reskind,BYTE *data=0,BYTE restype=0,uint16 index=0);
  ResourceBase(WORD resid,uint16 reskind,BOOL ispack,char *fname,uint16 index=0);
  virtual ~ResourceBase();

	BOOL IsEqual(ResourceBase *node,uint16 /*compkind*/);
	ResourceBase *Duplicate();


	BYTE *AllocRealData();
	void FreeRealData(BYTE *& data);

	WORD ResourceID;
	uint16 ResourceKind;
	BYTE *RealData;
	BYTE ResourceType;
	uint16 ResourceIndex;
	ResourceBase *prev;
	ResourceBase *next;
};

class ResourceManager : public DLLFrame<ResourceBase>
{
public:
	ResourceManager();
 ~ResourceManager();

	ResourceBase *GetResource(WORD resid,uint16 kind=RES_IMAGE);
	BOOL AddResource(ResourceBase *res);
	BOOL DeleteResource(WORD resid,uint16 kind=RES_IMAGE);
	void DeleteAllResource();
};

extern ResourceManager *ResMan;
#endif