#include "entbase.h"

EntryBase::EntryBase(WORD entid,char *name,uint32 bufsize):
	EntryID(entid),prev(0),next(0),
	BufferSize(bufsize),FileSize(0),HuboMan(0),HuboOwnership(FALSE)
{
	if(name) strncpy(EntryName,name,MAXNAME);
	else 		 strncpy(EntryName,"EntryBase",MAXNAME);
	EntryName[MAXNAME]=0;

	if(!BufferSize) BufferSize=1;
	FileSize = BufferSize;
	SpaceBuffer = new BYTE[BufferSize+1];
	if(SpaceBuffer){
		memset(SpaceBuffer,' ',BufferSize);
		SpaceBuffer[BufferSize]=0;
	}
	DataBuffer = new BYTE[BufferSize+1];
	if(DataBuffer){
		memset(DataBuffer,' ',BufferSize);
		DataBuffer[BufferSize]=0;
	}
}

EntryBase::~EntryBase()
{
	if(DataBuffer) delete[] DataBuffer;
	if(SpaceBuffer) delete[] DataBuffer;
	if(HuboMan&&HuboOwnership) delete HuboMan;
}

////////////////////////////////////////////////////////////////////

BOOL EntryBase::CopyDataBuffer(BYTE *dest,BYTE *src,uint32 buffsize)
{
	if(dest&&src){
		strncpy(dest,src,buffsize);
		dest[buffsize]=0;
		uint32 len=strlen(dest);
		if(len<buffsize)
			memset(dest+len,' ',buffsize-len);
		return TRUE;
	}
	return FALSE;
}

BOOL EntryBase::SetHuboManager(HuboManager *huboman,BOOL huboownership)
{
	if(huboman){
		if(HuboMan&&HuboOwnership) delete HuboMan;
		HuboMan=huboman;
		HuboOwnership=huboownership;
		return TRUE;
	}
	return FALSE;
}

EntryBase *EntryBase::CreateEqualEntry(EntryBase *ent,BOOL isdel)
{
	if(ent) {
		if(EntryID!=ent->EntryID) {
			if(isdel) delete ent;
			return 0;
		}
		strncpy(ent->EntryName,EntryName,MAXNAME);
		ent->EntryName[MAXNAME]='\0';
		if(HuboMan) ent->SetHuboManager(HuboOwnership? HuboMan->Duplicate():HuboMan,HuboOwnership);

		ent->Assign(this);
	}
	return ent;
}

BOOL EntryBase::IsEqual(EntryBase *ent,uint16 compkind)
{
	if(ent && EntryID==ent->EntryID){
		if(compkind&0x0001) return strcmp(DataBuffer,ent->DataBuffer)? FALSE:TRUE;
		else return TRUE;
	}
	return FALSE;
}

EntryBase *EntryBase::Duplicate()
{
	return CreateEqualEntry(new EntryBase(EntryID,EntryName,BufferSize),TRUE);
}


BOOL EntryBase::IsEmpty()
{
	BOOL ret = FALSE;

	if(!strcmp(DataBuffer,GetInitData())) ret=TRUE;
	return ret;
}

void EntryBase::InitData()
{
	if(!SetHuboIndex(0))
		SetData(GetInitData());
}



//return value=> 1:bigger than, -1: smaller than, 0:Equal
//							 2:NotEqual(Not Bigger or Smaller)

int16 EntryBase::Compare(EntryBase *ent,uint16/*compkind*/)
{
	int16 result=2;

	if(ent && EntryID==ent->EntryID){
		BOOL empty1=IsEmpty();
		BOOL empty2=ent->IsEmpty();
		result = empty1? (empty2? 2:-1):(empty2? 1:0);
		if(!result){
			int16 index1=GetHuboIndex();
			int16 index2=ent->GetHuboIndex();
			result = index1==index2? 0:(index1<index2? -1:1);
			if(!result)
				result = strcmp(DataBuffer,ent->DataBuffer);
		}
	}
	return result;
}

BOOL EntryBase::SetData(BYTE *data)
{
	if(data){
		CopyDataBuffer(DataBuffer,data,BufferSize);
		return TRUE;
	}
	return FALSE;
}

BOOL EntryBase::GetData(BYTE *data)
{
	if(DataBuffer){
		CopyDataBuffer(data,DataBuffer,BufferSize);
		return TRUE;
	}
	return FALSE;
}

int16 EntryBase::GetHuboIndex(BYTE *data)
{
	if(HuboMan) return HuboMan->GetIndexByText(data);
	return -1;
}

int16 EntryBase::GetHuboIndex()
{
	return GetHuboIndex(DataBuffer);
}

BOOL EntryBase::SetHuboIndex(int16 index)
{
	if(HuboMan) {
		HuboBase *hubo=HuboMan->GetHuboBase(index);
		if(hubo) return SetData(hubo->TextBuff);
	}
	return FALSE;
}

BOOL EntryBase::Assign(EntryBase *ent)
{
	if(ent && EntryID==ent->EntryID){
		SetData(ent->DataBuffer);
		return TRUE;
	}
	return FALSE;
}

void EntryBase::Get(FILE *fp)
{
	if(fp){
		BYTE *temp=new BYTE[BufferSize+1];
		fread(temp,BufferSize,1,fp);
		temp[BufferSize]=0;
		SetData(temp);
		delete[] temp;
	}
}

void EntryBase::Put(FILE *fp)
{
	if(fp) fwrite(DataBuffer,BufferSize,1,fp);
}
