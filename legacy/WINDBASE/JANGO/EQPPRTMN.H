#ifndef _EQPPRTMN_H
#define _EQPPRTMN_H

#include "jkwindef.h"

//#include "eqfilemn.h"
//#include "printman.h"

class Equip24PrintManager
{
public:
	Equip24PrintManager(int16 port=0);
	~Equip24PrintManager();
	int16 GetPrintStatus();
	void FormFeed();
	void LineFeed();
	void InitPrint();
	void PPutch(char ch);
	void PPutnch(char ch,int16 count=0);
	void PPuts(char *s);
	void PPuts(char *s,int16 offset, int16 fromwhere=0);
	void ChangePort(int16 port){PORT=port;}
	int16 GetPort(){return PORT;}

private:
	int16 PORT;
};
#endif