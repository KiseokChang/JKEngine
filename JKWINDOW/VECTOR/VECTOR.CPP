#include <conio.h>
#include <string.h>
#include <graphics.h>

typedef unsigned char       BYTE;
typedef unsigned int        WORD;
typedef unsigned long       DWORD;
typedef BYTE                BOOL;

/*=====================================================*/
#define	TRUE	1
#define	FALSE	0

typedef struct {
	int x;
	int y;
} POINT;


double B2K[16][16]={
	{      1.,      0.,      0.,      0.,      0.,     0.,     0.,      0.,	     0.,     0.,     0.,		  0.,		   0.,	    0.,	     0.,		  0.},
	{  8./27.,   4./9.,   2./9.,  1./27.,	     0.,     0.,     0.,		  0.,	     0.,     0.,     0.,		  0.,		   0.,	    0.,	     0.,		  0.},
	{  1./27.,   2./9.,   4./9.,  8./27., 	   0.,     0.,     0.,		  0.,	     0.,     0.,     0.,		  0.,		   0.,	    0.,	     0.,		  0.},
	{      0.,      0.,      0.,      1.,      0.,     0.,     0.,		  0.,      0.,     0.,     0.,		  0.,		   0.,	    0.,	     0.,		  0.},
	{  8./27.,      0.,	     0.,      0.,   4./9.,     0.,     0.,		  0.,   2./9.,     0.,     0.,		  0.,  1./27.,	    0.,	     0.,		  0.},
	{64./729.,32./243.,16./243., 8./729.,32./243.,16./81., 8./81., 4./243.,16./243., 8./81., 4./81., 2./243., 8./729., 4./243., 2./243., 1./729.},
	{ 8./729.,16./243.,32./243.,64./729., 4./243., 8./81.,16./81.,32./243., 2./243., 4./81., 8./81.,16./243., 1./729., 2./243., 4./243., 8./729.},
	{      0.,      0.,    	 0.,  8./27.,      0.,     0.,     0.,   4./9.,		   0.,     0.,     0.,   2./9.,		   0.,	    0.,	     0.,  1./27.},
	{  1./27.,      0.,      0.,	    0.,   2./9.,     0.,     0.,  	  0.,   4./9.,     0.,     0.,	    0.,  8./27.,	    0.,	     0.,		  0.},
	{ 8./729., 4./243., 2./243., 1./729.,16./243., 8./81., 4./81., 2./243.,32./243.,16./81., 8./81., 4./243.,64./729.,32./243.,16./243., 8./729.},
	{ 1./729., 2./243., 4./243., 8./729., 2./243., 4./81., 8./81.,16./243., 4./243., 8./81.,16./81.,32./243., 8./729.,16./243.,32./243.,64./729.},
	{      0.,      0.,      0.,  1./27.,      0.,     0.,     0.,   2./9.,		   0.,     0.,     0.,   4./9.,		   0.,	    0.,	     0.,  8./27.},
	{      0.,      0.,    	 0.,		  0.,      0.,     0.,     0.,		  0.,		   0.,     0.,     0.,		  0.,		   1.,	    0.,	     0.,		  0.},
	{      0.,      0.,   	 0.,		  0.,      0.,     0.,     0.,		  0.,		   0.,     0.,     0.,		  0.,  8./27.,   4./9.,   2./9.,  1./27.},
	{      0.,      0.,   	 0.,		  0.,      0.,     0.,     0.,		  0.,		   0.,     0.,     0.,		  0.,  1./27.,   2./9.,   4./9.,  8./27.},
	{      0.,      0.,	     0.,		  0.,      0.,     0.,     0.,		  0.,		   0.,     0.,     0.,		  0.,		   0.,	    0.,	     0.,		  1.}
};

