#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "real2pix.h"

RealCoordManager::RealCoordManager(JKPoint32 orgpos,JKPoint32 clientsize,double xzoom,double yzoom):
	RealOrgPos(orgpos),ClientSize(clientsize),XZoom(xzoom),YZoom(yzoom)
{}
RealCoordManager::~RealCoordManager()
{}

BOOL RealCoordManager::RealToPixel_SX(int32& size)
{
	if(XZoom>0.) {
		size=int32(size*XZoom);
		return TRUE;
	}
	return FALSE;
}

BOOL RealCoordManager::PixelToReal_SX(int32& size)
{
	if(XZoom>0.) {
		size=int32(size/XZoom);
		return TRUE;
	}
	return FALSE;
}

BOOL RealCoordManager::RealToPixel_SY(int32& size)
{
	if(YZoom>0.) {
		size=int32(size*YZoom);
		return TRUE;
	}
	return FALSE;
}

BOOL RealCoordManager::PixelToReal_SY(int32& size)
{
	if(YZoom>0.) {
		size=int32(size/YZoom);
		return TRUE;
	}
	return FALSE;
}

BOOL RealCoordManager::RealToPixel_X(int32& pos,BOOL ischeck)
{
	int32 temp=pos-RealOrgPos.x;
	if(RealToPixel_SX(temp)){
		if(!ischeck||IsValidPixel_X(temp)){
			pos=temp;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RealCoordManager::PixelToReal_X(int32& pos,BOOL ischeck)
{
	int32 temp=pos;
	if(PixelToReal_SX(temp)){
		temp+=RealOrgPos.x;
		if(!ischeck||IsValidReal_X(temp)){
			pos=temp;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RealCoordManager::RealToPixel_Y(int32& pos,BOOL ischeck)
{
	if(YZoom>0.){
		int32 temp=pos-RealOrgPos.y;
		temp=ClientSize.y-int32(temp*YZoom);
		if(!ischeck||IsValidPixel_Y(temp)){
			pos=temp;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RealCoordManager::PixelToReal_Y(int32& pos,BOOL ischeck)
{
	if(YZoom>0.){
		int32 temp=int32((ClientSize.y-pos)/YZoom);
		temp+=RealOrgPos.y;
		if(!ischeck||IsValidReal_Y(temp)){
			pos=temp;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RealCoordManager::RealToPixel(JKPoint32& pos,BOOL ischeck)
{
	JKPoint32 temp(pos);
	if(RealToPixel_X(temp.x,ischeck)&&RealToPixel_Y(temp.y,ischeck)){
		pos=temp;
		return TRUE;
	}
	return FALSE;
}

BOOL RealCoordManager::PixelToReal(JKPoint32& pos,BOOL ischeck)
{
	JKPoint32 temp(pos);
	if(PixelToReal_X(temp.x,ischeck)&&PixelToReal_Y(temp.y,ischeck)){
		pos=temp;
		return TRUE;
	}
	return FALSE;
}

BOOL RealCoordManager::RealToPixel(JKRect32& pos,BOOL ischeck)
{
	JKRect32 temp(pos);
	if(RealToPixel_X(temp.left,ischeck)&&RealToPixel_Y(temp.top,ischeck) &&
			RealToPixel_X(temp.right,ischeck)&&RealToPixel_Y(temp.bottom,ischeck)){
		pos.Set(temp.left,temp.bottom,temp.right,temp.top);
		return TRUE;
	}
	return FALSE;
}

BOOL RealCoordManager::PixelToReal(JKRect32& pos,BOOL ischeck)
{
	JKRect32 temp(pos);
	if(PixelToReal_X(temp.left,ischeck)&&PixelToReal_Y(temp.top,ischeck) &&
			PixelToReal_X(temp.right,ischeck)&&PixelToReal_Y(temp.bottom,ischeck)){
		pos.Set(temp.left,temp.bottom,temp.right,temp.top);
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////

BOOL RealCoordManager::IsValidPixel_X(int32 coord)
{
		if(coord>=0&&coord<ClientSize.x)    return TRUE;
	return FALSE;
}
BOOL RealCoordManager::IsValidPixel_Y(int32 coord)
{
	if(coord>=0&&coord<ClientSize.y) return TRUE;
	return FALSE;
}

BOOL RealCoordManager::IsValidPixel(JKPoint32& coord)
{
	if(coord.x>=0&&coord.x<ClientSize.x && coord.y>=0&&coord.y<ClientSize.y) return TRUE;
	return FALSE;
}

//////////////////////////////////////////////////////////////////////

BOOL RealCoordManager::IsValidReal_X(int32 coord)
{
	if(coord>=0&&coord<5*10000)	return TRUE;
	return FALSE;
}
BOOL RealCoordManager::IsValidReal_Y(int32 coord)
{
	if(coord>=0&&coord<10*10000) return TRUE;
	return FALSE;
}

BOOL RealCoordManager::IsValidReal(JKPoint32& coord)
{
	if(coord.x>=0&&coord.x<5*10000 && coord.y>=0&&coord.y<10*10000)
		return TRUE;
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
BOOL RealCoordManager::GetClientRealRect(JKRect32& rect)
{
	JKPoint32 pos(ClientSize.x-1,1);
	if(PixelToReal(pos)){
		JKRect32 temp(RealOrgPos,pos);
		if(temp.IsValid()) {
			rect=temp;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RealCoordManager::SetClientRealRect(JKRect32 rect)
{
	if(!rect.IsValid()) return FALSE;
	if(IsValidReal(rect.LeftTop())&&IsValidReal(rect.RightBottom())){
		JKPoint32 realsize(rect.Width()+1,rect.Height()+1);
		if(realsize.x>=100&&realsize.y>=100){
			XZoom= (double)ClientSize.x/realsize.x;
			YZoom= (double)ClientSize.y/realsize.y;
			RealOrgPos=rect.LeftTop();
			return TRUE;
		}
	}
	return FALSE;
}
//////////////////////////////////////////////////////////////////////
static char CHARSET_X[5+1]="BCDEF";
static char CHARSET_Y[10+1]="PQRSTUVABC";

BOOL RealCoordManager::RealToStr(char buffer[11],JKPoint32 pos)
{
	if(!buffer) return FALSE;
	if(!IsValidReal(pos)) return FALSE;
	int32 Prepix_X=pos.x/10000,Prepix_Y=pos.y/10000;
	int32 Num_X=pos.x%10000,Num_Y=pos.y%10000;
	if(Prepix_X>=0&&Prepix_X<5 && Prepix_Y>=0&&Prepix_Y<10){
		if(Num_X>=0&&Num_Y>=0){
			sprintf(buffer,"%c%c%04d%04d",CHARSET_X[Prepix_X],CHARSET_Y[Prepix_Y],Num_X,Num_Y);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RealCoordManager::StrToReal(char buffer[11],JKPoint32& pos)
{
	if(!buffer) return FALSE;
	if(strlen(buffer)!=10) return FALSE;
	int32 Prepix_X = toupper(buffer[0]),Prepix_Y = toupper(buffer[1]);
	if(Prepix_X>='B'&&Prepix_X<='F'){
		Prepix_X= Prepix_X-'B';

		if(Prepix_Y>='P'&&Prepix_Y<='V') Prepix_Y-='P';
		else if(Prepix_Y>='A'&&Prepix_Y<='C')	Prepix_Y=Prepix_Y-'A'+7;
		else return FALSE;

		char *temp=buffer+2;
		while(*temp) {
			if(!isdigit(*temp)&&*temp!=' ') return FALSE;
			temp++;
		}
		char Str_X[5],Str_Y[5];
		strncpy(Str_X,buffer+2,4); Str_X[4]=0;
		strncpy(Str_Y,buffer+6,4); Str_Y[4]=0;
		int32 Num_X=atoi(Str_X),Num_Y=atoi(Str_Y);
		pos.Set(Prepix_X*10000+Num_X,Prepix_Y*10000+Num_Y);
		return TRUE;
	}
	return FALSE;
}

