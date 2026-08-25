#include "metadc.h"

JKMetaDC::JKMetaDC(char *filename):
	JKDC(),DCFp(0),XSize(0),YSize(0),InstructCount(0)
{
	strncpy(MetaFileName,filename,MAXPATH);
	MetaFileName[MAXPATH]=0;
}

JKMetaDC::~JKMetaDC()
{}

BOOL JKMetaDC::BeginPaint()
{
	if(!DCFp){
		DCFp=fopen(MetaFileName,"wb");
		if(DCFp){
			fwrite("JKMetaFile",11,1,DCFp);
			fwrite(&XSize,sizeof(XSize),1,DCFp);
			fwrite(&YSize,sizeof(YSize),1,DCFp);
			fwrite(&InstructCount,sizeof(InstructCount),1,DCFp);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL JKMetaDC::EndPaint()
{
	if(DCFp) {
		fclose(DCFp);
		DCFp=0;
		return TRUE;
	}
	return FALSE;
}

void JKMetaDC::SetWriteMode(BYTE wmode)
{
	if(!DCFp) return;
	int16 instructid=META_SETWRITEMODE;
	fwrite(&instructid,sizeof(instructid),1,DCFp);
	fwrite(&wmode,sizeof(wmode),1,DCFp);
}

void JKMetaDC::SetTextColor(int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_SETTEXTCOLOR;
	fwrite(&instructid,sizeof(instructid),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::SetBackColor(int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_SETBACKCOLOR;
	fwrite(&instructid,sizeof(instructid),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);;
}

void JKMetaDC::SetPen(JKPen pen)
{
	if(!DCFp) return;
	int16 instructid=META_SETPEN;
	fwrite(&instructid,sizeof(instructid),1,DCFp);
	fwrite(&pen,sizeof(pen),1,DCFp);
}

void JKMetaDC::SetBrush(JKBrush brush)
{
	if(!DCFp) return;
	int16 instructid=META_SETBRUSH;
	fwrite(&instructid,sizeof(instructid),1,DCFp);
	fwrite(&brush,sizeof(brush),1,DCFp);
}

void JKMetaDC::PutPixel(JKPoint p,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_PUTPIXEL;
	fwrite(&instructid,sizeof(instructid),1,DCFp);
	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

BOOL JKMetaDC::GetImage(JKRect , BYTE *)
{
	return FALSE;
}

BOOL JKMetaDC::PutImage(JKPoint pos, BYTE *buffer,BYTE op,JKRect cliprect)
{
	if(!buffer) return FALSE;
	if(!DCFp) return FALSE;
	int16 instructid=META_PUTIMAGE;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&pos,sizeof(pos),1,DCFp);
	uint16 width=*((uint16 *)buffer)+1;
	uint16 height=*((uint16 *)(buffer+2))+1;
	fwrite(buffer,width*height+4,1,DCFp);
	fwrite(&op,sizeof(op),1,DCFp);
	fwrite(&cliprect,sizeof(cliprect),1,DCFp);
	return TRUE;
}

BOOL JKMetaDC::PutPCX(int8 *fname,JKPoint pos,JKRect rect,BYTE putmode,BOOL palchange)
{
	if(!fname) return FALSE;

	if(!DCFp) return FALSE;
	int16 instructid=META_PUTPCX;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(fname,MAXPATH,1,DCFp);
	fwrite(&pos,sizeof(pos),1,DCFp);
	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&putmode,sizeof(putmode),1,DCFp);
	fwrite(&palchange,sizeof(palchange),1,DCFp);
	return TRUE;
}

BOOL JKMetaDC::PutIMG(int8 *fname,JKPoint p,JKRect rect,BYTE putmode)
{
	if(!fname||!fname[0]) return FALSE;
	if(!DCFp) return FALSE;
	int16 instructid=META_PUTIMG;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(fname,MAXPATH,1,DCFp);
	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&putmode,sizeof(putmode),1,DCFp);
	return TRUE;
}

void JKMetaDC::Circle(JKPoint p,int16 radius,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_CIRCLE;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&radius,sizeof(radius),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::Arc(JKPoint p, double startangle, double endangle, int16 radius,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_ARC;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&startangle,sizeof(startangle),1,DCFp);
	fwrite(&endangle,sizeof(endangle),1,DCFp);
	fwrite(&radius,sizeof(radius),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::Pieslice(JKPoint p, double startangle, double endangle, int16 radius,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_PIESLICE;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&startangle,sizeof(startangle),1,DCFp);
	fwrite(&endangle,sizeof(endangle),1,DCFp);
	fwrite(&radius,sizeof(radius),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::DrawPolygon(JKPoint *polypoint,WORD numpoints,int16 color)
{
	if(!polypoint||!numpoints) return;
	if(!DCFp) return;
	int16 instructid=META_DRAWPOLYGON;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&numpoints,sizeof(numpoints),1,DCFp);
	for(uint16 i=0;i<numpoints;i++)
		fwrite(&polypoint[i],sizeof(polypoint[i]),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::FillPolygon(JKPoint *polypoint,WORD numpoints,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_FILLPOLYGON;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&numpoints,sizeof(numpoints),1,DCFp);
	for(uint16 i=0;i<numpoints;i++)
		fwrite(&polypoint[i],sizeof(polypoint[i]),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::Line(int16 x1,int16 y1,int16 x2,int16 y2,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_LINE;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&x1,sizeof(x1),1,DCFp);
	fwrite(&y1,sizeof(y1),1,DCFp);
	fwrite(&x2,sizeof(x2),1,DCFp);
	fwrite(&y2,sizeof(y2),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::MoveTo(int16 x,int16 y)
{
	if(!DCFp) return;
	int16 instructid=META_MOVETO;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&x,sizeof(x),1,DCFp);
	fwrite(&y,sizeof(y),1,DCFp);
}

void JKMetaDC::LineTo(int16 x,int16 y,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_LINETO;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&x,sizeof(x),1,DCFp);
	fwrite(&y,sizeof(y),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::HLine(JKPoint p,int16 delta,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_HLINE;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&delta,sizeof(delta),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::HLineTo(int16 delta,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_HLINETO;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&delta,sizeof(delta),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::VLine(JKPoint p,int16 delta,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_VLINE;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&delta,sizeof(delta),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::VLineTo(int16 delta,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_VLINETO;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&delta,sizeof(delta),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::Bezier(JKPoint ps[4],int16 color,BOOL iskbez,double delta)
{
	if(!DCFp) return;
	int16 instructid=META_BEZIER;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&ps[0],sizeof(ps[0]),1,DCFp);
	fwrite(&ps[1],sizeof(ps[1]),1,DCFp);
	fwrite(&ps[2],sizeof(ps[2]),1,DCFp);
	fwrite(&ps[3],sizeof(ps[3]),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
	fwrite(&iskbez,sizeof(iskbez),1,DCFp);
	fwrite(&delta,sizeof(delta),1,DCFp);
}

void JKMetaDC::BezierTo(JKPoint ps[3],int16 color,BOOL iskbez,double delta)
{
	if(!DCFp) return;
	int16 instructid=META_BEZIERTO;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&ps[0],sizeof(ps[0]),1,DCFp);
	fwrite(&ps[1],sizeof(ps[1]),1,DCFp);
	fwrite(&ps[2],sizeof(ps[2]),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
	fwrite(&iskbez,sizeof(iskbez),1,DCFp);
	fwrite(&delta,sizeof(delta),1,DCFp);
}

void JKMetaDC::SolidBar(JKRect rect,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_SOLIDBAR;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::Rectangle(JKRect rect,int16 color)
{
	if(!DCFp) return;
	int16 instructid=META_RECTANGLE;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
}

void JKMetaDC::Rectangle3D(JKRect& rect,int16 depth,int16 color1,int16 color2)
{
	if(!DCFp) return;
	int16 instructid=META_RECTANGLE3D;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&depth,sizeof(depth),1,DCFp);
	fwrite(&color1,sizeof(color1),1,DCFp);
	fwrite(&color2,sizeof(color2),1,DCFp);
}

void JKMetaDC::Box3D(JKRect& rect,int16 depth,int16 color,int16 color1,int16 color2)
{
	if(!DCFp) return;
	int16 instructid=META_BOX3D;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&depth,sizeof(depth),1,DCFp);
	fwrite(&color,sizeof(color),1,DCFp);
	fwrite(&color1,sizeof(color1),1,DCFp);
	fwrite(&color2,sizeof(color2),1,DCFp);
}

void JKMetaDC::ScrollDown(JKRect rect,WORD lines)
{
	if(!DCFp) return;
	int16 instructid=META_SCROLLDOWN;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&lines,sizeof(lines),1,DCFp);
}

void JKMetaDC::ScrollUp(JKRect rect,WORD lines)
{
	if(!DCFp) return;
	int16 instructid=META_SCROLLUP;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&lines,sizeof(lines),1,DCFp);
}

void JKMetaDC::ScrollLeft(JKRect rect,WORD lines)
{
	if(!DCFp) return;
	int16 instructid=META_SCROLLLEFT;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&lines,sizeof(lines),1,DCFp);
}

void JKMetaDC::ScrollRight(JKRect rect,WORD lines)
{
	if(!DCFp) return;
	int16 instructid=META_SCROLLRIGHT;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&lines,sizeof(lines),1,DCFp);
}

void JKMetaDC::EngPutCh(JKPoint p,BYTE ch)
{
	if(!DCFp) return;
	int16 instructid=META_ENGPUTCH;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&ch,sizeof(ch),1,DCFp);
}

void JKMetaDC::HanPutCh(JKPoint p,BYTE firstbyte,BYTE secondbyte)
{
	if(!DCFp) return;
	int16 instructid=META_HANPUTCH;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&firstbyte,sizeof(firstbyte),1,DCFp);
	fwrite(&secondbyte,sizeof(secondbyte),1,DCFp);
}

void JKMetaDC::PutCh(JKPoint p,WORD code)
{
	if(!DCFp) return;
	int16 instructid=META_PUTCH;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&code,sizeof(code),1,DCFp);
}

void JKMetaDC::TextOut(JKPoint p,WORD n,int8 *str)
{
	if(!str||!str[0]) return;
	if(n>strlen(str)) n=strlen(str);

	if(!DCFp) return;
	int16 instructid=META_TEXTOUT;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&p,sizeof(p),1,DCFp);
	fwrite(&n,sizeof(n),1,DCFp);
	fwrite(str,n,1,DCFp);
}

void JKMetaDC::TextOutX(JKRect rect,int8 *str,BYTE adjflag,BOOL IsWrapping)
{
	if(!str||!str[0]) return;
	int16 n=strlen(str);

	if(!DCFp) return;
	int16 instructid=META_TEXTOUTX;
	fwrite(&instructid,sizeof(instructid),1,DCFp);

	fwrite(&rect,sizeof(rect),1,DCFp);
	fwrite(&n,sizeof(n),1,DCFp);
	fwrite(str,n,1,DCFp);
	fwrite(&adjflag,sizeof(adjflag),1,DCFp);
	fwrite(&IsWrapping,sizeof(IsWrapping),1,DCFp);
}

