#include <dos.h>
#include <alloc.h>
#include "vesa256.h"

static INT xwidth;
static BYTE gFactor=0;


void setpage()
{
	asm{
		push dx
		push cx
		push bx
		push ax

		mov ah,0x4f
		mov al,5
		mov bh,0
		mov bl,0
		mov cl,gFactor
		shl dl,cl
		int 0x10

		pop ax
		pop bx
		pop cx
		pop dx

	}
}

BOOL IsVesa()
{
	INT retflag;

	SVGAInfo *sinfo = new SVGAInfo;

	asm{
		mov ah,0x4f
		mov al,0
		les di,sinfo
		int 0x10
		mov retflag,ax
	}
	delete sinfo;
	if(retflag==0x4f) return TRUE;
	return FALSE;
}

void SetMode(INT mode)
{
	ModeInfo *minfo;
	switch(mode){
		case VESA_640X480X256:
			xwidth=640;
			break;
		case VESA_800X600X256:
			xwidth=800;
			break;
		case VESA_1024X768X256:
			xwidth=1024;
			break;
		default:
			return;
	}

	minfo = new ModeInfo;
	asm{
		mov ah,0x4f
		mov al,2
		mov bx,mode
		int 0x10

		mov ah,0x4f
		mov al,1
		mov cx,mode
		les di,minfo
		int 0x10
	}

	for(gFactor=0;minfo->WinGranularity<64;gFactor++)
		minfo->WinGranularity<<=1;
	delete minfo;
}

void EndMode()
{
	asm{
		mov ah,0x4f
		mov al,2
		mov bx,3
		int 0x10
	}
}


void PutPixel(INT x,INT y,INT color)
{
	asm{
		mov ax,0xa000
		mov es,ax

		mov ax,y
		mov bx,xwidth
		mul bx
		add ax,x
		adc dx,0
		mov di,ax
	}
	setpage();
	asm{
		mov ah,byte ptr color
		mov es:[di],ah
	}

}

INT GetPixel(INT x,INT y)
{
	BYTE color;
	asm{
		mov ax,0xa000
		mov es,ax

		mov ax,y
		mov bx,xwidth
		mul bx
		add ax,x
		adc dx,0
		mov di,ax
	}
	setpage();
	asm{
		mov ah,es:[di]
		mov color,ah
	}
	return INT(color);
}