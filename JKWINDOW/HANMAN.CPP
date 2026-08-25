#include <stdio.h>
#include <string.h>
#include "basefunc.h"
#include "keybdman.h"
#include "hanman.h"


int8 HangulManager::IndexHF[3][32]={
	{
		 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
		15,16,17,18,19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	{
		 0, 0, 0, 1, 2, 3, 4, 5, 0, 0, 6, 7, 8, 9,10,11,
		 0, 0,12,13,14,15,16,17, 0, 0,18,19,20,21, 0, 0
	},
	{
		 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
		15,16, 0,17,18,19,20,21,22,23,24,25,26,27, 0, 0
	}
};


int8 HangulManager::DLast[22]= {
	0,  0,2,0,2,1,2,1,2,3,0,2,1,3,3,1,2,1,3,3,1,1
};
int8 HangulManager::DMiddle[20*2]={
	0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,
	2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,3,3,3
};

int8 HangulManager::DFirst[22*2]={
	0,0,0,0,0,0,0,0,0,1,3,3,3,1,2,4,4,4,2,1,3,0,
	5,5,5,5,5,5,5,5,5,6,7,7,7,6,6,7,7,7,6,6,7,5
};

int8 HangulManager::HangulFontFile[13]="hangul.fnt";
int8 HangulManager::HanjaFontFile[13]="hanja.fnt";
int8 HangulManager::EngFontFile[13]="english.fnt";
int8 HangulManager::SpcFontFile[13]="special.fnt";

int8 HangulManager::RefHanCodeFile[13]="refhan.cod";


HangulManager::HangulManager():
	CreationError(FALSE),
	HanFirstFont(0),HanMiddleFont(0),HanLastFont(0),
	EnglishFont(0)
{
	FILE *fp;

	if(!CreationError){
		fp=fopen(RefHanCodeFile,"rb");
		if(fp) fclose(fp);
		else CreationError=TRUE;
	}

///////////////////////////////////////////////////////////

	if(!CreationError){
		HanFirstFont = new BYTE[8*20*32];
		HanMiddleFont = new BYTE[4*22*32];
		HanLastFont = new BYTE[4*28*32];

		if(HanFirstFont&&HanMiddleFont&&HanLastFont){
			fp=fopen(HangulFontFile,"rb");
			if(fp){
				memset(HanFirstFont,0,8*20*32);
				memset(HanMiddleFont,0,4*22*32);
				memset(HanLastFont,0,4*28*32);

				fread(HanFirstFont,8*20*32,1,fp);
				fread(HanMiddleFont,4*22*32,1,fp);
				fread(HanLastFont,4*28*32,1,fp);
				fclose(fp);
			}
			else CreationError=TRUE;
		}
		else CreationError=TRUE;
	}
	if(!CreationError){
		EnglishFont= new BYTE[256*16];
		if(EnglishFont){
			fp=fopen(EngFontFile,"rb");
			if(fp){
				memset(EnglishFont,0,256*16);
				fread(EnglishFont,256*16,1,fp);
				fclose(fp);
			}
			else CreationError=TRUE;
		}
		else CreationError=TRUE;
	}
}

HangulManager::~HangulManager()
{
	if(HanFirstFont)	delete[] HanFirstFont;
	if(HanMiddleFont) delete[] HanMiddleFont;
	if(HanLastFont)   delete[] HanLastFont;
	if(EnglishFont)   delete[] EnglishFont;
}

//================================================================

WORD HangulManager::RefHanCode(WORD a,WORD b)
{
	FILE *fp=fopen(RefHanCodeFile,"rb");
	if(fp){
		fseek(fp,int32(3*a+b)*sizeof(WORD),SEEK_SET);
		WORD value;
		fread(&value,sizeof(WORD),1,fp);
		fclose(fp);
		return value;
	}
	return 0;
}

//===========================================================

WORD HangulManager::HanjaPos2HanjaCode(WORD hanjapos)
{
	 BYTE firstbyte = 0xe0+hanjapos/0xBC;
	 BYTE secondbyte = hanjapos%0xBC;
	 if(secondbyte<0x4E) secondbyte += 0x31;
	 else secondbyte += 0x43;
	 return MAKEWORD(secondbyte,firstbyte);
}

WORD HangulManager::HanjaPos2HangulCode(WORD hanjapos)
{
	for(int16 i=0; i<471; i++) {
		if(hanjapos<RefHanCode(i,2)+RefHanCode(i,1)&&hanjapos>=RefHanCode(i,2))	{
			return RefHanCode(i,0);
		}
	}
	return 0;
}

int16 HangulManager::HanjaCode2HanjaPos(WORD hanjacode)
{
	BYTE firstbyte  = HIBYTE(hanjacode);
	BYTE secondbyte = LOBYTE(hanjacode);
	if(firstbyte>=0xE0&&firstbyte<=0xF9){
		firstbyte-=0xE0;
		if(secondbyte>=0x31&&secondbyte<=0x7E){
			secondbyte-=0x31;
		}
		else if(secondbyte>=0x91&&secondbyte<=0xFE){
			secondbyte-=0x91;
			secondbyte+=0x4E;
		}
		else return -1;
		return (int16)firstbyte*0xBC+secondbyte;
	}
	return -1;
}

int16 HangulManager::HangulCode2HanjaIndex(WORD hangulcode)
{
	for(int16 i=0;i<471;i++) {
		if(hangulcode == RefHanCode(i,0))	{
			return i;
		}
	}
	return -1;
}


WORD HangulManager::HanjaCode2HangulCode(WORD hanjacode)
{
	int16 hanjapos=HanMan->HanjaCode2HanjaPos(hanjacode);
	if(hanjapos>=0)
		return HanMan->HanjaPos2HangulCode(hanjapos);
	return 0;
}


BOOL HangulManager::IsHangul1st(BYTE *s, int16 pos,BOOL includehanja)
{
	WORD len=strlen(s);
	BYTE ch=s[pos];
	if(len && pos<len-1 && ch&0x80 && !IsHangul2nd(s,pos)){
		if(ch<212 ||(ch>=224&&includehanja))
			return TRUE;
	}
	return FALSE;
}

BOOL HangulManager::IsHangul2nd(BYTE *s, int16 pos)
{
	WORD i;
	for(i = 0; i < pos;){
		if(s[i]&0x80) i+=2;
		else          i++;
	}
	return i == pos? FALSE:TRUE;
}

BOOL HangulManager::IsHanja(BYTE ch1,BYTE ch2)
{
	if((ch1>=0xe0 && ch1<=0xfa) &&
			((ch2>=0x31 && ch2<=0x7e)||(ch2>=0x91 && ch2<=0xfe)))
		return TRUE;
	return FALSE;
}

BOOL HangulManager::IsGraphic(BYTE ch1,BYTE ch2)
{
	WORD fontID[]={ 0xd480,0xd931,0xda31,0xdb31,0xdc31,0xdd31,0xde31};
	BYTE hc1,hc2;

	for(WORD i=0;i<7;i++) {
		hc1=(fontID[i]>>8)&0xff;
		hc2=(fontID[i]&0xff);
		if(ch1==hc1 && ch2>=hc2) return TRUE;
	}
	return FALSE;
}

BOOL HangulManager::IsSpecial(BYTE ch1,BYTE /*ch2*/)
{
	if(ch1==212) return TRUE;
	return FALSE;
}
////////////////////////////////////////////

BOOL HangulManager::GetWORDImage(BYTE *buffer,BYTE ch1,BYTE ch2,BOOL onlyeng)
{
	if(!buffer) return FALSE;

	if(ch1&0x80 && !onlyeng) {
		if(IsSpecial(ch1,ch2))		GetSpecialImage(buffer,ch2);
		else if(IsHanja(ch1,ch2)) GetHanjaImage(buffer,ch1,ch2);
		else                      GetHangulImage(buffer,ch1,ch2);
	}
	else{
		WORD i=0;
		while(i<16){
			*(buffer+i*2)=*(EnglishFont+ch1*16+i);
			i++;
			*(buffer+i*2+1)=*(EnglishFont+ch2*16+i);
			i++;
		}
	}
	return TRUE;
}

BOOL HangulManager::GetEnglishImage(BYTE *buffer,BYTE ch)
{
	if(!buffer) return FALSE;
	memcpy(buffer,EnglishFont+ch*16,16);
	return TRUE;
}

BOOL HangulManager::GetSpecialImage(BYTE *buffer,BYTE ch)
{
	if(!buffer) return FALSE;
	FILE *fp=fopen(SpcFontFile,"rb");
	if(fp) {
		fseek(fp,ch*32L,SEEK_SET);
		fread(buffer,32,1,fp);
		fclose(fp);
	}
	else memset(buffer,0,32);
	return TRUE;
}

BOOL HangulManager::GetHanjaImage(BYTE *buffer,BYTE ch1,BYTE ch2)
{
	if(!buffer) return FALSE;
	FILE *fp=fopen(HanjaFontFile,"rb");
	if(fp) {
		int16 pos=HanjaCode2HanjaPos(MAKEWORD(ch2,ch1));
		if(pos!=-1)	{
			fseek(fp,pos*32L,SEEK_SET);
			fread(buffer,32,1,fp);
		}
		else memset(buffer,0,32);
		fclose(fp);
	}
	else memset(buffer,0,32);
	return TRUE;
}

BOOL HangulManager::GetHangulImage(BYTE *buffer,BYTE ch1,BYTE ch2)
{
	if(!buffer) return FALSE;

	BYTE fcode = ((ch1&0x7f)>>2)&0x1f;
	BYTE mcode = ((ch1&0x03)<<3|ch2>>5)&0x1f;
	BYTE lcode = ch2&0x1f;

	fcode=IndexHF[0][fcode];
	mcode=IndexHF[1][mcode];
	lcode=IndexHF[2][lcode];

	int16 f3=DLast[mcode];
	int16 f2,f1;
	if(lcode){
		f2=DMiddle[fcode+20];
		f1=DFirst[mcode+22];
	}
	else{
		f2=DMiddle[fcode];
		f1=DFirst[mcode];
	}

	memset(buffer,0,32);
	if(fcode) OrHangulImage(buffer,HanFirstFont+(f1*20+fcode)*32);
	if(mcode)	OrHangulImage(buffer,HanMiddleFont+(f2*22+mcode)*32);
	if(lcode) OrHangulImage(buffer,HanLastFont+(f3*28+lcode)*32);
	return TRUE;
}

void HangulManager::OrHangulImage(int8 *dest, int8 *src)
{
	if(dest&&src)
		for(int16 i=0;i<32;i++) *(dest+i)|=*(src+i);
}

//======================================//
HangulManager *HanMan=0;
//======================================//