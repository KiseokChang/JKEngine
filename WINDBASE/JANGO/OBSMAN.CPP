#include "joglobal.h"
#include "obsman.h"

ObserveManager::ObserveManager(char *fname)
{
	if(!fname){
		strcpy(ObserveDataFile,BudaeName);
		strcat(ObserveDataFile,"obsv");
		strcpy(ObserveTableFile,BudaeName);
		strcat(ObserveTableFile,"obsv");
		strcpy(ObserveTempFile,BudaeName);
		strcat(ObserveTempFile,"obsv");
	}
	else{
		strcpy(ObserveDataFile,fname);
		strcpy(ObserveTableFile,fname);
		strcpy(ObserveTempFile,fname);
	}
	strcat(ObserveDataFile,".dat");
	strcat(ObserveTableFile,".tbl");
	strcat(ObserveTempFile,".tmp");

	HeaderSize=(int32)sizeof(OBSREFENTRY)*MaxRefEntryCount;
	IsInitFile();
}

ObserveManager::~ObserveManager()
{
}

void ObserveManager::IsInitFile()
{
	struct ffblk ffblk1,ffblk2;
	BOOL TblExist=!findfirst(ObserveTableFile,&ffblk1,0);
	BOOL DataExist=!findfirst(ObserveDataFile,&ffblk2,0);
	if(!TblExist)
		MakeEmptyTable();
	else if(HeaderSize!=ffblk1.ff_fsize||!DataExist)
		MakeEmptyTable();
}

BOOL ObserveManager::IsEmptyTable()
{
	BOOL ret=TRUE;
	FILE *TblFp = fopen(ObserveTableFile,"rb");
	if(TblFp){
		OBSREFENTRY refentry;
		for(uint16 i=0;i<MaxRefEntryCount;i++){
			fread(&refentry,sizeof(refentry),1,TblFp);
			if(refentry.size){
				ret = FALSE;
				break;
			}
		}
		fclose(TblFp);
	}
	else ret=FALSE;
	return ret;
}

int16 ObserveManager::FindHead(int16 index)
{
	int16 ret=-1;
	OBSREFENTRY refentry;
	int16 prev;
	while(index>=0&&index<MaxRefEntryCount){
		GetRefEntry(index,refentry);
		prev=refentry.prev;
		if(prev==-1){
			ret=index;
			break;
		}
		index=prev;
	}
	return ret;
}

int16 ObserveManager::FindTail(int16 index)
{
	int16 ret=-1;
	OBSREFENTRY refentry;
	int16 next;
	while(index>=0&&index<MaxRefEntryCount){
		GetRefEntry(index,refentry);
		next=refentry.next;
		if(next<0){
			ret=index;
			break;
		}
		index=next;
	}
	return ret;
}

void ObserveManager::MakeEmptyTable()
{
	FILE *Fp;

	Fp = fopen(ObserveTableFile,"wb");
	OBSREFENTRY refentry;
	refentry.size=0;
	refentry.prev=-1;
	refentry.next=-1;

	for(uint16 i=0;i<MaxRefEntryCount;i++)
		fwrite(&refentry,sizeof(refentry),1,Fp);
	fclose(Fp);

	Fp = fopen(ObserveDataFile,"wb");
	fclose(Fp);
}

