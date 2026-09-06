#include "biorythm.h"

BioRythm::BioRythm(char *name,int16 year,int16 month,int16 day,int16 xx,int16 yy,JKControl *parent):
	JKDialog(JKRect(xx,yy,xx+350,yy+350),parent),BornYear(year),BornMonth(month),BornDay(day)
{
	WinForm=0x07;
	SetText("¤a ·¡ µ¡   Ÿ¡ —q");
  Depth=2;
	getdate(&Date);
	strcpy(Name,name);
  Year=Date.da_year;
  Month=Date.da_mon;
  Day=Date.da_day;
	if(Day<6&&Month==1) Year--,Month=12,Day=31+Day-5;
	else if(Day<6&&Month==3) LeapYear(Year)?Day=29+Day-5:Day=28+Day-5,Month--;
	else if(Day<6) Month--,Day=MONTH[month]-(Day-5);
	else Day-=5;
	GetPassDay();
}

void BioRythm::SetupWindow()
{
  char buf[25];
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

  rect=JKRect(18,rect.bottom-70,rect.right-90,rect.bottom-50);
  JKStatic *stc=new JKStatic(rect,ID_STATIC+1);
  sprintf(buf,"¬÷ ¡w:%s",Name);
  stc->SetText(buf);
  stc->BackColor=LtGray;
  stc->AdjustFlag=ADJ_LEFT;
  AddControl(stc);

  rect.top+=20;
  rect.bottom+=20;
  stc=new JKStatic(rect,ID_STATIC+2);
  sprintf(buf,"È´áei: %4d‘e%2d¶©%2d·©",BornYear,BornMonth,BornDay);
  stc->SetText(buf);
  stc->BackColor=LtGray;
  stc->AdjustFlag=ADJ_LEFT;
  AddControl(stc);

  rect.top+=20;
  rect.bottom+=20;
  stc=new JKStatic(rect,ID_STATIC+3);
  sprintf(buf,"Ñe ¸ ·©: %4d‘e%2d¶©%2d·©",Date.da_year,Date.da_mon,Date.da_day);
  stc->SetText (buf);
  stc->AdjustFlag=ADJ_LEFT;
  stc->BackColor=LtGray;
  AddControl(stc);

  rect=JKRect(18+2,205,325,225);
  rect.right=rect.left+50;
  stc=new JKStatic(rect,ID_STATIC+4);
  stc->SetText ("¯¥  ÁA");
  stc->AdjustFlag=ADJ_XYCENTER;
  stc->BackColor=Black;
  stc->TextColor=LtRed;
  AddControl(stc);

  rect.left+=(325-rect.left)/2-25;
  rect.right=rect.left+50;
  stc=new JKStatic(rect,ID_STATIC+5);
  stc->SetText ("»¡  ¬÷");
  stc->AdjustFlag=ADJ_XYCENTER;
  stc->BackColor=Black;
  stc->TextColor=LtGreen;
  AddControl(stc);

  rect.right=320,rect.left=rect.right-50;
	stc=new JKStatic(rect,ID_STATIC+6);
  stc->SetText ("ˆq  ¬÷");
  stc->AdjustFlag=ADJ_XYCENTER;
  stc->BackColor=Black;
  stc->TextColor=Yellow;
  AddControl(stc);

  char buf2[38];
  int16 ye,mon;
  Month==12?ye=Year+1,mon=1:mon=Month+1,ye=Year;
  rect=JKRect(18+3,14,325-3,34);
  stc=new JKStatic(rect,ID_STATIC+7);
  sprintf(buf2,"%4d‘e%2d¶©%2d·© - %4d‘e%2d¶©%2d·©",Year,Month,Day,ye,mon,Day);
  stc->SetText(buf2);
  stc->AdjustFlag=ADJ_XYCENTER;
  stc->BackColor=Black;
  stc->TextColor=Yellow;
  AddControl(stc);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
  rect=JKRect(rect.right-100,rect.bottom-50,rect.right-15,rect.bottom-15);
  JKButton *btn=new JKButton(rect,ID_BUTTON+1);
  btn->Depth=2;
  btn->SetText("ÑÂ  ·¥");
  btn->TextColor=Red;
  AddControl(btn);

}

