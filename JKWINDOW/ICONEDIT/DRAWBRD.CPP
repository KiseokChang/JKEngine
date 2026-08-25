#include "selcolor.h"
#include "drawbrd.h"

DrawBoard::DrawBoard(JKRect& rect,WORD ctlid,uint16& width,uint16& height,BYTE*&imgbuf,BYTE*&maskbuf,JKControl *parent):
	JKControl(rect,ctlid,parent),IconWidth(width),IconHeight(height),ImageBuff(imgbuf),MaskBuff(maskbuf)
{
	if(ImageBuff&&MaskBuff) {
		BackColor=Black;
		CurrentColor = White;
	}
	else CreationError;
}

DrawBoard::~DrawBoard()
{
}

void DrawBoard::PutPoint(JKDC& dc,int16 xpos,int16 ypos,int16 color)
{
	if(!IconWidth || !IconHeight) return;
	JKRect rect((int32)xpos*ClientRect.Width()/IconWidth,(int32)ypos*ClientRect.Height()/IconHeight,
							(int32)(xpos+1)*ClientRect.Width()/IconWidth-1,(int32)(ypos+1)*ClientRect.Height()/IconHeight-1);
	dc.SolidBar(rect,color);
	if(color==0xff){
		int16 depth=-1;
		while(rect.IsValid()){
			dc.Rectangle3D(rect,depth);
			rect.Expand(-3);
			depth=-depth;
		}
	}
}

void DrawBoard::PaintClient(JKDC& dc,JKRect&)
{
	int16 i,j;
	for(i=0;i<IconHeight;i++){
		for(j=0;j<IconWidth;j++)
			PutPoint(dc,j,i,MaskBuff[IconWidth*i+j+4]!=0xff? ImageBuff[IconWidth*i+j+4] : 0xff);
		}
}



void DrawBoard::EvLeftClick(JKPoint& p)
{
	uint16 xpos=(int32)p.x*IconWidth/(ClientRect.Width()+1);
	uint16 ypos=(int32)p.y*IconHeight/(ClientRect.Height()+1);
	if(ImageBuff[xpos+ypos*IconWidth+4]!=CurrentColor||(!CurrentColor&&MaskBuff[xpos+ypos*IconWidth+4]==0xff)){
		JKDC *dc = CreateClientDC();
		if(dc){
			MouseEvents->HideCursor();
			PutPoint(*dc,xpos,ypos,(int)CurrentColor);
			MouseEvents->ShowCursor();
		}
	}
	if(CurrentColor!=0xff){
		ImageBuff[xpos+ypos*IconWidth+4]=CurrentColor;
		MaskBuff[xpos+ypos*IconWidth+4]=0;
	}
	else{
		ImageBuff[xpos+ypos*IconWidth+4]=0;
		MaskBuff[xpos+ypos*IconWidth+4]=0xFF;
	}
}

void DrawBoard::EvLeftDragging(JKPoint& p)
{
	uint16 xpos=(int32)p.x*IconWidth/(ClientRect.Width()+1);
	uint16 ypos=(int32)p.y*IconHeight/(ClientRect.Height()+1);
	if(ImageBuff[xpos+ypos*IconWidth+4]!=CurrentColor||(!CurrentColor&&MaskBuff[xpos+ypos*IconWidth+4]==0xff)){
		JKDC *dc =CreateClientDC();
		if(dc){
			MouseEvents->HideCursor();
			PutPoint(*dc,xpos,ypos,(int)CurrentColor);
			MouseEvents->ShowCursor();
		}
	}
	if(CurrentColor!=0xff){
		ImageBuff[xpos+ypos*IconWidth+4]=CurrentColor;
		MaskBuff[xpos+ypos*IconWidth+4]=0;
	}
	else{
		ImageBuff[xpos+ypos*IconWidth+4]=0;
		MaskBuff[xpos+ypos*IconWidth+4]=0xFF;
	}
}

void DrawBoard::EvRightClick(JKPoint&)
{
	SelectColorDialog colordlg(JKPoint(20,20),CurrentColor,this);
	colordlg.Run();
}


