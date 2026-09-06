#include <math.h>
#include "calccord.h"

CoordCalculator::CoordCalculator()
{}

CoordCalculator::~CoordCalculator()
{}


// Adjustify into Unit Range
double CoordCalculator::AdjustRadian(double rad)
{
	rad=fmod(rad,M_PI*2);
	if(rad<0.) rad+=M_PI*2;
	return rad;
}
double CoordCalculator::AdjustGrade(double grade)
{
	grade=fmod(grade,360.);
	if(grade<0.) grade+=360.;
	return grade;
}
int16 CoordCalculator::AdjustMil(int16 mil)
{
	mil%=6400;
	if(mil<0) mil+=6400;
	return mil;
}

// Convert Units
int16 CoordCalculator::RadianToMil(double rad)
{
	rad=AdjustRadian(rad);
	return AdjustMil(int16((rad*6400)/(M_PI*2)+0.5));
}
double CoordCalculator::MilToRadian(int16 mil)
{
	mil=AdjustMil(mil);
	return AdjustRadian(mil*(M_PI*2)/6400.);
}

int16 CoordCalculator::GradeToMil(double grade)
{
	grade=AdjustGrade(grade);
	return AdjustMil(int16((grade*6400)/360.+0.5));
}
double CoordCalculator::MilToGrade(int16 mil)
{
	mil=AdjustMil(mil);
	return AdjustGrade(mil*360./6400.);
}

double CoordCalculator::GradeToRadian(double grade)
{
	grade=AdjustGrade(grade);
	return AdjustRadian(grade*(M_PI*2)/360.);
}
double CoordCalculator::RadianToGrade(double rad)
{
	rad=AdjustRadian(rad);
	return AdjustGrade(rad*360./(M_PI*2));
}

// Calculate Distance
double CoordCalculator::CalcDistance(JKPoint p1,JKPoint p2)
{
	double xdiff= p2.x-p1.x;
	double ydiff= p2.y-p1.y;
	return sqrt(xdiff*xdiff+ydiff*ydiff);
}
double CoordCalculator::CalcDistance(JKPoint32 p1,JKPoint32 p2)
{
	double xdiff= p2.x-p1.x;
	double ydiff= p2.y-p1.y;
	return sqrt(xdiff*xdiff+ydiff*ydiff);
}
double CoordCalculator::CalcDistance(REALPOINT p1,REALPOINT p2)
{
	double xdiff= p2.x-p1.x;
	double ydiff= p2.y-p1.y;
	return sqrt(xdiff*xdiff+ydiff*ydiff);
}


//   0
//   |    p2
//   |? /
//   | /
//   p1
//
double CoordCalculator::CalcDirection(JKPoint p1,JKPoint p2)
{
	int16 xdiff= p2.x-p1.x;
	int16 ydiff= p2.y-p1.y;

	double rad=M_PI/2.-atan2((double)ydiff,xdiff? (double)xdiff:0.00000001);
	return AdjustRadian(rad);
}
double CoordCalculator::CalcDirection(JKPoint32 p1,JKPoint32 p2)
{
	int32 xdiff= p2.x-p1.x;
	int32 ydiff= p2.y-p1.y;

	double rad=M_PI/2.-atan2((double)ydiff,xdiff? (double)xdiff:0.00000001);
	return AdjustRadian(rad);
}
double CoordCalculator::CalcDirection(REALPOINT p1,REALPOINT p2)
{
	double xdiff= p2.x-p1.x;
	double ydiff= p2.y-p1.y;

	double rad=M_PI/2.-atan2(ydiff,xdiff>0.00000001? xdiff:0.00000001);
	return AdjustRadian(rad);
}


//        p1
//       /
//      /?
//   p2 ----- p3
//

