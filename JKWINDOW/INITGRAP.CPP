#include <stdio.h>
#include <conio.h>
#include <graphics.h>

void main()
{
	int gd=VESA256,gm=0;
	initgraph(&gd,&gm,"");
	printf("1234567890123456789012345678901234567890123456789012345678901234567890");
	closegraph();
}