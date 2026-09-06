#include <math.h>
#include "calccord.h"
#include "metabank.h"

void SVLine(JKDC& dc,RealCoordManager& coordman,JKPoint32 p1,JKPoint32 p2,BYTE color,int32 width,JKRect32 paintrect)
{
	JKRect32 restrict(0,0,coordman.ClientSize.x-1,coordman.ClientSize.y-1);
	if(paintrect.IsValid()) restrict=restrict.CalcOverlap(paintrect);
	if(!restrict.IsValid()) return;

	coordman.RealToPixel(p1,FALSE);
	coordman.RealToPixel(p2,FALSE);

	JKRect32 rect(min(p1.x,p2.x),min(p1.y,p2.y),max(p1.x,p2.x),max(p1.y,p2.y));
	rect=restrict.CalcOverlap(rect);
	if(!rect.IsValid()) return;

	int32 wx=width,wy=width;
	coordman.RealToPixel_SX(wx);
	coordman.RealToPixel_SY(wy);

	if(p2.x<p1.x) {
		JKPoint32 temp(p1);
		p1.Set(p2);
		p2.Set(temp);
	}

	int32 dx=abs(p2.x-p1.x);
	int32 dy=abs(p2.y-p1.y);
	int32 z = -(dx+1)/2;
	int32 yfalg= p2.y>p1.y? 1:-1;

	JKPoint32 p(p1);
	if(!wx&&!wy){
		if(restrict.IsContain(p))	dc.PutPixel(JKPoint(p.x,p.y),color);
		while(p.x!=p2.x||p.y!=p2.y){
			if(z<0){
				p.x++;	z+=dy;
			}
			else{
				p.y+=yfalg;	z-=dx;
			}
			if(restrict.IsContain(p))	dc.PutPixel(JKPoint(p.x,p.y),color);
		}
	}
	else{
		int32 wx_2=wx/2,wy_2=wy/2;
		int32 wwx_2=wx-wx_2,wwy_2=wy-wy_2;

		int32 yinc= yfalg>0? wwy_2:-wy_2;
		if(restrict.IsContain(p))
			dc.SolidBar(JKRect(p.x-wx_2,p.y-wy_2,p.x+wwx_2,p.y+wwy_2),color);
		while(p.x!=p2.x||p.y!=p2.y){
			if(z<0){
				p.x++;	z+=dy;
				if(restrict.IsContain(p)) dc.VLine(JKPoint(p.x+wwx_2,p.y-wy_2),wy+1,color);
			}
			else{
				p.y+=yfalg;	z-=dx;
				if(restrict.IsContain(p))	dc.HLine(JKPoint(p.x-wx_2,p.y+yinc),wx+1,color);
			}
		}
	}
}

void SVArc(JKDC& dc,RealCoordManager& coordman,JKPoint32 p,int32 startmil,int32 endmil,int32 radius,BYTE color,int32/*width*/)
{
	CoordCalculator coordcalc;
	double startangle=coordcalc.MilToRadian(startmil);
	double endangle=coordcalc.MilToRadian(endmil);

	JKRect32 restrict(0,0,coordman.ClientSize.x-1,coordman.ClientSize.y-1);
	JKPoint32 realorg(p);
	coordman.RealToPixel(realorg,FALSE);
	JKPoint32 realrad(radius,radius);
	coordman.RealToPixel_SX(realrad.x);
	coordman.RealToPixel_SY(realrad.y);
	JKRect32 rect(realorg-realrad,realorg+realrad);
	rect=restrict.CalcOverlap(rect);
	if(!rect.IsValid()) return;

	dc.Arc(JKPoint(realorg.x,realorg.y),startangle,endangle,JKPoint(realrad.x,realrad.y),color);
/*
	int32 wx=width,wy=width;
	coordman.RealToPixel_SX(wx);
	coordman.RealToPixel_SY(wy);
	int32 wx_2=wx/2,wy_2=wy/2;

	JKPoint32 startp(10000*sin(startangle),-10000*cos(startangle));
	JKPoint32 endp(10000*sin(endangle),-10000*cos(endangle));
	double diffangle= coordcalc.AdjustRadian(endangle-startangle);
	BOOL flag= diffangle>M_PI? 1:0;

	JKPoint32 oldp(realorg),r;
	int32 mx=0,nx=radius;
	int32 t=3-2*radius;
	do{
		for(BYTE i=0;i<8;i++){
			if(i&0x01){	r.x=nx; r.y=mx;	}
			else{	r.x=mx; r.y=nx;	}
			if(i&0x02) r.x=-r.x;
			if(i&0x04) r.y=-r.y;

			JKPoint32 putp=p+r;
			coordman.RealToPixel(putp,FALSE);
				if(oldp!=putp){
				if(startp==endp)
					dc.SolidBar(JKRect(putp.x-wx_2,putp.y-wy_2,putp.x+wx-wx_2,putp.y+wy-wy_2),color);
				else if(!flag){
					if(startp.y*r.x+startp.x*r.y<=0 && endp.y*r.x+endp.x*r.y>=0)
						dc.SolidBar(JKRect(putp.x-wx_2,putp.y-wy_2,putp.x+wx-wx_2,putp.y+wy-wy_2),color);
				}
				else {
					if(startp.y*r.x+startp.x*r.y<=0 || endp.y*r.x+endp.x*r.y>=0)
						dc.SolidBar(JKRect(putp.x-wx_2,putp.y-wy_2,putp.x+wx-wx_2,putp.y+wy-wy_2),color);
				}
			}
		}

		if(t<0) {	t+=4*mx+6;}
		else { t+=4*(mx-nx)+10;	nx--;	}
		mx++;
	} while(mx<=nx);
*/
}

void SVPieslice(JKDC& dc,RealCoordManager& coordman,JKPoint32 p,int32 startmil,int32 endmil,int32 radius,BYTE color,int32 width)
{
	CoordCalculator coordcalc;
	double startangle=coordcalc.MilToRadian(startmil);
	double endangle=coordcalc.MilToRadian(endmil);
	JKPoint32 StartPos(p.x+radius*sin(startangle),p.y+radius*cos(startangle));
	JKPoint32 EndPos(p.x+radius*sin(endangle),p.y+radius*cos(endangle));
	SVLine(dc,coordman,p,StartPos,color,width);
	SVLine(dc,coordman,p,EndPos,color,width);
	SVArc(dc,coordman,p,startmil,endmil,radius,color,width);
}