BOOL ObserveManager::GetRefEntry(uint16 index,OBSREFENTRY& refentry)
{
	if(index<MaxRefEntryCount){
		FILE *TblFp = fopen(ObserveTableFile,"rb");
		if(TblFp){
			fseek(TblFp,sizeof(refentry)*index,SEEK_SET);
			fread(&refentry,sizeof(refentry),1,TblFp);
			fclose(TblFp);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL ObserveManager::PutRefEntry(uint16 index,OBSREFENTRY& refentry)
{
	if(index<MaxRefEntryCount){
		FILE *TblFp = fopen(ObserveTableFile,"rb+");
		if(TblFp){
			fseek(TblFp,sizeof(refentry)*index,SEEK_SET);
			fwrite(&refentry,sizeof(refentry),1,TblFp);
			fclose(TblFp);
			return TRUE;
		}
	}
	return FALSE;
}

int16 ObserveManager::GetPrevIndex(uint16 index)
{
	OBSREFENTRY refentry;
	if(GetRefEntry(index,refentry))
		return refentry.prev;
	return -1;
}

int16 ObserveManager::GetNextIndex(uint16 index)
{
	OBSREFENTRY refentry;
	if(GetRefEntry(index,refentry))
		return refentry.next;
	return -1;
}

int32 ObserveManager::CalcDataPosition(uint16 index)
{
	FILE *TblFp = fopen(ObserveTableFile,"rb");
	if(index<MaxRefEntryCount){
		if(TblFp){
			int32 Total=0;
			OBSREFENTRY refentry;
			for(int16 i=0;i<index;i++){
				fread(&refentry,sizeof(refentry),1,TblFp);
				Total+=(int32)refentry.size;
			}
			fclose(TblFp);
			return Total;
		}
	}
	return -1;
}

int16 ObserveManager::AllocIndex(uint16 size)
{
	int16 ret=-1;
	if(size){
		BYTE *TempBuffer = new BYTE[size];
		if(TempBuffer){
			FILE *TblFp = fopen(ObserveTableFile,"rb+");
			FILE *DataFp = fopen(ObserveDataFile,"rb");
			FILE *TmpFp = fopen(ObserveTempFile,"wb");
			if(TblFp&&DataFp&&TmpFp){
				OBSREFENTRY refentry;
				uint16 refsize;
				for(int16 i=0;i<MaxRefEntryCount;i++){
					fseek(TblFp,sizeof(refentry)*i,SEEK_SET);
					fread(&refentry,sizeof(refentry),1,TblFp);
					refsize=refentry.size;
					if(ret<0&&!refsize&&refentry.prev<0&&refentry.next<0){
						fseek(TblFp,sizeof(refentry)*i,SEEK_SET);
						refentry.size=size;
						fwrite(&refentry,sizeof(refentry),1,TblFp);
						ret = i;
						memset(TempBuffer,0,size);
						fwrite(TempBuffer,size,1,TmpFp);
					}
					else if(refsize){
						BYTE ch;
						for(uint16 j=0;j<refsize;j++){
							ch=getc(DataFp);
							putc(ch,TmpFp);
						}
					}
				}
				fclose(TblFp);
				fclose(DataFp);
				fclose(TmpFp);
			}
			else{
				if(TblFp) fclose(TblFp);
				if(DataFp) fclose(DataFp);
				if(TmpFp) fclose(TmpFp);
			}
			delete[] TempBuffer;
			remove(ObserveDataFile);
			rename(ObserveTempFile,ObserveDataFile);
		}
	}
	return ret;
}

BOOL  ObserveManager::ReallocIndex(uint16 index,uint16 size)
{
	BOOL ret=FALSE;
	if(index<MaxRefEntryCount){
		OBSREFENTRY refentry;
		GetRefEntry(index,refentry);
		if(refentry.size&&size){
			BYTE *TempBuffer = new BYTE[size];
			if(TempBuffer){
				FILE *TblFp = fopen(ObserveTableFile,"rb+");
				FILE *DataFp = fopen(ObserveDataFile,"rb");
				FILE *TmpFp = fopen(ObserveTempFile,"wb");
				if(TblFp&&DataFp&&TmpFp){
					uint16 refsize;
					for(int16 i=0;i<MaxRefEntryCount;i++){
						fseek(TblFp,sizeof(refentry)*i,SEEK_SET);
						fread(&refentry,sizeof(refentry),1,TblFp);
						refsize=refentry.size;
						if(i==index){
							fseek(TblFp,sizeof(refentry)*i,SEEK_SET);
							refentry.size=size;
							fwrite(&refentry,sizeof(refentry),1,TblFp);
							ret = TRUE;
							memset(TempBuffer,0,size);
							uint16 minsize = refsize<size? refsize:size;
							for(uint16 j=0;j<minsize;j++)
								TempBuffer[j]=getc(DataFp);
							fwrite(TempBuffer,size,1,TmpFp);
						}
						else if(refsize){
							BYTE ch;
							for(uint16 j=0;j<refsize;j++){
								ch=getc(DataFp);
								putc(ch,TmpFp);
							}
						}
					}
					fclose(TblFp);
					fclose(DataFp);
					fclose(TmpFp);
				}
				else{
					if(TblFp) fclose(TblFp);
					if(DataFp) fclose(DataFp);
					if(TmpFp) fclose(TmpFp);
				}
				delete[] TempBuffer;
				remove(ObserveDataFile);
				rename(ObserveTempFile,ObserveDataFile);
			}
		}
	}
	return ret;
}

BOOL ObserveManager::FreeIndex(uint16 index)
{
	BOOL ret=FALSE;
	if(index<MaxRefEntryCount){
		OBSREFENTRY refentry,temprefentry;
		GetRefEntry(index,refentry);
		if(refentry.size){
			FILE *TblFp = fopen(ObserveTableFile,"rb+");
			FILE *DataFp = fopen(ObserveDataFile,"rb");
			FILE *TmpFp = fopen(ObserveTempFile,"wb");
			if(TblFp&&DataFp&&TmpFp){
				uint16 refsize;
				for(int16 i=0;i<MaxRefEntryCount;i++){
					fseek(TblFp,sizeof(refentry)*i,SEEK_SET);
					fread(&refentry,sizeof(refentry),1,TblFp);
					refsize=refentry.size;
					if(i==index){
						int16 prev=refentry.prev;
						int16 next=refentry.next;
						if(prev>=0){
							fseek(TblFp,sizeof(refentry)*prev,SEEK_SET);
							fread(&temprefentry,sizeof(temprefentry),1,TblFp);
							temprefentry.next=next;
							fseek(TblFp,sizeof(refentry)*prev,SEEK_SET);
							fwrite(&temprefentry,sizeof(temprefentry),1,TblFp);
						}
						if(next>=0){
							fseek(TblFp,sizeof(refentry)*next,SEEK_SET);
							fread(&temprefentry,sizeof(temprefentry),1,TblFp);
							temprefentry.prev=prev;
							fseek(TblFp,sizeof(refentry)*next,SEEK_SET);
							fwrite(&temprefentry,sizeof(temprefentry),1,TblFp);
						}
						fseek(TblFp,sizeof(refentry)*i,SEEK_SET);
						refentry.size=0;
						refentry.prev=refentry.next=-1;
						fwrite(&refentry,sizeof(refentry),1,TblFp);
						ret = TRUE;
						fseek(DataFp,refsize,SEEK_CUR);
					}
					else if(refsize){
						BYTE ch;
						for(uint16 j=0;j<refsize;j++){
							ch=getc(DataFp);
							putc(ch,TmpFp);
						}
					}
				}
				fclose(TblFp);
				fclose(DataFp);
				fclose(TmpFp);
			}
			else{
				if(TblFp) fclose(TblFp);
				if(DataFp) fclose(DataFp);
				if(TmpFp) fclose(TmpFp);
			}
			remove(ObserveDataFile);
			rename(ObserveTempFile,ObserveDataFile);
		}
	}
	return ret;
}

void ObserveManager::FreeAll(int16 index)
{
	int16 previndex;
	index=FindHead(index);
	while(index>=0){
		previndex = GetNextIndex(index);
		if(!FreeIndex(index)) break;
		index=previndex;
	}
}

//==================================================================//
uint16 ObserveManager::GetObserveData(uint16 index,BYTE *date,BYTE *& observe,BYTE *& treat)
{
	uint16 getsize=0;
	if(index<MaxRefEntryCount){
		FILE *fp=fopen(ObserveDataFile,"rb");
		if(fp){
			int32 pos=CalcDataPosition(index);
			fseek(fp,pos,SEEK_SET);
			if(date){
				uint16 datesize=10;
				uint16 observesize, treatsize;
				fread(&observesize,sizeof(observesize),1,fp);
				fread(&treatsize,sizeof(treatsize),1,fp);

				if(datesize) fread(date,datesize,1,fp);
				date[datesize]=0;
				observe=date+(datesize+1);
				fread(observe,observesize,1,fp);
				if(observesize) observe[observesize]=0;
				treat=observe+(observesize+1);
				if(treatsize) fread(treat,treatsize,1,fp);
				treat[treatsize]=0;
				getsize=sizeof(uint16)*2+datesize+observesize+treatsize;
			}
			fclose(fp);
		}
	}
	return getsize;
}

uint16 ObserveManager::PutObserveData(uint16 index,BYTE *date,BYTE *observe,BYTE *treat)
{
	uint16 putsize=0;
	if(index<MaxRefEntryCount){
		FILE *fp=fopen(ObserveDataFile,"rb+");
		if(fp){
			int32 pos=CalcDataPosition(index);
			fseek(fp,pos,SEEK_SET);
			if(date&&observe&&treat){
				uint16 datesize=10;
				uint16 observesize=strlen(observe);
				uint16 treatsize=strlen(treat);
				fwrite(&observesize,sizeof(observesize),1,fp);
				fwrite(&treatsize,sizeof(treatsize),1,fp);

				if(datesize) fwrite(date,datesize,1,fp);
				if(observesize) fwrite(observe,observesize,1,fp);
				if(treatsize) fwrite(treat,treatsize,1,fp);
				putsize=sizeof(uint16)*2+datesize+observesize+treatsize;
			}
			fclose(fp);
		}
	}
	return putsize;
}

int16 ObserveManager::AddObserveMan(ObserveManager *obsman,int16 index)
{
	int16 firstallocindex=-1,preallocindex=-1,allocindex;
	if(obsman){
		OBSREFENTRY refentry,prerefentry={0,-1,-1};
		index=obsman->FindHead(index);
		while(index>=0){
			obsman->GetRefEntry(index,refentry);
			if(refentry.size){
				BYTE *buffer=new BYTE[refentry.size+1+10],*observe,*treat;
				if(buffer){
					allocindex=AllocIndex(refentry.size);
					if(allocindex>=0){
						if(preallocindex>=0){
							GetRefEntry(preallocindex,prerefentry);
							prerefentry.next=allocindex;
							PutRefEntry(preallocindex,prerefentry);
						}

						obsman->GetObserveData(index,buffer,observe,treat);
						PutObserveData(allocindex,buffer,observe,treat);

						prerefentry.size=refentry.size;
						prerefentry.prev=preallocindex;
						prerefentry.next=-1;
						PutRefEntry(allocindex,prerefentry);

						preallocindex=allocindex;
						if(firstallocindex<0)	firstallocindex=allocindex;
					}
					delete[] buffer;
				}
			}
			index=refentry.next;
		}
	}
	return firstallocindex;
}

//=============================================================//
BOOL ObserveManager::PrintObserveData(uint16 index)
{
	if(index<MaxRefEntryCount){
		FILE *fp=fopen(ObserveDataFile,"rb");
		if(fp){
			int32 pos=CalcDataPosition(index);
			fseek(fp,pos,SEEK_SET);
			char date[11];
			uint16 datesize=10;
			uint16 observesize, treatsize;
			fread(&observesize,sizeof(observesize),1,fp);
			fread(&treatsize,sizeof(treatsize),1,fp);
			if(datesize) fread(date,datesize,1,fp);
			date[datesize]=0;

			if(!PrintMan->PrintString("< ¡e”q ·©¸a >\r\n")) { fclose(fp); return FALSE; }
			if(!PrintMan->PrintString(date)) { fclose(fp); return FALSE; }
			if(!PrintMan->PrintString("\r\n\r\n")) { fclose(fp); return FALSE; }
			if(!PrintMan->PrintString("< ¡e”q ¶w >\r\n")) { fclose(fp); return FALSE; }
			uint16 i;
			char ch;
			for(i=0;i<observesize;i++){
				ch=getc(fp);
				if(!PrintMan->PrintCharacter(ch)) { fclose(fp); return FALSE; }
			}
			if(!PrintMan->PrintString("\r\n\r\n")) { fclose(fp); return FALSE; }
			if(!PrintMan->PrintString("< ¹¡Ã¡£¹ ¸b¬÷¸a >\r\n")) { fclose(fp); return FALSE; }
			for(i=0;i<treatsize;i++){
				ch=getc(fp);
				if(!PrintMan->PrintCharacter(ch)) { fclose(fp); return FALSE; }
			}
			if(!PrintMan->PrintString("\r\n\r\n")) { fclose(fp); return FALSE; }
			fclose(fp);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL ObserveManager::PrintAll(uint16 index)
{
	if(index<MaxRefEntryCount){
		int16 index=FindHead(index);
		while(index>=0){
			if(!PrintObserveData(index)) return FALSE;
			index=GetNextIndex(index);
		}
		return TRUE;
	}
	return FALSE;
}