double K2B[16][16]={
	{     1.,    0.,    0.,     0.,    0.,    0.,    0.,    0.,		 0.,    0.,    0.,		0.,		  0.,		 0.,	  0.,		  0.},
	{ -5./6.,    3.,-3./2.,  1./3., 	 0.,    0.,    0.,		0.,		 0.,    0.,    0.,		0.,		  0.,		 0.,	  0.,		  0.},
	{	 1./3.,-3./2.,    3., -5./6., 	 0.,    0.,    0.,		0.,		 0.,    0.,    0.,		0.,		  0.,		 0.,	  0.,		  0.},
	{     0.,    0.,    0.,		  1., 	 0.,    0.,    0.,		0.,	   0.,    0.,    0.,		0.,		  0.,		 0.,	  0.,		  0.},
	{ -5./6., 	 0.,    0.,     0.,		 3.,		0.,    0.,    0.,-3./2.,		0.,		 0.,		0.,	 1./3.,		 0.,    0.,		  0.},
	{25./36.,-5./2., 5./4.,-5./18.,-5./2.,    9.,-9./2.,    1., 5./4.,-9./2., 9./4.,-1./2.,-5./18.,    1.,-1./2.,  1./9.},
	{-5./18., 5./4.,-5./2.,25./36.,    1.,-9./2.,    9.,-5./2.,-1./2., 9./4.,-9./2., 5./4.,  1./9.,-1./2.,    1.,-5./18.},
	{ 	  0.,		 0.,		0., -5./6.,		 0.,    0.,		 0., 	  3.,    0.,    0.,		 0.,-3./2.,     0.,    0.,		0.,  1./3.},
	{  1./3.,    0.,    0.,     0.,-3./2.,	  0.,    0.,    0.,		 3.,		0.,		 0.,		0., -5./6.,		 0.,    0.,		  0.},
	{-5./18.,    1.,-1./2.,  1./9., 5./4.,-9./2., 9./4.,-1./2.,-5./2.,    9.,-9./2.,    1.,25./36.,-5./2., 5./4.,-5./18.},
	{  1./9.,-1./2.,    1.,-5./18.,-1./2., 9./4.,-9./2., 5./4.,    1.,-9./2.,    9.,-5./2.,-5./18., 5./4.,-5./2.,25./36.},
	{     0.,		 0.,		0.,  1./3.,    0.,		0.,		 0.,-3./2.,    0.,    0.,		 0.,		3.,     0.,    0.,	  0., -5./6.},
	{     0.,    0.,		0.,		  0.,    0.,    0.,    0.,		0.,		 0.,    0.,    0.,		0.,		  1.,		 0.,		0.,		  0.},
	{     0.,    0.,	  0.,		  0.,    0.,    0.,    0.,		0.,		 0.,    0.,    0.,		0., -5./6.,		 3.,-3./2.,	 1./3.},
	{     0.,    0.,		0.,		  0.,    0.,    0.,    0.,		0.,		 0.,    0.,    0.,		0.,	 1./3.,-3./2.,		3., -5./6.},
	{     0.,    0.,		0.,		  0.,    0.,    0.,    0.,		0.,		 0.,    0.,    0.,		0.,		  0.,		 0.,		0.,	    1.}
};

void MoveTo(POINT p)
{
	moveto(p.x,p.y);
}

void LineTo(POINT p)
{
	lineto(p.x,p.y);
}

POINT Points[4][4]={
	{{200,100},{240,100},{280,100},{320,100}},
	{{200,140},{240,140},{280,140},{320,140}},
	{{200,180},{240,180},{280,180},{320,180}},
	{{200,220},{240,220},{280,220},{320,220}}
};

POINT CalcBezPoint(double u,double v,POINT points[4][4])
{
	double table[2][4];
	table[0][0]=(1-u)*(1-u)*(1-u);
	table[0][1]=3*u*(1-u)*(1-u);
	table[0][2]=3*u*u*(1-u);
	table[0][3]=u*u*u;

	table[1][0]=(1-v)*(1-v)*(1-v);
	table[1][1]=3*v*(1-v)*(1-v);
	table[1][2]=3*v*v*(1-v);
	table[1][3]=v*v*v;

	unsigned i,j;
	double doublex=0,doubley=0;
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			doublex+=table[0][i]*table[1][j]*points[i][j].x;
			doubley+=table[0][i]*table[1][j]*points[i][j].y;
		}
	}
	POINT p;
	p.x=int(doublex+0.5);
	p.y=int(doubley+0.5);
	return p;
}

POINT CalcKBezPoint(double u,double v,POINT points[4][4])
{
	double table[2][4];
	table[0][0]=1./2.*(1.-3.*u)*(2.-3.*u)*(1.-u);
	table[0][1]=9./2.*(2.-3.*u)*(1.-u)*u;
	table[0][2]=-9./2.*(1.-u)*u*(1.-3.*u);
	table[0][3]=1./2.*u*(1.-3*u)*(2.-3.*u);

	table[1][0]=1./2.*(1.-3.*v)*(2.-3.*v)*(1.-v);
	table[1][1]=9./2.*(2.-3.*v)*(1.-v)*v;
	table[1][2]=-9./2.*(1.-v)*v*(1.-3.*v);
	table[1][3]=1./2.*v*(1.-3*v)*(2.-3.*v);

	unsigned i,j;
	double doublex=0,doubley=0;
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			doublex+=table[0][i]*table[1][j]*points[i][j].x;
			doubley+=table[0][i]*table[1][j]*points[i][j].y;
		}
	}
	POINT p;
	p.x=int(doublex+0.5);
	p.y=int(doubley+0.5);
	return p;
}

void ChangeBezMode(BOOL mode,POINT Points[4][4],POINT PointBuf[4][4])
{
	double doublex,doubley;
	for(unsigned i=0;i<16;i++){
		doublex=0.;
		doubley=0.;
		if(mode){
			for(unsigned j=0;j<16;j++){
				doublex+=B2K[i][j]*Points[j/4][j%4].x;
				doubley+=B2K[i][j]*Points[j/4][j%4].y;
			}
		}
		else{
			for(unsigned j=0;j<16;j++){
				doublex+=K2B[i][j]*Points[j/4][j%4].x;
				doubley+=K2B[i][j]*Points[j/4][j%4].y;
			}
		}
		PointBuf[i/4][i%4].x=int(doublex+0.5);
		PointBuf[i/4][i%4].y=int(doubley+0.5);
	}
}

