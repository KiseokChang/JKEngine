#ifndef _CALCCORD_H
#define _CALCCORD_H

#include "point32.h"

//////////////////////////////////////////////
// Use Coordinate System As Following       //
//                                          //
//               |                          //
//          (-,+)|(+,+)                     //
//               |                          //
//           ----+----                      //
//               |                          //
//          (-,-)|(+,-)                     //
//               |                          //
//                                          //
//////////////////////////////////////////////
// Use Radian System As Stand Unit.         //
// ( Round clockwise )                      //
//                                          //
//               0 (0) <0>                  //
//               |                          //
//               |                          //
//     PI*3/2 ---+---- PI*1/2 (90) <1600>   //
//    (270)      |                          //
//    <4800>     |                          //
//               PI (180) <3200>            //
//                                          //
//////////////////////////////////////////////
//   Unit Range                             //
//                                          //
//   - Radian => 0.<= r < 2.*PI   => double //
//   - Grade  => 0.<= g < 360.    => double //
//   - Mil    => 0 <= g < 6400    => double //
//                                          //
// * ALL Return Value is into Unit Range    //
//                                          //
//////////////////////////////////////////////

typedef struct
{
	double x,y;

}REALPOINT;

class CoordCalculator
{
public:
	CoordCalculator();
 ~CoordCalculator();

// Adjustify into Unit Range
	int16 AdjustMil(int16 mil);
	double   AdjustRadian(double rad);
	double   AdjustGrade(double grade);

// Convert Units
	int16 RadianToMil(double rad);
	double   MilToRadian(int16 mil);

	int16 GradeToMil(double grade);
	double   MilToGrade(int16 mil);

	double   GradeToRadian(double grade);
	double   RadianToGrade(double rad);

// Calculate Distance
	double   CalcDistance(JKPoint p1,JKPoint p2);
	double   CalcDistance(JKPoint32 p1,JKPoint32 p2);
	double   CalcDistance(REALPOINT p1,REALPOINT p2);

//   0
//   |    p2
//   |? /
//   | /
//   p1
//
	double   CalcDirection(JKPoint p1,JKPoint p2);
	double   CalcDirection(JKPoint32 p1,JKPoint32 p2);
	double   CalcDirection(REALPOINT p1,REALPOINT p2);


//        p1
//       /
//      /?
//   p2 ----- p3
//
	double   CalcAngle(JKPoint p1,JKPoint p2,JKPoint p3);
	double   CalcAngle(JKPoint32 p1,JKPoint32 p2,JKPoint32 p3);
	double   CalcAngle(REALPOINT p1,REALPOINT p2,REALPOINT p3);


//     d1 d       =>TRUE 		d1 d2       =>FALSE
//     | /                  | /
//     |/                 	|/
//      ----- d2             ----- d
//
	BOOL     IsInnerDirection(double d1,double d2,double d);

	BOOL IsCross(JKPoint p1,JKPoint p2,JKPoint r1,JKPoint r2);
	BOOL IsCross(JKPoint32 p1,JKPoint32 p2,JKPoint32 r1,JKPoint32 r2);
	BOOL IsCross(REALPOINT p1,REALPOINT p2,REALPOINT r1,REALPOINT r2);

};
#endif