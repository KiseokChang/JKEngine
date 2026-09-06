#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <dos.h>
#include <graphics.h>

#include "sintbl.h"
#include "vgavideo.h"
#include "drawchar.h"


void Wait(int16 sec);
void DrawBigChar();
void DrawStringWithShade(int16 x, int16 y,int16 fgcolor,int16 bkcolor,int16 distance, char *str);
void DrawSpiral();
void DrawWheel();
void DrawSlant();


/*********************************************************************/

void main()
{
	if(InitFontMachinery(".")){
		int gd=DETECT,gm;
		initgraph(&gd,&gm,"..\\bgi");
		while(1){
			if(kbhit()&&getch()==27) break;
			DrawBigChar();  Wait(3);
			DrawWheel();    Wait(3);
			DrawSlant();    Wait(3);
			DrawSpiral();   Wait(3);
		}
		closegraph();
		CloseFontMachinery();
	}
}


void Wait(int16 sec)
{
	int		n = 0;
	int		i;

	sec *= 10;
	while (!kbhit()) {
		delay(100);
		if (++n > sec) return;
	}
	if (getch() == 27) {
		CloseFontMachinery();
		closegraph();
		exit(0);
	}
}

void DrawStringWithShade(int16 x, int16 y, int16 fgcolor, int16 bkcolor,
				int16 distance, char *str)
{
	setcolor(bkcolor);
	DrawString(x + distance, y + distance, str);
	setcolor(fgcolor);
	DrawString(x, y, str);
}


void DrawWheel()
{
	int		i, x, y, xc, yc, n;
	int		SinVal, CosVal;

	static char *DemoStr[4] = { "Ðe","‹i", "¬á", "ÁA" };

	setbkcolor(MAGENTA);
	cleardevice(); 

	ResetCTM();
	SetFont(HANGUL, FN_MYUNGJO);
	SetFontSize(120, 120);
	n = 0;

	for (y = 96; y < 100; y += 96) {
		x = 0;
		for (i = 0; i < 720; i += 15) {
			setcolor(MAGENTA);
			bar(x - 105,y-90,190,180);
			RotateFont(15);
			if (++n >= 4) n = 0;
			SinVal = SinTbl[(i + 15) % 360];
			CosVal = CosTbl[(i + 15) % 360];
			xc = x - ((long)60 * (CosVal + SinVal)) / 1024;
			yc = y + ((long)60 * (-SinVal + CosVal)) / 1024;
			setcolor(YELLOW);
			DrawString(xc, yc,  DemoStr[n]);
			x += 15;
			if (kbhit()) {
				if (getch() == 27) return;
				else getch();	/* pause */
			}
		}
	}
}


void DrawSpiral()
{
	int		i;
	int		x, y, r, xc, yc, color;

	setbkcolor(BLUE);
  cleardevice();


	ResetCTM();
	SetFont(HANGUL, FN_MYUNGJO);
	setcolor(YELLOW);
	SetFontSize(180, 180);

	xc = 320;
	yc = 240;
	color = 0;
	for (i = 0; i < 720; i += 10) {
		SetFontSize(4 + i / 6, 4 + i / 6);
		setcolor(color);
		color = (color + 1) % 16;
		if (color == BLUE) color++;
		r = 4 + i / 4;
		ResetCTM();
		RotateFont(i);
		x = xc + (int)((long)r * CosTbl[i % 360] / 1024);
		y = yc + (int)((long)r * SinTbl[i % 360] / 1024);
		DrawString(x, y, "MicroSoftware");
		if (kbhit()) {
			if (getch() == 27) return;
			else getch();	/* pause */
		}
	}
}


void DrawSlant()
{
	int		i;

	setbkcolor(GREEN);
	cleardevice();

	ResetCTM();
	SetFont(HANGUL, FN_MYUNGJO);
	for (i = 0; i <= 45; i += 5) {
		ResetCTM();
		SetFontSize(160, 160);
		SlantFont(-i);
		setcolor(LIGHTCYAN);
		DrawString(200, 380, "ÐeŠ‚");
		delay(100);
		if(kbhit()) {
			if(getch()==27) return;
			else getch();	/* pause */
		}
		setcolor(GREEN);
		bar(0, 220, 640, 160);
	}
	for (i = 45; i >= -45; i -= 5) {
		ResetCTM();
		SetFontSize(160, 160);
		SlantFont(-i);
		setcolor(LIGHTCYAN);
		DrawString(200, 380, "ÐeŠ‚");
		delay(100);
		if(kbhit()) {
			if(getch() == 27) return;
			else getch();	/* pause */
		}
		setcolor(GREEN);
		bar(0, 220, 640, 160);
	}
	for(i = -45; i <= 0; i += 5) {
		ResetCTM();
		SetFontSize(160, 160);
		SlantFont(-i);
		setcolor(LIGHTCYAN);
		DrawString(200, 380, "ÐeŠ‚");
		delay(100);
		if(kbhit()) {
			if(getch() == 27) return;
			else getch();	/* pause */
		}
		setcolor(GREEN);
		if(i) bar(0, 220, 640, 160);
	}
}


void DrawBigChar()
{
	int		i;
	int		x, y, r, xc, yc, color;
	static	int	size[10] = { 16, 24, 32, 40, 48, 80, 120, 200, 300, 500 };
	char	s[100];

	setbkcolor(BLUE);
	cleardevice();
	setcolor(WHITE);

	ResetCTM();
	SetFont(HANGUL, FN_MYUNGJO);
	setcolor(WHITE);

	for (i = 0; i < 10; i++) {
		cleardevice();
		SetFontSize(32, 32);
		sprintf(s, "%d x %d", size[i], size[i]);
		DrawString(10, 32, s);
		SetFontSize(size[i], size[i]);
		if (i < 7)
			DrawString(0, 40 + size[i], " a·¡Ça¡­¡ÏaËa¶Á´á");
		else
			DrawString(0, 40 + size[i], " a·¡");
		if (kbhit()) {
			if (getch() == 27) return;
			else getch();	/* pause */
		}
		Wait(2);
	}
}

