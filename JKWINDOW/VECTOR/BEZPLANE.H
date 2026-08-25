#ifndef _BEZPLANE_H
#define _BEZPLANE_H

#include "jkpoint.h"

extern double B2K[16][16];
extern double K2B[16][16];

class BezierPlane
{
public:
	void NormalizeControlPoint(JKRect rect,JKPoint points[4][4]);
	JKPoint CalcBezPoint(double u,double v,JKPoint points[4][4],BOOL iskbez);
	void ChangeBezMode(BOOL mode,JKPoint points[4][4],JKPoint PointBuf[4][4]);
	void GetCurvePoint(BOOL mode,BOOL isu,double uv,JKPoint points[4][4],JKPoint PointBuf[4]);
};

#endif