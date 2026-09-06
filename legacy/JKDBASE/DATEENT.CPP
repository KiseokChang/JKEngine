#include <dos.h>
#include <stdlib.h>
#include "dateent.h"

DateEntry::DateEntry(WORD entid,int8 *name,struct date *refdate):
	EntryBase(entid,name,10)
{
	strcpy(SpaceBuffer,"    .  .  ");
	if(refdate) SetDate(*refdate);
	else SetCurrentDay();
}

EntryBase *DateEntry::Duplicate()
{
	return CreateEqualEntry(new DateEntry(EntryID,EntryName));
}

void DateEntry::SetCurrentDay()
{
	int8 buf[11];
	struct date curdate;
	getdate(&curdate);
	sprintf(buf,"%4d.%2d.%2d",curdate.da_year,curdate.da_mon,curdate.da_day);
	SetData(buf);
}

BYTE DateEntry::SetDate(struct date& refdate)
{
	return DateToStr(DataBuffer,refdate);
}

BOOL DateEntry::IsValidData(BYTE *data)
{
	struct date InputDate={0,0,0};
	BYTE reult=StrToDate(data,InputDate);
	if(reult!=0x08) {
		DateToStr(data,InputDate);
		return TRUE;
	}
	return FALSE;
}

int16 DateEntry::Compare(EntryBase *ent,uint16)
{
	int16 result=2;
	if(IsEqual(ent)){
		BOOL empty1=IsEmpty();
		BOOL empty2=ent->IsEmpty();
		result = empty1? (empty2? 2:-1):(empty2? 1:0);
		if(!result){
			DateEntry *other=(DateEntry *)ent;
			if(other) {
				struct date thisdate,otherdate;
				BYTE thisret=StrToDate(DataBuffer,thisdate);
				BYTE otherret=StrToDate(other->DataBuffer,otherdate);
				BYTE commonret=thisret|otherret;
				if(commonret&0x08){
					return (thisret&0x08)? ((otherret&0x08)? 2:-1):1;
				}

				if(thisdate.da_year>otherdate.da_year) result=1;
				else if(thisdate.da_year<otherdate.da_year) result=-1;
				if(result) return result;

				if(thisdate.da_mon>otherdate.da_mon) result=1;
				else if(thisdate.da_mon<otherdate.da_mon) result=-1;
				if(result) return result;
				if(thisdate.da_day>otherdate.da_day) result=1;
				else if(thisdate.da_day<otherdate.da_day) result=-1;
			}
		}
	}
	return result;
}

BYTE DateEntry::StrToDate(int8 *str,struct date& refdate)
{
	struct date temp={0,0,0};
	refdate=temp;
	if(!str) return 0x0f;

	int8 *buffer = new int8[BufferSize+1];
	strncpy(buffer,str,BufferSize);
	buffer[BufferSize]=0;

	int8 *delim[3];
	delim[0]=strchr(buffer,'.');
	if(delim[0]){
		*delim[0]=0;
		refdate.da_year=atoi(buffer);
		delim[1]=strchr(delim[0]+1,'.');
		if(delim[1]){
			*delim[1]=0;
			refdate.da_mon=atoi(delim[0]+1);
			delim[2]=strchr(delim[1]+1,'.');
			if(delim[2]) delim[2]=0;
			refdate.da_day=atoi(delim[1]+1);
		}
	}
	delete[] buffer;

	return ValidDate(refdate);
}

BYTE DateEntry::DateToStr(int8 *str,struct date& refdate)
{
	if(!str) return 0x0f;

	BYTE reult=ValidDate(refdate);
	if(reult&0x01) strcpy(str,"    .");
	else sprintf(str,"%4d.",refdate.da_year);
	if(reult&0x02) strcpy(str+5,"  .");
	else sprintf(str+5,"%2d.",refdate.da_mon);
	if(reult&0x02) strcpy(str+8,"  ");
	else sprintf(str+8,"%2d",refdate.da_day);
	return reult;
}

BYTE DateEntry::ValidDate(struct date& refdate)
{
	static int16 days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	BYTE result=0x00;
	if(refdate.da_year>0) {
		if(refdate.da_mon>0 && refdate.da_mon<=12){
			int16 year=refdate.da_year;
			days[1]=(!(year%4) && year%100 && !(year%400))? 29:28;
			if(refdate.da_day>0 && refdate.da_day<=days[refdate.da_mon]) result=0x00;
			else	result=0x04;
		}
		else result=0x06;
	}
	else result=0x07;
	return result;
}