void main()
{
	int gd=VGA,gm=VGAHI;
	initgraph(&gd,&gm,"\\bc\\bgi");

	setwritemode(0);
	double u,v;
	POINT p;
	unsigned i,j;


	setcolor(LIGHTBLUE);
	for(u=0;u<=1.;u+=0.03){
		MoveTo(CalcBezPoint(u,0,Points));
		for(v=0;v<=1.;v+=0.005){
			LineTo(CalcBezPoint(u,v,Points));
		}
	}

	for(v=0;v<=1.;v+=0.03){
		MoveTo(CalcBezPoint(0,v,Points));
		for(u=0;u<=1.;u+=0.005){
			LineTo(CalcBezPoint(u,v,Points));
		}
	}
	setcolor(YELLOW);
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			p=Points[i][j];
			line(p.x-1,p.y-1,p.x+1,p.y+1);
			line(p.x-1,p.y+1,p.x+1,p.y-1);
		}
	}
	getch();
	Points[0][1].y-=120;
	Points[0][2].y-=120;
	Points[3][1].y+=220;
	Points[3][2].y+=220;
	Points[1][0].x-=120;
	Points[2][0].x-=120;
	Points[1][3].x+=120;
	Points[2][3].x+=120;

	cleardevice();
	setcolor(LIGHTBLUE);
	for(u=0;u<=1.;u+=0.03){
		MoveTo(CalcBezPoint(u,0,Points));
		for(v=0;v<=1.;v+=0.005){
			LineTo(CalcBezPoint(u,v,Points));
		}
	}

	for(v=0;v<=1.;v+=0.03){
		MoveTo(CalcBezPoint(0,v,Points));
		for(u=0;u<=1.;u+=0.005){
			LineTo(CalcBezPoint(u,v,Points));
		}
	}
	setcolor(YELLOW);
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			p=Points[i][j];
			line(p.x-1,p.y-1,p.x+1,p.y+1);
			line(p.x-1,p.y+1,p.x+1,p.y-1);
		}
	}
	getch();
	cleardevice();
	setcolor(LIGHTBLUE);
	for(u=0;u<=1.;u+=0.03){
		MoveTo(CalcKBezPoint(u,0,Points));
		for(v=0;v<=1.;v+=0.005){
			LineTo(CalcKBezPoint(u,v,Points));
		}
	}

	for(v=0;v<=1.;v+=0.03){
		MoveTo(CalcKBezPoint(0,v,Points));
		for(u=0;u<=1.;u+=0.005){
			LineTo(CalcKBezPoint(u,v,Points));
		}
	}
	setcolor(YELLOW);
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			p=Points[i][j];
			line(p.x-1,p.y-1,p.x+1,p.y+1);
			line(p.x-1,p.y+1,p.x+1,p.y-1);
		}
	}
	getch();

	POINT Temp[4][4];
	ChangeBezMode(0,Points,Temp);
	memcpy(Points,Temp,4*4*sizeof(POINT));

	cleardevice();
	setcolor(LIGHTRED);
	for(u=0;u<=1.;u+=0.03){
		MoveTo(CalcBezPoint(u,0,Points));
		for(v=0;v<=1.;v+=0.005){
			LineTo(CalcBezPoint(u,v,Points));
		}
	}

	for(v=0;v<=1.;v+=0.03){
		MoveTo(CalcBezPoint(0,v,Points));
		for(u=0;u<=1.;u+=0.005){
			LineTo(CalcBezPoint(u,v,Points));
		}
	}
	setcolor(YELLOW);
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			p=Points[i][j];
			line(p.x-1,p.y-1,p.x+1,p.y+1);
			line(p.x-1,p.y+1,p.x+1,p.y-1);
		}
	}
	getch();

	ChangeBezMode(1,Points,Temp);
	memcpy(Points,Temp,4*4*sizeof(POINT));

	cleardevice();
	setcolor(LIGHTGRAY);
	for(u=0;u<=1.;u+=0.03){
		MoveTo(CalcKBezPoint(u,0,Points));
		for(v=0;v<=1.;v+=0.005){
			LineTo(CalcKBezPoint(u,v,Points));
		}
	}

	for(v=0;v<=1.;v+=0.03){
		MoveTo(CalcKBezPoint(0,v,Points));
		for(u=0;u<=1.;u+=0.005){
			LineTo(CalcKBezPoint(u,v,Points));
		}
	}
	setcolor(YELLOW);
	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			p=Points[i][j];
			line(p.x-1,p.y-1,p.x+1,p.y+1);
			line(p.x-1,p.y+1,p.x+1,p.y-1);
		}
	}
	getch();


	closegraph();
}
