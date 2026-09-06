#include "eqpplace.h"
#include "extnval.h"

EqpPlace::EqpPlace(char* filename)
{
	if(filename) {
		strcpy(FileName,filename);
		FileName[12]=0;
	}
	else strcpy(FileName,PLACENAME);
}

EqpPlace::~EqpPlace()
{}

int16 EqpPlace::Read(char& buf,int16 index)
{
	 FILE *PlaceFp;
	 if((PlaceFp=fopen(FileName,"rb+"))==NULL) return 0;
	 fseek(PlaceFp,15*index,SEEK_SET);
	 if(fread(&buf,15,1,PlaceFp)==0) return 0;
	 fclose(PlaceFp);
	 return 1;
}
int16 EqpPlace::Write(char& str)
{
	 FILE *PlaceFp;
	 char Place[15];
	 if((PlaceFp=fopen(FileName,"ab+"))==NULL) return FALSE;
	 fwrite(&str,15,1,PlaceFp);
	 fclose(PlaceFp);
	 return TRUE;
}