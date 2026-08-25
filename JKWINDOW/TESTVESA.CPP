#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <graphics.h>
#include "vesa256.h"

INT huge Detect() { return 1; }

void main()
{
	if(!IsVesa()) {
		printf("\nNot Vesa!\n");
		exit(1);
	}

	INT gd,gm;
	INT cmd=21;
	gd = installuserdriver("MEGA256",Detect);
	gm = cmd-10;
	initgraph(&gd,&gm,"");
	if(graphresult()!=grOk) {
		printf("\nFailure Graphic Initialize!\n");
		exit(1);
	}

	SetMode(VESA_1024X768X256);
	WORD height=100;
	WORD j,i;
	for(j=100;j<height+100;j++)
		for(i=0;i<100;i++){
			PutPixel(i,j,LIGHTBLUE);
			PutPixel(i,j+height,GetPixel(i,j));
			PutPixel(i,j+height*2,GetPixel(i,j+height));
		}
	getch();
	EndMode();
}