#include <stdio.h>

typedef BYTE BYTE;

BYTE StColor[14][3]={
	0x00,0x00,0x00,  // Black
	0x3F,0x3F,0x3F,  // White
	0x00,0x00,0x2A,  // Blue
	0x00,0x2A,0x00,  // Green
	0x00,0x2A,0x2A,  // Cyan
	0x2A,0x00,0x00,  // Red
	0x2A,0x00,0x2A,  // Magenta
	0x2A,0x15,0x00,  // Brown
	0x10,0x10,0x3F,  // LtBlue
	0x10,0x3F,0x10,  // LtGreen
	0x10,0x3F,0x3F,  // LtCyan
	0x3F,0x10,0x10,  // LtRed
	0x3F,0x10,0x3F,  // LtMagenta
	0x3F,0x3F,0x10   // Yellow
};

BYTE grade[6]={0x05,0x0f,0x1c,0x25,0x30,0x38};

void main()
{
	FILE *fp = fopen("jkpalett.dat","wb");
	if(fp){
		WORD count=1;
		fwrite(&count,sizeof(count),1,fp);
		LONG size=768;
		fwrite(&size,sizeof(size),1,fp);

		WORD i;
		BYTE rgb[3];
		for(i=0;i<14;i++){
			rgb[0]=StColor[i][0];	rgb[1]=StColor[i][1];	rgb[2]=StColor[i][2];
			fwrite(rgb,sizeof(BYTE)*3,1,fp);
		}

		for(WORD r=0;r<6;r++){
			rgb[0]=grade[r];
			for(WORD g=0;g<6;g++){
				rgb[1]=grade[g];
				for(WORD b=0;b<6;b++){
					rgb[2]=grade[b];
					if(r!=g || r!=b) fwrite(rgb,sizeof(BYTE)*3,1,fp);
				}
			}
		}

		for(i=0;i<32;i++){
			rgb[0]=rgb[1]=rgb[2]=i*2;
			fwrite(rgb,sizeof(BYTE)*3,1,fp);
		}
		fclose(fp);
	}
}