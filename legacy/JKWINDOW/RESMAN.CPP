#include <stdio.h>
#include "resman.h"

ResourceBase::ResourceBase(WORD resid,uint16 reskind,BYTE *data,BYTE restype,uint16 index):
	ResourceID(resid),ResourceKind(reskind),RealData(data),ResourceType(restype),ResourceIndex(index),
	prev(0),next(0)
{
}

ResourceBase::ResourceBase(WORD resid,uint16 reskind,BOOL ispack,char *fname,uint16 index):
	ResourceID(resid),ResourceKind(reskind),RealData(0),ResourceType(RT_DYNAMIC),ResourceIndex(0),
	prev(0),next(0)
{
	if(fname){
		FILE *fp=fopen(fname,"rb");
		if(fp){
			int32 size=0;
			if(ispack){
				uint16 total=0;
				fread(&total,sizeof(total),1,fp);
				if(index<total) {
					while(index--){
						fread(&size,sizeof(size),1,fp);
						if(size) fseek(fp,size*sizeof(BYTE),SEEK_CUR);
					}
				}
				else {
					fclose(fp);
          return;
				}
			}
			fread(&size,sizeof(size),1,fp);
			if(size){
				RealData=new BYTE[size];
				if(RealData) fread(RealData,size*sizeof(BYTE),1,fp);
			}
			fclose(fp);
		}
		else RealData=0;
	}
}

ResourceBase::~ResourceBase()
{
	if(RealData&&(ResourceType&RT_DYNAMIC)) delete[] RealData;
}

BOOL ResourceBase::IsEqual(ResourceBase *node,uint16 /*compkind*/)
{
	if(node)
		return (ResourceID==node->ResourceID &&	ResourceKind==node->ResourceKind);
	return FALSE;
}
ResourceBase *ResourceBase::Duplicate()
{	return new ResourceBase(ResourceID,ResourceKind,RealData,ResourceType,ResourceIndex);	}


BYTE *ResourceBase::AllocRealData()
{
	BYTE *data=RealData;
	if(data && ResourceType&RT_FILE){
		FILE *fp=fopen(data,"rb");
		if(fp){
			int32 size=0;
			if(ResourceType&RT_PACKFILE){
				uint16 total=0;
				fread(&total,sizeof(total),1,fp);
				if(ResourceIndex<total) {
					uint16 index=ResourceIndex;
					while(index--){
						fread(&size,sizeof(size),1,fp);
						if(size) fseek(fp,size*sizeof(BYTE),SEEK_CUR);
					}
				}
				else {
					fclose(fp);
					return 0;
				}
			}
			fread(&size,sizeof(size),1,fp);
			if(size){
				data=new BYTE[size];
				if(data) fread(data,size*sizeof(BYTE),1,fp);
			}
			fclose(fp);
		}
		else data=0;
	}
	return data;
}

void ResourceBase::FreeRealData(BYTE *& data)
{
	if(data && (ResourceType&RT_FILE)) delete[] data;
	data=0;
}

//================================================//

ResourceManager::ResourceManager():
	DLLFrame<ResourceBase>()
{}

ResourceManager::~ResourceManager()
{}

ResourceBase *ResourceManager::GetResource(WORD resid,uint16 kind)
{
	ResourceBase *findnode = new ResourceBase(resid,kind);
	ResourceBase *node=GetNode(findnode,0);
	if(findnode) delete findnode;
	return node;
}

BOOL ResourceManager::AddResource(ResourceBase *res)
{
	if(res&&GetNode(res,0)){
		delete res;
		return FALSE;
	}
	return PutNodeLast(res);
}

BOOL ResourceManager::DeleteResource(WORD resid,uint16 kind)
{
	ResourceBase *findnode = new ResourceBase(resid,kind);
	BOOL result=DeleteNode(findnode,0);
	if(findnode) delete findnode;
	return result;
}

void ResourceManager::DeleteAllResource()
{
	ClearNode();
}


//////////////////////////////
// ResourceManager* ResMan; /////
//////////////////////////////
