#include <dos.h>
#include <stdlib.h>
#include "timeent.h"

TimeEntry::TimeEntry(WORD entid,char *name,struct time *reftime):
	EntryBase(entid,name,11)
{
	strcpy(SpaceBuffer,"  :  :  .  ");
	if(reftime) SetTime(*reftime);
	else SetCurrentTime();
}

void TimeEntry::SetCurrentTime()
{
	int8 buf[12];
	struct time curtime;
	gettime(&curtime);
	sprintf(buf,"%02d:%02d:%02d.%02d",curtime.ti_hour,curtime.ti_min,curtime.ti_sec,curtime.ti_hund);
	SetData(buf);
}

EntryBase *TimeEntry::Duplicate()
{
	return CreateEqualEntry(new TimeEntry(EntryID,EntryName));
}

BYTE TimeEntry::SetTime(struct time& reftime)
{
	return TimeToStr(DataBuffer,reftime);
}

BOOL TimeEntry::IsValidData(BYTE *data)
{
	struct time InputTime={0,0,0};
	BYTE reult=StrToTime(data,InputTime);
	if(reult!=0x10) {
		TimeToStr(data,InputTime);
		return TRUE;
	}
	return FALSE;
}

int16 TimeEntry::Compare(EntryBase *ent,uint16)
{
	int16 result=2;
	if(IsEqual(ent)){
		BOOL empty1=IsEmpty();
		BOOL empty2=ent->IsEmpty();
		result = empty1? (empty2? 2:-1):(empty2? 1:0);
		if(!result){
			TimeEntry *other=(TimeEntry *)ent;
			if(other) {
				struct time thistime,othertime;
				BYTE thisret=StrToTime(DataBuffer,thistime);
				BYTE otherret=StrToTime(other->DataBuffer,othertime);
				BYTE commonret=thisret|otherret;
				if(commonret&0x10)
					return (thisret&0x10)? ((otherret&0x10)? 2:-1):1;

				if(thistime.ti_hour>othertime.ti_hour) result=1;
				else if(thistime.ti_hour<othertime.ti_hour) result=-1;
				if(result) return result;

				if(thistime.ti_min>othertime.ti_min) result=1;
				else if(thistime.ti_min<othertime.ti_min) result=-1;
				if(result) return result;

				if(thistime.ti_sec>othertime.ti_sec) result=1;
				else if(thistime.ti_sec<othertime.ti_sec) result=-1;
				if(result) return result;

				if(thistime.ti_hund>othertime.ti_hund) result=1;
				else if(thistime.ti_hund<othertime.ti_hund) result=-1;
			}
		}
	}
	return result;
}

BYTE TimeEntry::StrToTime(int8 *str,struct time& reftime)
{
	struct time temp={0,0,0,0};
	reftime=temp;
	if(!str) return 0x10;

	int8 *buffer = new int8[BufferSize+1];
	strncpy(buffer,str,BufferSize);
	buffer[BufferSize]=0;

	int8 *delim[4];
	delim[0]=strchr(buffer,':');
	if(delim[0]){
		*delim[0]=0;
		delim[1]=strchr(delim[0]+1,':');
		if(delim[1]){
			*delim[1]=0;
			delim[2]=strchr(delim[1]+1,'.');
			if(delim[2]) {
				*delim[2]=0;
				reftime.ti_hund=atoi(delim[2]+1);
			}
			reftime.ti_sec=atoi(delim[1]+1);
		}
		reftime.ti_min=atoi(delim[0]+1);
	}
	reftime.ti_hour=atoi(buffer);
	delete[] buffer;

	return ValidTime(reftime);
}

BYTE TimeEntry::TimeToStr(int8 *str,struct time& reftime)
{
	if(!str) return 0x10;

	BYTE reult=ValidTime(reftime);
	if(reult&0x01) strcpy(str,"00:");
	else sprintf(str,"%02d:",reftime.ti_hour);
	if(reult&0x02) strcpy(str+3,"00:");
	else sprintf(str+3,"%02d:",reftime.ti_min);
	if(reult&0x04) strcpy(str+6,"00.");
	else sprintf(str+6,"%02d.",reftime.ti_sec);
	if(reult&0x08) strcpy(str+9,"00");
	else sprintf(str+9,"%02d",reftime.ti_hund);
	return reult;
}

BYTE TimeEntry::ValidTime(struct time& reftime)
{
	BYTE result=0x00;
	if(reftime.ti_hour>=24)		result|=0x01;
	if(reftime.ti_min>=60) 		result|=0x02;
	if(reftime.ti_sec>=60) 	 	result|=0x03;
	if(reftime.ti_hund>=100)	result|=0x04;
	return result;
}