#include "jkstatic.h"
#include "occmsg.h"
#include "showcur.h"

ShowCurInfom::ShowCurInfom(JKRect& rect,RealCoordManager& coordman,JKPoint32& blocksize,JKControl* parent):
	JKDialog(rect,parent),RealCoordMan(coordman),BlockSize(blocksize)
{
	WinAttr|=WA_TITLEMOVEABLE;
  SetText("¬wÑ×•¡  ¶áÃ¡ ¸÷¥¡");
	BackColor=White;
}
ShowCurInfom::~ShowCurInfom()
{}

JKRect ShowCurInfom::CalcClientRect()
{
	JKRect rect=JKDialog::CalcClientRect();
	rect.left+=10; rect.top+=10;
	rect.right=rect.left+30*5+1;
	rect.bottom=rect.top+30*10+1;
	return rect;
}

void ShowCurInfom::SetupWindow()
{
	char tempbuff[25];
	JKRect rect=ClientRect.SetLeftTopBy();
	rect.left=rect.right+15;
	rect.right=rect.left+150;

	rect.bottom=rect.top+20;
	JKStatic *stc;
	stc=new JKStatic(rect,0);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=LtGray;
	stc->AdjustFlag=ADJ_YCENTER;
	stc->SetText("¹ÁÃbÐa”e ¹ÁÎa :");
	AddControl(stc);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+20;
	stc=new JKStatic(rect,0);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=White;
	stc->AdjustFlag=ADJ_YCENTER;
	if(RealCoordMan.RealToStr(tempbuff,RealCoordMan.RealOrgPos))
		stc->SetText(tempbuff);
	AddControl(stc);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+20;
	stc=new JKStatic(rect,0);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=LtGray;
	stc->AdjustFlag=ADJ_YCENTER;
	stc->SetText("ÑÂ”§¡·I(X,Y) :");
	AddControl(stc);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+20;
	stc=new JKStatic(rect,0);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=White;
	stc->AdjustFlag=ADJ_YCENTER;
	sprintf(tempbuff,"(%6.2f,%6.2f)",RealCoordMan.XZoom,RealCoordMan.YZoom);
	stc->SetText(tempbuff);
	AddControl(stc);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+20;
	stc=new JKStatic(rect,0);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=LtGray;
	stc->AdjustFlag=ADJ_YCENTER;
	stc->SetText("‰b¸aˆe‰b(X,Y) :");
	AddControl(stc);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+20;
	stc=new JKStatic(rect,0);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=White;
	stc->AdjustFlag=ADJ_YCENTER;
	sprintf(tempbuff,"<%6ld,%6ld>",BlockSize.x,BlockSize.y);
	stc->SetText(tempbuff);
	AddControl(stc);
}

////////////////////////////////////////////////

void ShowCurInfom::PaintWindow(JKDC& dc,JKRect& paintrect)
{
	JKRect rect=WinRect.SetLeftTopBy();
	dc.SolidBar(rect,LtGray);
	dc.Rectangle3D(ClientRect.ExpandBy(2),-2);
	JKDialog::PaintWindow(dc,paintrect);
}

void ShowCurInfom::PaintClient(JKDC& dc,JKRect&)
{
	JKRect32 realrect;
	if(RealCoordMan.GetClientRealRect(realrect)){
		JKRect temp(realrect.left*30/10000,300-realrect.bottom*30/10000,
					 realrect.right*30/10000,300-realrect.top*30/10000);
		dc.SolidBar(temp,LtMagenta);
	}

//////////////////////////////////////////////////////////////////////

	JKRect rect=ClientRect.SetLeftTopBy();
	int16 width=rect.Width()+1,height=rect.Height()+1;

	for(uint16 i=0;i<=10;i++)
		dc.HLine(JKPoint(0,i*30),width,LtBlue);
	for(uint16 j=0;j<=5;j++)
		dc.VLine(JKPoint(j*30,0),height,LtBlue);

////////////////////////////////////////////////////

	static char CHARSET_X[5+1]="BCDEF";
	static char CHARSET_Y[10+1]="PQRSTUVABC";

  dc.IsOverWrite=FALSE;
	char Prfx[3]="  ";
	for(i=0;i<5;i++){
		Prfx[0]=CHARSET_X[i];
		for(uint16 j=0;j<10;j++){
			Prfx[1]=CHARSET_Y[9-j];
			dc.TextOutX(JKRect(i*30,j*30,i*30+29,j*30+29),Prfx,ADJ_XYCENTER);
		}
	}
}

void ShowCurInfom::EvLeftClick(JKPoint& p)
{
	int32 GridX=p.x/30,GridY=9-p.y/30;
	if(GridX>=0&&GridX<5 && GridY>=0&&GridY<10){
		JKRect32 rect(GridX*10000,GridY*10000,(GridX+1)*10000-1,(GridY+1)*10000-1);
		if(RealCoordMan.SetClientRealRect(rect)){
			SendControlCommand(CM_CHANGEBASE);
      RedrawClient();
		}
	}
}

