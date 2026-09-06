#include <stdio.h>
#include "joglobal.h"



char BudaeName[3+1]="855";

char PasswordTable[PasswordCount][8+1] = {
	"855fa2ba",
	"855fa2ba",
	"855fa2ba",
	"855fa2ba",
	"855fa2ba",
	"855fa2ba",
	"855fa2ba",
	"855fa2ba",
	"855fa2ba",
	"855fa2ba"
};

BOOL InitGlobal()
{
	FILE *fp=fopen("joconfig.855","rb");
	if(fp){
		fread(BudaeName,3*sizeof(char),1,fp);
		BudaeName[3]=0;
		for(uint16 i=0;i<PasswordCount;i++){
			fread(PasswordTable[i],8*sizeof(char),1,fp);
			PasswordTable[i][8]=0;
		}
		fclose(fp);
		return TRUE;
	}
	return FALSE;
}

void WriteGlobal()
{
	FILE *fp=fopen("joconfig.855","wb");
	if(fp){
		fwrite(BudaeName,3*sizeof(char),1,fp);
		for(uint16 i=0;i<PasswordCount;i++){
			fwrite(PasswordTable[i],8*sizeof(char),1,fp);
		}
		fclose(fp);
	}
}