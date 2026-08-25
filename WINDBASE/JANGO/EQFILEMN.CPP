#include "eqfilemn.h"

EqiupFileManage::EqiupFileManage(char *filename=NULL):
	CreationError(FALSE),HeadSize(0)
{
	MakeFileName(filename);
}

EqiupFileManage::~EqiupFileManage()
{
	Close();
}

BOOL EqiupFileManage::ReadOpen()
{
	if((EqFP=fopen(FileName,"rb"))==NULL) {
		CreationError=TRUE;
		return FALSE;
	}
	return TRUE;
}

BOOL EqiupFileManage::WriteOpen()
{
	if((EqFP=fopen(FileName,"ab+"))==NULL) {
			CreationError=TRUE;
			return FALSE;
		}
	return TRUE;
}

BOOL EqiupFileManage::Read(BOMB& buf,int16 index=0)
{
	ReadOpen();
	if(CreationError) return FALSE;
	fseek(EqFP,sizeof(buf)*index,SEEK_SET);
	if(fread(&buf,sizeof(buf),1,EqFP)==0) return FALSE;
	Close();
	return TRUE;
}

BOOL EqiupFileManage::Append(BOMB& buf)
{
	if(WriteOpen()==0&&CreationError) return FALSE;
	fseek(EqFP,0,SEEK_END);
	fwrite(&buf,sizeof(buf),1,EqFP);
	Close();
	return TRUE;
}

BOOL EqiupFileManage::Modify(BOMB& buf,int16 index)
{
	if((EqFP=fopen(FileName,"rb+"))==NULL) {
		CreationError=TRUE;
		return FALSE;
	}
	fseek(EqFP,sizeof(buf)*index,SEEK_SET);
	fwrite(&buf,sizeof(buf),1,EqFP);

	Close();
	return TRUE;
}

BOOL EqiupFileManage::Delete(int16 index)
{
	FILE *tempfp;
	BOMB temp;
	int16 count=0;

	if(!ReadOpen()) return FALSE;
	if((tempfp=fopen("temp$$$.dat","wb+"))==NULL) return FALSE;
	fseek(EqFP,0,SEEK_SET);
	while(!feof(EqFP)) {
		if(count==index) {
			if((fseek(EqFP,sizeof(BOMB),SEEK_CUR))!=0) break;
		}
		if((fread(&temp,sizeof(BOMB),1,EqFP))==0) break;
		fwrite(&temp,sizeof(BOMB),1,tempfp);
		count++;
	}
	fclose(EqFP);
	fclose(tempfp);
	remove(BackFileName);
	rename(FileName,BackFileName);
	rename("temp$$$.dat",FileName);
	return TRUE;
}

void EqiupFileManage::Close()
{
	fclose(EqFP);
}

BOOL EqiupFileManage::Compare(BOMB& buf1,BOMB& buf2)
{
	if(strcmp(buf1.Name,buf2.Name)!=0) return FALSE;
	else if(strcmp(buf1.Kind,buf2.Kind)!=0) return FALSE;
	else if(strcmp(buf1.Dod,buf2.Dod)!=0) return FALSE;
	else if(strcmp(buf1.Lot,buf2.Lot)!=0) return FALSE;
	else if(strcmp(buf1.Born,buf2.Born)!=0) return FALSE;
	else if(strcmp(buf1.Place,buf2.Place)!=0) return FALSE;
	else if(strcmp(buf1.Use,buf2.Use)!=0) return FALSE;
	else if(strcmp(buf1.HaveYear,buf2.HaveYear)!=0) return FALSE;
	return TRUE;
}

void EqiupFileManage::MakeFileName(char *name)
{
	char tempname[8];

	if(name==0) {
		strcpy(FileName,"855equip.dat");
		strcpy(BackFileName,"855equip.old");
	}
	else {
		strcpy(FileName,name);
		strcpy(BackFileName,name);
		strcat(FileName,".dat");
		strcat(BackFileName,".old");
	}
}