#ifndef _CALCCORD_H
#define _CALCCORD_H


// Use Radians As Stand Unit.

class CoordCalculator
{
public:
	MilCalculator();
 ~MilCalculator();

	uint16 RadianToMil(double rad);
	double   MilToRadian(uint16 mil);

	uint16 GradeToMil(double grade);
	double   MilToGrade(uint16 mil);

	double   GradeToRadian(double grade);
	double   RadianToGrade(double rad);




};
#endif