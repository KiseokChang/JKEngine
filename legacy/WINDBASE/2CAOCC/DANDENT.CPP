#include "dandent.h"

#define ResKindCount		2

static int32 LimitResID[ResKindCount][2]={
	{0x4000+0x200,0x4000+0x200+60},
	{0x4000+0x300,0x4000+0x300+79}
};

DandaehoEntry::DandaehoEntry(WORD entid,WORD reskind,char *name):
	NumberEntry(entid,name,LimitResID[reskind<ResKindCount? reskind:0][0],
							LimitResID[reskind<ResKindCount? reskind:0][1],10,
							LimitResID[reskind<ResKindCount? reskind:0][0]),
	ResKind(reskind<ResKindCount? reskind:0)
{}

DandaehoEntry::~DandaehoEntry()
{}

BOOL DandaehoEntry::SetResKind(WORD reskind)
{
	if(reskind<ResKindCount)	{
		ResKind=reskind;
		SetDataForm(LimitResID[ResKind][0],LimitResID[ResKind][1],10,LimitResID[ResKind][0]);
		return TRUE;
	}
  return FALSE;
}
