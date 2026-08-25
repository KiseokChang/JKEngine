#ifndef _SELBATT_H
#define _SELBATT_H

#include "firerec.h"
#include "jkdialog.h"

typedef struct{
	uint16 distance;    //0 => Cannot Fire
	uint16 direction;       //0 => Cannot Fire
}FIRECAPABILITY;



class SelectFireBattalionDialog:public JKDialog
{
public:
	SelectFireBattalionDialog(JKPoint& p,FireRecord& firerec,uint16 tgtype,JKControl *parent=0);
 ~SelectFireBattalionDialog();

	void InitWindow();
	void SetupWindow();

  char *CreateListString(BattalionRecord& rec);
	void InitList();
  void AddList(int16 index,BOOL redraw=FALSE);

	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD /*ctrlid*/,uint16 /*index*/);
	void EvCommand(WORD cmdid,WORD data,DWORD,WORD);

	void EvClickPodaeControl(WORD ctrlid);

  void EvTargetSlected(WORD index);
	void DecideTempFireMan();

	BOOL GetFireCapability(BattalionRecord& battrec,JKPoint32 coord,FIRECAPABILITY& firecapable);

	FireRecord& FireRec;

	uint16 TargetType;

	WORD FireIndex;
};
#endif
