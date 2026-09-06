#ifndef _REAL2PIX_H
#define _REAL2PIX_H

#include "point32.h"

class RealCoordManager
{
public:
	RealCoordManager(JKPoint32 orgpos=JKPoint32(),JKPoint32 clientsize=JKPoint32(),double xzoom=1.,double yzoom=1.);
	virtual ~RealCoordManager();

	BOOL RealToPixel_SX(int32& size);
	BOOL PixelToReal_SX(int32& size);
	BOOL RealToPixel_SY(int32& size);
	BOOL PixelToReal_SY(int32& size);
	BOOL RealToPixel_X(int32& pos,BOOL ischeck=TRUE);
	BOOL PixelToReal_X(int32& pos,BOOL ischeck=TRUE);
	BOOL RealToPixel_Y(int32& pos,BOOL ischeck=TRUE);
	BOOL PixelToReal_Y(int32& pos,BOOL ischeck=TRUE);
	BOOL RealToPixel(JKPoint32& pos,BOOL ischeck=TRUE);
	BOOL PixelToReal(JKPoint32& pos,BOOL ischeck=TRUE);
	BOOL RealToPixel(JKRect32& pos,BOOL ischeck=TRUE);
	BOOL PixelToReal(JKRect32& pos,BOOL ischeck=TRUE);
	///////////////////////////////////////////////////////////////////////
	BOOL IsValidPixel_X(int32 coord);
	BOOL IsValidPixel_Y(int32 coord);
	BOOL IsValidPixel(JKPoint32& coord);

	BOOL IsValidReal_X(int32 coord);
	BOOL IsValidReal_Y(int32 coord);
	BOOL IsValidReal(JKPoint32& coord);
	//////////////////////////////////////////////////////////////////////
	BOOL GetClientRealRect(JKRect32& rect);
  BOOL SetClientRealRect(JKRect32 rect);

	BOOL RealToStr(char buffer[11],JKPoint32 pos);
	BOOL StrToReal(char buffer[11],JKPoint32& pos);

	JKPoint32 RealOrgPos;
	JKPoint32 ClientSize;
	double XZoom,YZoom;
};
#endif