double CoordCalculator::CalcAngle(JKPoint p1,JKPoint p2,JKPoint p3)
{
	double d1= CalcDirection(p2,p1);
	double d2= CalcDirection(p2,p3);

	return AdjustRadian(d2-d1);
}
double CoordCalculator::CalcAngle(JKPoint32 p1,JKPoint32 p2,JKPoint32 p3)
{
	double d1= CalcDirection(p2,p1);
	double d2= CalcDirection(p2,p3);

	return AdjustRadian(d2-d1);
}
double CoordCalculator::CalcAngle(REALPOINT p1,REALPOINT p2,REALPOINT p3)
{
	double d1= CalcDirection(p2,p1);
	double d2= CalcDirection(p2,p3);

	return AdjustRadian(d2-d1);
}



//     d1 d       =>TRUE 		d1 d2       =>FALSE
//     | /                  | /
//     |/                 	|/
//      ----- d2             ----- d
//

BOOL CoordCalculator::IsInnerDirection(double d1,double d2,double d)
{
	double ang1=AdjustRadian(d2-d1);
	double ang2=AdjustRadian(d2-d);

	return (ang1>=ang2)? TRUE:FALSE;
}

#define Threshold		(0.0001)

BOOL CoordCalculator::IsCross(JKPoint p0,JKPoint p1,JKPoint r0,JKPoint r1)
{
	double matrix[2][2],con[2];;
	matrix[0][0]=(double)p1.x-p0.x; matrix[0][1]=(double)r0.x-r1.x;
	matrix[1][0]=(double)p1.y-p0.y; matrix[1][1]=(double)r0.y-r1.y;
	con[0]=r0.x-p0.x; con[1]=r0.y-p0.y;

	double D=matrix[0][0]*matrix[1][1]-matrix[0][1]*matrix[1][0];
	if(D>=0.0001 || D <= -0.0001){
		double u=(matrix[1][1]*con[0]-matrix[0][1]*con[1])/D;
		double v=(matrix[0][0]*con[1]-matrix[1][0]*con[0])/D;
		if(u>=0&&u<=1 && v>=0&&v<=1)	return TRUE;
	}
	return FALSE;
}
BOOL CoordCalculator::IsCross(JKPoint32 p0,JKPoint32 p1,JKPoint32 r0,JKPoint32 r1)
{
	double matrix[2][2],con[2];;
	matrix[0][0]=(double)p1.x-p0.x; matrix[0][1]=(double)r0.x-r1.x;
	matrix[1][0]=(double)p1.y-p0.y; matrix[1][1]=(double)r0.y-r1.y;
	con[0]=r0.x-p0.x; con[1]=r0.y-p0.y;

	double D=matrix[0][0]*matrix[1][1]-matrix[0][1]*matrix[1][0];
	if(D>=0.0001 || D <= -0.0001){
		double u=(matrix[1][1]*con[0]-matrix[0][1]*con[1])/D;
		double v=(matrix[0][0]*con[1]-matrix[1][0]*con[0])/D;
		if(u>=0&&u<=1 && v>=0&&v<=1)	return TRUE;
	}
	return FALSE;
}
BOOL CoordCalculator::IsCross(REALPOINT p0,REALPOINT p1,REALPOINT r0,REALPOINT r1)
{
	double matrix[2][2],con[2];;
	matrix[0][0]=p1.x-p0.x; matrix[0][1]=r0.x-r1.x;
	matrix[1][0]=p1.y-p0.y; matrix[1][1]=r0.y-r1.y;
	con[0]=r0.x-p0.x; con[1]=r0.y-p0.y;

	double D=matrix[0][0]*matrix[1][1]-matrix[0][1]*matrix[1][0];
	if(D>=0.0001 || D <= -0.0001){
		double u=(matrix[1][1]*con[0]-matrix[0][1]*con[1])/D;
		double v=(matrix[0][0]*con[1]-matrix[1][0]*con[0])/D;
		if(u>=0&&u<=1 && v>=0&&v<=1)	return TRUE;
	}
	return FALSE;
}