void BioRythm::PaintClient(JKDC& dc)
{
  JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
  rect=JKRect(18,40,325,200);
  dc.Box3D(rect,-2,Black);
  rect.top=rect.bottom+2,rect.bottom=rect.top+25;
  dc.Box3D(rect,-2,Black);
  rect=JKRect(18,11,325,36);
  dc.Box3D(rect,-2,Black);

  rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

  dc.VLine(JKPoint(rect.left+70,43),160,LtGray);
  dc.HLine(JKPoint(rect.left+20,120),305,LtGray);
  DrawCurve(dc);
}

void BioRythm::DrawCurve(JKDC& dc)
{
	int16 daymod;
  int16 xx=20,yy=120;
	uint16 width=300,height=60;
	double i,kk,dayd;

  //Physical Curve
  //PHYSICAL    28

  daymod=PassDay%PHYSICAL;
  dayd=daymod*(width/PHYSICAL);

  i=dayd;
  kk=sin(i*M_PI/180.);
	dc.MoveTo(xx+(int16)(width/180.0+0.5),yy+(int16)(-kk*height+0.5));
	for (i;i<360.0+dayd;i+=360.0*5/width) {
   	kk=sin(i*M_PI/180.);
		dc.LineTo(xx+(int16)((i-dayd)*width/360.0+0.5),yy+(int16)(-kk*height+0.5),LtRed);
	}

  //Intellect Curve
  //INTELLECT   32

  width=300*INTELLECT/PHYSICAL;
  daymod=PassDay%INTELLECT;
  dayd=daymod*(width/INTELLECT);

  i=dayd;
  kk=sin(i*M_PI/180.);
	dc.MoveTo(xx+(int16)(width/180.0+0.5),yy+(int16)(-kk*height+0.5));
	for (;i<315.0+dayd;i+=360.0*5/width) {
   	kk=sin(i*M_PI/180.);
		dc.LineTo(xx+(int16)((i-dayd)*width/360.0+0.5),yy+(int16)(-kk*height+0.5),LtGreen);
	}

  //Emotion Curve
  //EMOTION    24

  width=300*EMOTION/PHYSICAL;
  daymod=PassDay%EMOTION;
  dayd=daymod*(width/EMOTION);

  i=dayd;
  kk=sin(i*M_PI/180.);
	dc.MoveTo(xx+(int16)(width/180.0+0.5),yy+(int16)(-kk*height+0.5));
	for (i;i<420.0+dayd;i+=360.0*5/width) {
   	kk=sin(i*M_PI/180.);
		dc.LineTo(xx+(int16)((i-dayd)*width/360.0+0.5),yy+(int16)(-kk*height+0.5),Yellow);
	}
}

void BioRythm::GetPassDay()
{
	int16 i;
	uint16 passday;
	passday=Date.da_day-1;

	for (i=Year;i<Date.da_year;i++) (LeapYear(i))?passday+=366:passday+=365;
	for (i=0;i<12;i++) passday+=MONTH[i];
	for (i=0;i<Month;i++) passday-=MONTH[i];
	passday-=Day;
	passday-=LeapYear(Year);
	PassDay=passday;
}

int16 BioRythm::LeapYear(int16 year)
{
	if(!year%400) return 1;
	else if(!year%100) return 0;
	else if(!year%4) return 1;
	else return 0;
}


void BioRythm::EvCtrlClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1){
		SendMessage(ID_OK);
	}
}

void BioRythm::EvCharDown(uint16 key,WORD)
{
	switch (key) {
		case ENTER:
			SendMessage(ID_OK);
			break;
		case ESC:
			SendMessage(ID_CANCEL);
			break;
	}
}


