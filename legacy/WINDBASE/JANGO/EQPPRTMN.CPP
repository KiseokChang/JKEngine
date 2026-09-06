#include "eqpprtmn.h"
#include "jkbutton.h"
#include <bios.h>

Equip24PrintManager::Equip24PrintManager(int16 port):PORT(port)
{

}

Equip24PrintManager::~Equip24PrintManager()
{
}

int16 Equip24PrintManager::GetPrintStatus()
{
	return biosprint(3,0,PORT);
}

void Equip24PrintManager::InitPrint()
{
	biosprint(1,0,0);
}

void Equip24PrintManager::PPutch(char ch)
{
	int16 result=GetPrintStatus();
	if(!(result&0x28))	biosprint(0,ch,PORT);
}

void Equip24PrintManager::PPutnch(char ch,int16 count)
{
	for(int16 i=0;i<count;i++) PPutch(ch);
}

void Equip24PrintManager::PPuts(char *s)
{
	while(*s){
		PPutch(*(s++));
	}
}

void Equip24PrintManager::PPuts(char *s,int16 offset, int16 fromwhere)
{
	int16 len=strlen(s);
	offset=(len-fromwhere<offset)?len-fromwhere:offset;
	for(int16 i=fromwhere;i<offset;i++)	PPutch(*(s+i));
}

void Equip24PrintManager::FormFeed()
{
	PPutch(FF);
}

void Equip24PrintManager::LineFeed()
{
	PPutch(CR);
	PPutch(LF);
}
