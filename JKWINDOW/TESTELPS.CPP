#include <conio.h>
#include <math.h>
#include <graphics.h>

typedef int  int16;
typedef long int32;


// 원 그리기에의 응용

void draw4P(int16 cx, int16 cy, int16 relx, int16 rely,int16 color)
{
	putpixel(cx+relx, cy-rely,color);
	if(relx){
		putpixel(cx-relx, cy-rely,color);
		if(rely){
			putpixel(cx+relx, cy+rely,color);
			putpixel(cx-relx, cy+rely,color);
		}
	}
	else if(rely) putpixel(cx,cy+rely,color);
}

const double XRATIO=1.;
const double YRATIO=2.;

void drawBran4(int16 cx, int16 cy, int16 rad,int16 color)
{
	int16 x, y;
	double sqrXRatio=XRATIO*XRATIO, sqrYRatio=YRATIO*YRATIO;

	if(rad==0) {putpixel(cx, cy,color); return;}
	//*****************************************
	y=int16(rad*YRATIO/XRATIO+0.5);
	double nDelta=int16(sqrXRatio*(y-0.5)*(y-0.5)-rad*rad*sqrYRatio+0.5);
	int16 boundary=rad*XRATIO/sqrt(sqrXRatio+sqrYRatio);

	putpixel(cx, cy+y,color);
	if(y) putpixel(cx, cy-y,color);
	x=0;
	while(x<boundary){
		nDelta+=2*x*sqrYRatio+sqrYRatio;
		if(nDelta>0)    {
			nDelta-=2*y*sqrXRatio;
			y--;
		}
		x++;
		draw4P(cx, cy, x, y,color);
	}

	//*************************************
	boundary=y-1;
	x=rad;
	y=0;
	nDelta=sqrYRatio/4-sqrYRatio*x;
	putpixel(cx+x, cy,color);

	if(x) putpixel(cx+x, cy,color);
	while(y<boundary){
		nDelta+=2*y*sqrXRatio+sqrXRatio;
		if(nDelta>0)    {
			nDelta-=2*x*sqrYRatio;
			x--;
		}
		y++;
		draw4P(cx, cy, x, y,color);
	}
}


void DrawEllipse(int16 cx,int16 cy, int16 xrad,int16 yrad,int16 color)
{
	int32 Threshold;
	int16 Working[2];
	int32 Squared[2]={(int32)xrad*xrad,(int32)yrad*yrad};
	int32 Adjust[2];

//========================================================

	putpixel(cx,cy+yrad,color);
	putpixel(cx,cy-yrad,color);

	Working[0]=0; Working[1]=yrad;
	Adjust[0]=0; Adjust[1]=Squared[0]*2*yrad;
	Threshold=Squared[0]/4-Squared[0]*yrad;

	while(1){
		Threshold+=Adjust[0]+Squared[1];
		if(Threshold>=0){
			Adjust[1]-=Squared[0]*2;
			Threshold-=Adjust[1];
			Working[1]--;
		}
		Adjust[0]+=Squared[1]*2;
		Working[0]++;
		if(Adjust[0]>=Adjust[1]) break;
		draw4P(cx,cy,Working[0],Working[1],color);
	}

//-------------------------------------------------------

	putpixel(cx+xrad,cy,color);
	putpixel(cx-xrad,cy,color);

	Working[0]=xrad; Working[1]=0;
	Adjust[0]=Squared[1]*2*xrad; Adjust[1]=0;
	Threshold=Squared[1]/4-Squared[1]*xrad;

	while(1){
		Threshold+=Adjust[1]+Squared[0];
		if(Threshold>=0){
			Adjust[0]-=Squared[1]*2;
			Threshold-=Adjust[0];
			Working[0]--;
		}
		Adjust[1]+=Squared[0]*2;
		Working[1]++;
		if(Adjust[1]>Adjust[0]) break;
		draw4P(cx,cy,Working[0],Working[1],color);
	}

}

void main()
{
	int16 gd=DETECT,gm;

	initgraph(&gd,&gm,"d:\\bc\\bgi");

//	drawBran4(100,100,80,WHITE);
//	getch();
	DrawEllipse(100,100,30,100,WHITE);
	getch();

	closegraph();
}