#include <ctype.h>
#include <string.h>
#include "basefunc.h"


void DeleteWhiteSpace(char *str)
{
	if(!str) return;
	BOOL IsHan=FALSE;
	char *temp1=str,*temp2=str;
	while(*temp1){
		if(IsHan || (*temp1!=' '&&*temp1!='\t')){
			*(temp2++)=*temp1;
			if(!IsHan && *temp1&0x80) IsHan=TRUE;
			else IsHan=FALSE;
		}
		temp1++;
	}
	*temp2=0;
}

char *StrChrNot(char *str,char ch)
{
	if(str){
		char *temp=str;
		while(*temp){
			if(*temp!=ch) return temp;
			temp++;
		}
	}
	return 0;
}

char *StrChrNotReverse(char *str,char ch)
{
	if(str){
		WORD len=strlen(str);
		if(len){
			char *temp=str+len;
			do {
				if(*--temp!=ch) return temp;
			} while(str!=temp);
		}
	}
	return 0;
}


BOOL StrMaskPicture(char *str,char *picture,char mask)
{
	if(str&&picture){
		BOOL IsHan=FALSE;
		while(*str){
			if(!IsHan){
				if(strchr(picture,*str)) *str=mask;
				else {
					if(*str&0x80) IsHan=TRUE;
				}
			}
			else IsHan=FALSE;
			str++;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL StrMaskNotPicture(char *str,char *picture,char mask)
{
	if(str&&picture){
		BOOL IsHan=FALSE;
		while(*str){
			if(!IsHan){
				if(!strchr(picture,*str)) {
					*str=mask;
					if(*str&0x80) IsHan=TRUE;
				}
			}
			else IsHan=FALSE;
			str++;
		}
		return TRUE;
	}
	return FALSE;
}

#define ADJUSTRIGHT		0
#define ADJUSTLEFT		1

void AdjustString(char *str,BYTE /*adjflag*/)
{
	if(!str) return;
	WORD size=strlen(str);
	char *temp=new char[size+1];
	strncpy(temp,str,size);
	temp[size]='\0';
/*	if(adjflag==ADJUSTRIGHT){
		char *realstr1=StrChrNot(temp,' ');
		if(realstr1) {
			char *realstr2=StrChrNotReverse(temp,' ');
			if(realstr2){
				INT len=(INT)(realstr2-realstr1)+1;
				if(len>0){
					if(len<size){
						if(adjflag==ADJUSTLEFT){
							memset(str+len,' ',size-len);
							strncpy(str,realstr1,len);
						}
						else{
							memset(str,' ',size-len);
							strncpy(str+(size-len),realstr1,len);
						}
					}
					else{
						strncpy(str,realstr1,size);
					}
					str[size]='\0';
				}
			}
		}
	}
	else if(adjflag==ADJUSTLEFT){
		char *realstr1=StrChrNot(temp,' ');
		if(realstr1) strcpy(str,realstr1);
		str[size]='\0';
	}
	else{
		WORD len=strlen(temp);
		if(len<size){
			strncpy(str,temp,len);
			memset(str+len,' ',size-len);
		}
		else{
			strncpy(str,temp,size);
		}
		str[size]='\0';
	}
*/
	delete[] temp;
}


