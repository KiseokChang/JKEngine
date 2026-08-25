#include "jkinit.h"
#include "jkapp.h"
#include "jkmodule.h"
#include "256pcx.h"
//#include "16to256.h"

BYTE ColorConvTable256[256]={0,};

BOOL SkipPCXLine(FILE *fp,unsigned bytes,unsigned lines)
{
	if(!fp) return FALSE;

	while(lines--){
		unsigned n=0;
		while(n<bytes){
			BYTE c=(fgetc(fp) & 0xff);
			if((c&0xc0)==0xc0) {
				n+=c&0x3f;	c=fgetc(fp);
			}
			else n++;
		}
	}
	return TRUE;
}

unsigned ReadPCXLine(FILE *fp,char *buf,int bytes,BOOL palchange)
{
	if(!fp || !buf) return 0;
	unsigned n=0;
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



BOOL PutPCX(char *pcxfn,JKPoint p,JKRect rect,BYTE putmode,BOOL palchange)
{
	if(!pcxfn) return FALSE;
	FILE *fp=fopen(pcxfn,"rb");
	if (!fp) return FALSE;

	fseek(fp,-769L,2);
	BOOL is256 = (fgetc(fp)==12)? TRUE:FALSE;

	PCXHDR  header;
	fseek(fp,0,0);
	fread(&header,128,1,fp);

	unsigned mode=NO_PCX;

	if(header.Manufacturer!=0x0a) mode= NO_PCX;
	else if(header.HRes==720 && header.VRes==348) mode=PCX_HERCULES;
	else if(header.NPlanes==4) mode=PCX_VGA16;
	else if(is256) mode=PCX_VGA256;

	if(mode!=PCX_VGA256) return FALSE;


	int r,g,b;
	fseek(fp,-768L,2);
	for (unsigned i=0;i<256;i++) {
		r=fgetc(fp)>>2;
		g=fgetc(fp)>>2;
		b=fgetc(fp)>>2;
		if(!palchange)	ColorConvTable256[i]= FindNearestColor(r,g,b);
		else					SetPalette(i,r,g,b);
	}

	unsigned width=header.x2-header.x1+1;
	unsigned height=header.y2-header.y1+1;
	if(!rect.IsValid()) rect.Set(p.x,p.y,p.x+width-1,p.y+height-1);
	JKRect ImageRect(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=ImageRect.CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return FALSE;
	}
	rect=JKRect(0,0,getmaxx(),getmaxy()).CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return FALSE;
	}

	unsigned bytes=header.BytesperLine;
	BYTE *LineBuffer = new char[bytes];

	fseek(fp,128,0);
	SkipPCXLine(fp,bytes,rect.top-p.y);
	for(i=rect.top;i<=rect.bottom;i++) {
		unsigned n=ReadPCXLine(fp,LineBuffer,bytes,palchange);
		if(n>width) n=width;
		WriteImageLine(JKPoint(rect.left,i),LineBuffer+(rect.left-p.x),rect.Width()+1,putmode);
	}
	delete[] LineBuffer;
	fclose(fp);
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
		if(argc>=2) {
			BYTE putmode=0;
			ResourceBase *res=new ResourceBase(0,RES_PALETTE,"jkpalett.dat",RT_FILE|RT_PACKFILE,0);
			if(res){
				BYTE *data = res->AllocRealData();
				if(data) {
					::SetAllPalette(data);
					res->FreeRealData(data);

					PutPCX(argv[1],JKPoint(78,100),JKRect(0,0,-1,-1),putmode,FALSE);
					getch();
				}
				delete res;
			}
		}
		CloseJKProgram();
	}
}