#ifndef _FHEADER_H
#define _FHEADER_H

#include <stdio.h>
#include "keydef.h"

class FILEHEADER
{
public:

	WORD RecordID;
	uint16 AllocCount;
	uint16 RealCount;

	BOOL GetFileHeader(char *fname)
	{
		FILE *fp = fopen(fname,"rb");
		if(fp){
			fread(&RecordID,sizeof(RecordID),1,fp);
			fread(&AllocCount,sizeof(AllocCount),1,fp);
			fread(&RealCount,sizeof(RealCount),1,fp);
			fclose(fp);
			return TRUE;
		}
		return FALSE;
	}
};
#endif