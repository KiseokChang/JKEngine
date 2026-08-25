#ifndef _EQUIPWIN_H
#define _EQUIPWIN_H

#include "inwonman.h"
#include "bombman.h"
#include "jkdialog.h"
#include "eqfilemn.h"
#include "eqpgloba.h"
#include "extnval.h"

#define EQPUIPMESSAGE	 USERMESSAGE+51
#define EQPUIPCONTROL  ID_USERCONTROL+51

static	char *Kind[16]={"HE","WP","ICM","DP-ICM","ILL","HEAT","RAP","RA-FAS",
					"5.56MM","¸w´¢","®…¤i¯¥‰Å","¯¡Ðe¯¥‰Å","¸ó‹e¯¥‰Å","‰ÅÉ·¯¥‰Å","’A‰Å",""};
static	char *Name[5]={DAEDAEHQ,DAEDAEA,DAEDAEB,DAEDAEC,""};

class EquipWindow:public JKDialog
{
public:
	EquipWindow(WORD ctrlid);
 ~EquipWindow();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);
	void SaveBomb();
	void InitBombData();
	void AfterHide();
	BOOL FindBomb(BOMB& bm);
	void InitOrgBomb();
	void InitEquipListBox(BOMB& bm);

	void EvCharDown(uint16 key,WORD modifier);
	void EvLBCharDown(WORD /*ctrlid*/,uint16 /*index*/,uint16 /*key*/,WORD /*modifier*/);
	void EvButtonClick(WORD ctrlid);
	void EvShowWindow();
	void EvCtrlOnlyMove(WORD ctrlid);
	void EvOnlyMove(JKPoint&);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	ProcessResult RespondMessage(JKMSG& msg);

	void InitTable();
	void ChangeDate();

	BombManager CurrentBomb;
	InwonManager IngaInwon;
	EquipGlobal global;

	BOMB OrgBomb,TempBomb;
	int16 KindNumber;
	int16 NameNumber;
	BOOL InitYear;

private:
	uint16 IsShowNum;
	BOOL DefaultSetFlag;
};

#endif