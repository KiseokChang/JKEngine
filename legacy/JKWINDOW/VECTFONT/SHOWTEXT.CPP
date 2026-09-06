#include <stdio.h>
#include <stdlib.h>
#include <bios.h>
#include <conio.h>
#include <graphics.h>

#include "vgavideo.h"
#include "drawchar.h"


void main(int16 ac, char *av[])
{
	FILE	*fp;
	int16		c, c2;
	int16		xsize, ysize, x, y, xinc, yinc, fontid;

	if (ac < 4) {
		printf("Usage: showtext  xsize  ysize  text-file\n");
		exit(-1);
	}

	xsize = atoi(av[1]);
	ysize = atoi(av[2]);
	if (xsize < 4 || ysize < 4 || xsize > 500 || ysize > 500) {
		printf("Invalid size (must be from 4 to 500 dot)\n");
		exit(-1);
	}

	if ((fp = fopen(av[3], "rt")) == NULL) {
		printf("Can't open file %s\n", av[4]);
		exit(-1);
	}

	if (!InitFontMachinery(".")) {
		printf("Can't initialize font engine\n");
		exit(-1);
	}

	int gd=DETECT,gm;
	initgraph(&gd,&gm,"..\\bgi");
  setcolor(WHITE);
	bar(0,0,getmaxx(),getmaxy());

	SetFont(HANGUL, FN_MYUNGJO);
	SetFont(ENGLISH, FN_MYUNGJO);
	SetFontSize(xsize, ysize);

	x=0; y=ysize;
	while((c=fgetc(fp))!=EOF) {
		if(c=='\r'){
			x=0;
		}
		else if(c=='\n') {
			x=0; y+=ysize*3/2;
		}
		else if(c<128) {          
			setcolor(BLACK);
			DrawChar(x,y,c,xinc,yinc);
			x += xinc; y += yinc;
		}
		else {
			c2 = fgetc(fp);
			setcolor(LIGHTBLUE);
			DrawChar(x,y,c*256+c2,xinc,yinc);
			x += xinc; y += yinc;
		}

		if (kbhit()&&getch()==27) break;

		if(y>getmaxy()) {
			if (getch() == 27) break;
			x = 0;
			y = ysize;
			cleardevice();
		}
	}
	getch();
	fclose(fp);
	closegraph();
	CloseFontMachinery();
}

