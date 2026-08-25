#include "eqpprint.h"
#include "jkbutton.h"
#include "extnval.h"

EquipPrint::EquipPrint():
	JKDialog(JKRect(200,100,450,300),0),SomoFlag(FALSE)
{
	WinForm=0x03;
  SetText("ÏaŸ¥Ëa Â‰b Àw");
}

EquipPrint::~EquipPrint()
{}

void EquipPrint::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=rect.Width()/2-80;
	rect.right=rect.left+60;
	rect.top=rect.bottom-50;
	rect.bottom=rect.top+30;
	JKButton *btn=new JKButton(rect,ID_BUTTON);
	btn->SetText("ÑÂ·¥");
	AddControl(btn);

	rect.left=rect.right+40;
	rect.right=rect.left+60;
	btn=new JKButton(rect,ID_BUTTON+1);
	btn->SetText("Âá­¡");
	AddControl(btn);
}

void EquipPrint::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.IsOverWrite=FALSE;
	rect.top+=25;
	rect.bottom=rect.top+40;
	dc.TextOutX(rect,"»¡‹q ¬åÈ‚–E Èe´¢·i\nÏaŸ¥Ëa¡ Â‰bÐs“¡”a.",ADJ_XYCENTER);
}

void EquipPrint::EvButtonClick(WORD ctrlid)
{
  int16 i=0,LoopCount=0;
  int32 BombCount=0;
  EqiupFileManage *fm;
  BYTE status = biosprint(2,0,0);

	switch(ctrlid) {
		case ID_BUTTON:
			if((status&0x10)&&!(status&0x29)) {
        if(SomoFlag) fm=new EqiupFileManage(SOMONAME);
					else fm=new EqiupFileManage(EQUIPNAME);

				PrintState();
				if(SomoFlag)PrintHeadLine2();
				else PrintHeadLine();
				while(fm&&fm->Read(Bomb,i)) {
					if(FindBomb(Bomb)||SomoFlag) {
						if(SomoFlag) {
							if(!PrintListString2(Bomb)) break;
							BombCount+=atol(Bomb.Count);
							LoopCount++;
						}
						else {
							if(!PrintListString(Bomb)) break;
							BombCount+=atol(Bomb.Count);
							LoopCount++;
						}
						if((LoopCount+48)%48==0&&LoopCount) {
							if(!PrintTailLine()) break;
							fputc(FF,LPT1);
							if(SomoFlag)PrintHeadLine2();
							else PrintHeadLine();
						}
					}
					i++;
				}
				if(fm) delete fm;
				if(!PrintTailLine()) break;
				if((status&0x10)&&!(status&0x29)) {
					fprintf(LPT1,"Á· : %ld ¤i",BombCount);
				}
				fputc(FF,LPT1);
			}
			CloseWindow();
			break;
		case ID_BUTTON+1:
			CloseWindow();
			break;
	}
}

BOOL EquipPrint::PrintHeadLine()
{
	BYTE status = biosprint(2,0,0);
	if((status&0x10)&&!(status&0x29)) {
		fprintf(LPT1,"===============================================================================================\r\n");
		fprintf(LPT1,"  ­¡­¢     Èe¹·     D.O.D      LOT-NO               ¹A¸b‘e•¡   ¤i® ¶w•¡ ®w·©    ¥¡‰Å¸w­¡		 \r\n");
		fprintf(LPT1,"===============================================================================================\r\n");
		return TRUE;
	}
	return FALSE;
}

BOOL EquipPrint::PrintHeadLine2()
{
	BYTE status = biosprint(2,0,0);
	if((status&0x10)&&!(status&0x29)) {
		fprintf(LPT1,"===============================================================================================\r\n");
		fprintf(LPT1,"  ­¡­¢     Èe¹·     D.O.D      LOT-NO               ¹A¸b‘e•¡   ¤i® ¶w•¡ ®w·©     ­¡¡¡·©     \r\n");
		fprintf(LPT1,"===============================================================================================\r\n");
		return TRUE;
	}
	return FALSE;
}

