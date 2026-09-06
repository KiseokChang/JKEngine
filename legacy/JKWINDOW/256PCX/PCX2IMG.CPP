#include "resman.h"
#include "jkinit.h"
#include "jkmodule.h"
#include "pcx2img.h"

BYTE ColorConvTable256[256]={0,};

uint16 ReadPCXLine(FILE *fp,char *buf,int16 bytes,BOOL palchange)
{
	if(!fp || !buf) return 0;
	uint16 n=0;
	while(n<bytes){
		BYTE c=(fgetc(fp) & 0xff);
		if((c&0xc0)==0xc0) {
			BYTE i=c&0x3f;
			c=fgetc(fp);
			if(!palchange) c=ColorConvTable256[c];
			while(i--) buf[n++]=c;
		}
		else{
			if(!palchange) buf[n++]=ColorConvTable256[c];
			else buf[n++]=c;
		}
	}
	return n;
}



BOOL PCX2IMG(char *pcxfn,char *imgfn)
{
	if(!pcxfn || !imgfn || !strcmp(pcxfn,imgfn)) return FALSE;

	FILE *pcxfp=fopen(pcxfn,"rb");
	if (!pcxfp) return FALSE;

	FILE *imgfp=fopen(imgfn,"wb");
	if(!imgfp) { fclose(pcxfp);	return FALSE;	}

	fseek(pcxfp,-769L,2);
	BOOL is256 = (fgetc(pcxfp)==12)? TRUE:FALSE;
	PCXHDR  header;
	fseek(pcxfp,0,0);
	fread(&header,128,1,pcxfp);

	uint16 mode=NO_PCX;
	if(header.Manufacturer!=0x0a) mode= NO_PCX;
	else if(header.HRes==720 && header.VRes==348) mode=PCX_HERCULES;
	else if(header.NPlanes==4) mode=PCX_VGA16;
	else if(is256) mode=PCX_VGA256;
	if(mode!=PCX_VGA256) return FALSE;

	fseek(pcxfp,-768L,SEEK_END);
	BYTE tempRGB[256][3];
	fread(tempRGB,256*3,1,pcxfp);
	for (uint16 i=0;i<256;i++)
		ColorConvTable256[i]= FindNearestColor(tempRGB[i][0]>>2,tempRGB[i][1]>>2,tempRGB[i][2]>>2);


	uint16 width=header.x2-header.x1+1;
	uint16 height=header.y2-header.y1+1;
	uint16 temp;
	temp=width-1;
	fwrite(&temp,sizeof(temp),1,imgfp);
	temp=height-1;
	fwrite(&temp,sizeof(temp),1,imgfp);

	uint16 bytes=header.BytesperLine;
	BYTE *LineBuffer = new char[bytes];
	fseek(pcxfp,128,0);
	for(i=0;i<height;i++) {
		uint16 n=ReadPCXLine(pcxfp,LineBuffer,bytes);
		if(n>width) n=width;
		fwrite(LineBuffer,width,1,imgfp);
		writeimageline(0,i,LineBuffer,width,COPY_PUT);
	}
	delete[] LineBuffer;
	fclose(pcxfp);
	fclose(imgfp);
	return TRUE;
}

//=======================================================================

EventHandler *CreateApplication()
{
	return 0;
}

void main(int argc, char **argv)
{
	if(InitJKProgram()) {
		if(argc>=3)	{
			ResourceBase *res=new ResourceBase((WORD)0,(uint16)RES_PALETTE,(BYTE *)"jkpalett.dat",(BYTE)RT_FILE|RT_PACKFILE,(uint16)0);
			if(res){
				BYTE *data = res->AllocRealData();
				if(data) {
					::SetAllPalette(data);
					res->FreeRealData(data);

					PCX2IMG(argv[1],argv[2]);
					getch();
				}
				delete res;
			}
		}
		CloseJKProgram();
	}
}