BOOL EquipPrint::PrintTailLine()
{
	BYTE status = biosprint(2,0,0);
	if((status&0x10)&&!(status&0x29)) {
		fprintf(LPT1,"===============================================================================================\r\n");
		return TRUE;
	}
	return FALSE;
}

BOOL EquipPrint::PrintListString(BOMB& bm)
{
	char Place[11];
	strncpy(Place,bm.Place,10);
	Place[11]=NULL;
	BYTE status = biosprint(2,0,0);
	if((status&0x10)&&!(status&0x29)) {
		fprintf(LPT1,"  %-8s %-8s %-10s %-20s %-10s %-4s %-4s %-10s %-10s\r\n",bm.Name,bm.Kind,bm.Dod,bm.Lot,
			bm.Born,bm.Count,bm.Use,bm.HaveYear,bm.Place);
		return TRUE;
	}
	return FALSE;
}

BOOL EquipPrint::PrintListString2(BOMB& bm)
{
	BYTE status = biosprint(2,0,0);
	if((status&0x10)&&!(status&0x29)) {
		fprintf(LPT1,"  %-8s %-8s %-10s %-20s %-10s %-4s %-4s %-10s %-10s \r\n",bm.Name,bm.Kind,bm.Dod,bm.Lot,
			bm.Born,bm.Count,bm.Use,bm.HaveYear,bm.SomoYear);
		return TRUE;
	}
	return FALSE;
}

BOOL EquipPrint::PrintState()
{
	BOMB TempBomb=CompareBomb;
	if(*CompareBomb.Kind==NULL||SomoFlag) strcpy(TempBomb.Kind,"¸åÁA");
	if(*CompareBomb.Dod==NULL||SomoFlag) strcpy(TempBomb.Dod,"¸åÁA");
	if(*CompareBomb.Lot==NULL||SomoFlag) strcpy(TempBomb.Lot,"¸åÁA");
	if(*CompareBomb.Born==NULL||SomoFlag) strcpy(TempBomb.Born,"¸åÁA");
	if(*CompareBomb.Place==NULL||SomoFlag) strcpy(TempBomb.Place,"¸åÁA");
	if(*CompareBomb.Use==NULL||SomoFlag) strcpy(TempBomb.Use,"¸åÁA");

	BYTE status = biosprint(2,0,0);
	if((status&0x10)&&!(status&0x29)) {
		if(SomoFlag) fprintf(LPT1,"                                <<  ­¡¡¡Èe ÑeÑ× Â‰b  >>                                   \n\n");
		else fprintf(LPT1,"                                <<  ¥¡·AÈe ÑeÑ× Â‰b  >>                                   \n\n");

		fprintf(LPT1,"Èe¹·:%s, DOD:%s, LOT_NO:%s, ¹A¸b‘e•¡:%s, ¥¡‰Å¸w­¡:%s, ¶w•¡:%s\r\n",TempBomb.Kind,TempBomb.Dod,
			TempBomb.Lot,TempBomb.Born,TempBomb.Place,TempBomb.Use);
		return TRUE;
	}
	return FALSE;
}

BOOL EquipPrint::FindBomb(BOMB& bm)
{
	if(strcmp(CompareBomb.Name,bm.Name)==0||*CompareBomb.Name==0)
	if(strcmp(CompareBomb.Kind,bm.Kind)==0||*CompareBomb.Kind==0)
	if(strcmp(CompareBomb.Dod,bm.Dod)==0||*CompareBomb.Dod==0)
	if(strcmp(CompareBomb.Lot,bm.Lot)==0||*CompareBomb.Lot==0)
	if(strstr(bm.Born,CompareBomb.Born)!=0||*CompareBomb.Born==0)
	if(strcmp(CompareBomb.Place,bm.Place)==0||*CompareBomb.Place==0)
	if(strcmp(CompareBomb.Use,bm.Use)==0||*CompareBomb.Use==0) return TRUE;
	return FALSE;
}

