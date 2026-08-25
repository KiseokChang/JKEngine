#include "graph_pm.h"

static unsigned short VideoSelector;
static void AllocVideoSelector()
{
	asm		{
		mov  cx, 1
		mov  ax, 0000h          // DPMI Allocate Selector
		int  31h
		mov  VideoSelector, ax

		mov  bx, ax
		lar  cx, bx
		shr  ecx, 8
		or   cx, 4000h          // make it 'big'
		mov  ax, 0009h          // DPMI Set Descriptor Access Rights
		int  31h

		mov  bx, VideoSelector
		mov  dx, 0FFFFh
		xor  cx, cx         		// map 1M
		mov  ax, 0008h          // DPMI Set Segment Limit
		int  31h

		xor  dx, dx
		mov  cx, 0x000a
		mov  bx, VideoSelector
		mov  ax, 0007h          // DPMI Set Segment Base Address
		int  31h
	}
}


static void FreeVideoSelector()
{
	asm		{
		mov     bx, VideoSelector
		mov     ax, 0001h               // DPMI Free VideoSelector
		int     31h
	}
}

#pragma startup AllocVideoSelector
#pragma exit		FreeVideoSelector

//////////////////////////////////////////////////////////
static BYTE gFactor=0;

void SetVesaMode()
{
	asm {
		mov ah,0x4f
		mov al,2
		mov bx,0x105
		int 0x10
	}
/*
	ModeInfo *minfo = new ModeInfo;
	asm {
		mov ah,0x4f
		mov al,1
		mov cx,0x105
		les edi,minfo
		int 0x10
	}
	gFactor=0;
	while(minfo->WinGranularity<64){
		minfo->WinGranularity<<=1;
		gFactor++;
	}
	delete minfo;
*/
}

void EndVesaMode()
{
	asm{
		mov ah,0x4f
		mov al,2
		mov bx,0x3
		int 0x10
	}
}

void setpage()
{
//	asm push cx
	asm push bx
	asm push ax

	asm {
//		push ds
//		push cs
//		pop ds
//		mov cl,byte ptr gFactor
//		pop ds

//		shl dl,cl
		mov ah,0x4f
		mov al,5
		mov bx,0
		int 0x10
	}

	asm pop ax
	asm pop bx
//	asm pop cx
}

///////////////////////////////////////////////////////////////////////////

BYTE _writemode=0;

int16 _getmaxx()
{
	return 1023;
}

int16 _getmaxy()
{
	return 767;
}

void setwritemode(BYTE mode)
{
	_writemode=mode;
}

BYTE getwritemode()
{
	return _writemode;
}

void stosb_putmode()
{
	asm pop  edi
	asm pop  ecx

	asm push ax
	BYTE putmode=_writemode;
	asm pop ax

copy_put:
	{
		asm cmp putmode,0
		asm jnz xor_put

		asm mov es:[di],al
		asm inc di
		asm jmp stosb_end
	}
xor_put:
	{
		asm cmp putmode,1
		asm jnz or_put

		asm xor es:[di],al
		asm inc di
		asm jmp stosb_end
	}
or_put:
	{
		asm cmp putmode,2
		asm jnz and_put

		asm or es:[di],al
		asm inc di
		asm jmp stosb_end
	}
and_put:
	{
		asm cmp putmode,3
		asm jnz not_put

		asm and es:[di],al
		asm inc di
		asm jmp stosb_end
	}
not_put:
	{
		asm cmp putmode,4
		asm jnz remain_put

		asm push ax
		asm not al
		asm pop ax
		asm mov es:[di],al
		asm inc di
		asm jmp stosb_end
	}
remain_put:
	{
		asm mov es:[di],al
		asm inc di
	}
stosb_end:

	asm push ecx
	asm push edi
}

void REPstosb_putmode()
{
	asm pop edi
	asm pop ecx

	asm push ax
	BYTE putmode=_writemode;
	asm pop ax

	asm cmp ecx,0
	asm jz stosb_end
copy_put:
	{
		asm cmp putmode,0
		asm jnz xor_put
copy_loop:
		asm mov es:[di],al
		asm inc di
		asm loop copy_loop
		asm jmp stosb_end
	}
xor_put:
	{
		asm cmp putmode,1
		asm jnz or_put
xor_loop:
		asm xor es:[di],al
		asm inc di
		asm loop xor_loop
		asm jmp stosb_end
	}
or_put:
	{
		asm cmp putmode,2
		asm jnz and_put
or_loop:
		asm or es:[di],al
		asm inc di
		asm loop or_loop
		asm jmp stosb_end
	}
and_put:
	{
		asm cmp putmode,3
		asm jnz not_put
and_loop:
		asm and es:[di],al
		asm inc di
		asm loop and_loop
		asm jmp stosb_end
	}
not_put:
	{
		asm cmp putmode,4
		asm jnz remain_put
not_loop:
		asm push ax
		asm not al
		asm pop ax
		asm mov es:[di],al
		asm inc di
		asm loop not_loop
		asm jmp stosb_end
	}
remain_put:
	{
remain_loop:
		asm mov es:[di],al
		asm inc di
		asm loop remain_loop
	}
stosb_end:

	asm push ecx
	asm push edi
}

void putpixel(int16 x,int16 y,BYTE color)
{
//	int16 xw=1024;
	asm push es
	asm mov es,VideoSelector


/*
	asm	mov ax,y
	asm	mov bx,xw
	asm	mul bx
	asm	add ax,x
	asm	adc dx,0
	asm	mov di,ax
*/
	asm	mov dx,y
	asm	mov di,dx
	asm	shr dx,6
	asm	shl di,10
	asm	add di,x
	setpage();
	asm	mov al,color
	stosb_putmode();

	asm pop es
}

void hline(int16 x,int16 y,uint16 delta,BYTE color)
{
	int16 xwidth=delta;
	int16 xw=1024;

	asm push es
	asm mov es,VideoSelector

	asm xor ecx,ecx

	asm mov ax,y
	asm mov bx,xw
	asm mul bx
	asm add ax,x
	asm adc dl,0             		// add with carry
	asm mov di,ax            		// di is offset
	setpage();

pass:
	asm mov al,color   // fill color

	asm add di,xwidth
	asm jc  proc2

	asm sub di,xwidth
	asm mov cx,xwidth
	REPstosb_putmode();
	asm pop es
	return;
proc2:
	asm push di
	asm sub di,xwidth
	asm mov cx,di
	asm neg cx
	REPstosb_putmode();
	asm inc dl
	setpage();
	asm pop di
	asm mov cx,di
	asm xor di,di
	REPstosb_putmode();
	asm pop es
}

void vline(int16 x,int16 y,uint16 delta,BYTE color)
{
	int16 ywidth=delta;
	int16 xw=1024;

	asm push es
	asm mov es,VideoSelector

	asm xor ecx,ecx

	asm mov ax,y
	asm mov bx,xw
	asm mul bx
	asm add ax,x
	asm adc dl,0             		// add with carry
	asm mov bx,ax            		// di is offset
	setpage();

pass:
	asm mov al,color   // fill color
	asm mov cx,ywidth
lp1:
	asm mov di,bx

	stosb_putmode();

	asm add bx,xw
	asm jnc next2
	asm inc dl
	setpage();
next2:
	asm loop lp1
	asm pop es
}

void bar(int16 left, int16 top, int16 right, int16 bottom,BYTE color)
{
	int16 xwidth=right-left+1;
	int16 ywidth=bottom-top+1;
	if(xwidth<=0||xwidth<=0) return;

	int16 xw=1024;

	asm push es
	asm mov es,VideoSelector

	asm xor ecx,ecx

	asm mov ax,top
	asm mov bx,xw
	asm mul bx
	asm add ax,left
	asm adc dl,0             // add with carry
	asm mov bx,ax            // bx is offset
	setpage();

pass:
	asm mov al,color        // fill color
	asm mov cx,ywidth
lp2:
	asm push cx
	asm mov di,bx
	asm add di,xwidth
	asm jc  proc2

	asm mov cx,xwidth
	asm sub di,xwidth
	REPstosb_putmode();
	asm jmp next1

proc2:
	asm sub di,xwidth
	asm mov cx,di
	asm neg cx
	REPstosb_putmode();
	asm mov di,bx
	asm add di,xwidth
	asm inc dl
	setpage();
	asm mov cx,di
	asm xor di,di
	REPstosb_putmode();

next1:
	asm add bx,xw
	asm cmp bx,di
	asm jae next2
	asm inc dl
	setpage();
next2:
	asm pop cx
	asm loop lp2
	asm pop es
}
/*
void getline256(int16 xwidth,BYTE *buff)
{
	asm push dx
	asm push ds
	asm push es

	setpage();
pass:
	asm push ds
	asm pop  es
	asm mov edi,buff
	asm mov ds,VideoSelector

	asm add si,xwidth
	asm jnc pro1

	asm push si
	asm sub si,xwidth
	asm mov cx,si
	asm neg cx
	asm cld
	asm REP movsb

	asm inc dl
	setpage();
	asm pop si
	asm mov cx,si
	asm xor si,si
	asm REP movsb

	asm pop es
	asm pop ds
	asm pop dx
	return;
pro1:
	asm sub si,xwidth
	asm mov cx,xwidth
	asm cld

	asm REP movsb

	asm pop es
	asm pop ds
	asm pop dx
}

void putline256(int16 xwidth,BYTE *buff)
{
	asm push dx
	asm push ds
	asm push es

	setpage();
pass:

	asm mov es,VideoSelector
	asm mov esi,buff

	asm add di,xwidth
	asm jnc pro1

	asm push di
	asm sub di,xwidth
	asm mov cx,di
	asm neg cx
	asm cld
	asm REP movsb
	asm inc dl
	setpage();
	asm pop di
	asm mov cx,di
	asm xor di,di
	asm REP movsb

	asm pop es
	asm pop ds
	asm pop dx
	return;

pro1:
	asm sub di,xwidth
	asm mov cx,xwidth
	asm cld
	asm REP movsb

	asm pop es
	asm pop ds
	asm pop dx
}

void scrollup(int16 x1, int16 y1, int16 x2,int16 y2,uint16 lines,BYTE bkcolor)
{

	int16 count=y2-y1+1-lines;
	if(count>0){
		int16  xwidth=x2-x1+1;
		if(xwidth<=0) return;

		BYTE *buff= new BYTE[xwidth];
		if(!buff) return;

		int16  xw=1024;

		asm mov ax,y1
		asm mov bx,xw
		asm mul bx
		asm add ax,x1
		asm adc dl,0
		asm mov di,ax
		asm mov si,ax
		asm mov dh,dl

		asm mov cx,lines
	lp0:
		asm add si,xw
		asm jnc next
		asm inc dh           // si page increment
	next:
		asm loop lp0

		asm mov cx,count
	lp1:
		asm push cx
		asm xchg dh,dl

		getline256(xwidth,buff);

		asm xchg dh,dl

		putline256(xwidth,buff);

		asm add  si,xw
		asm jnc  next2
		asm inc  dh
	next2:
		asm add  di,xw
		asm jnc  next3
		asm inc  dl
	next3:
		asm pop  cx
		asm loop lp1

		delete[] buff;
		y1=y2-lines+1;
	}
	BYTE temp=_writemode;
	_writemode=0;
	bar(x1,y1,x2,y2,bkcolor);
	_writemode=temp;
}

void scrolldown(int16 x1, int16 y1, int16 x2,int16 y2,uint16 lines,BYTE bkcolor)
{
	int16 count=y2-y1+1-lines;
	if(count>0){
		int16  xwidth=x2-x1+1;
		if(xwidth<=0) return;

		BYTE *buff= new BYTE[xwidth];
		if(!buff) return;

		int16  xw=1024;

		asm mov ax,y2
		asm mov bx,xw
		asm mul bx
		asm add ax,x1
		asm adc dl,0
		asm mov di,ax
		asm mov si,ax
		asm mov dh,dl

		asm mov cx,lines
	lp0:
		asm sub si,xw
		asm jnc next
		asm dec dh           // si page increment
	next:
		asm loop lp0

		asm mov cx,count
	lp1:
		asm push cx
		asm xchg dh,dl

		getline256(xwidth,buff);

		asm xchg dh,dl

		putline256(xwidth,buff);

		asm sub  si,xw
		asm jnc  next2
		asm dec  dh
	next2:
		asm sub  di,xw
		asm jnc  next3
		asm dec  dl
	next3:
		asm pop  cx
		asm loop lp1

		delete[] buff;
		y2=y1+lines-1;
	}
	BYTE temp=_writemode;
	_writemode=0;
	bar(x1,y1,x2,y2,bkcolor);
	_writemode=temp;
}
void scrollleft(int16 x1, int16 y1, int16 x2,int16 y2,uint16 lines,BYTE bkcolor)
{
}
void scrollright(int16 x1, int16 y1, int16 x2,int16 y2,uint16 lines,BYTE bkcolor)
{
}
*/
void writeimageline(int16 x, int16 y,int8 *buff,int16 n,BYTE putmode)
{
	uint16 xwidth=1024;
	int32 tmp=(int32)y*xwidth+x;
	BYTE page=BYTE(HIWORD(tmp));
	uint16 off=LOWORD(tmp);
	BYTE tempmode=_writemode;
	_writemode = putmode;

	asm push es
	asm mov es,VideoSelector

	asm mov dh,0
	asm mov dl,page
	setpage();

	asm mov esi,buff
	asm xor edi,edi
	asm mov di,off

	asm add di,n
	asm jc proc2
	asm cld
	asm sub di,n
	asm xor ecx,ecx
	asm mov cx,n

imgnext1:
	asm lodsb
	stosb_putmode();
	asm loop imgnext1
	_writemode = tempmode;
	asm pop es
	return;
proc2:
	asm push di
	asm cld
	asm sub di,n
	asm xor ecx,ecx
	asm mov cx,di
	asm neg cx
imgnext2:
	asm lodsb
	stosb_putmode();
	asm loop imgnext2
	asm pop di
	asm mov dl,page
	asm inc dl
	setpage();
	asm xor ecx,ecx
	asm mov cx,di
	asm xor di,di
	asm cmp cx,0
	asm jz  imgend
imgnext3:
	asm lodsb
	stosb_putmode();
	asm loop imgnext3
imgend:
	_writemode = tempmode;
	asm pop es
}

void scanimageline(int16 x, int16 y,int8 *buff,int16 n)
{
	asm push ds

	uint16 xwidth=1024;
	int32 tmp=(int32)y*xwidth+x;
	BYTE page=BYTE(HIWORD(tmp));
	uint16 off=LOWORD(tmp);

	asm mov dl,page
	setpage();

	asm mov edi,buff
	asm mov ds,VideoSelector
	asm xor esi,esi
	asm mov si,off
	asm add si,n
	asm jc proc2

	asm sub si,n
	asm xor ecx,ecx
	asm mov cx,n
	asm REP movsb
	asm pop ds
	return;

proc2:
	asm push si
	asm sub si,n
	asm xor ecx,ecx
	asm mov cx,si
	asm neg cx
	asm REP movsb
	asm pop si
	asm mov dl,page
	asm inc dl
	setpage();
	asm xor ecx,ecx
	asm mov cx,si
	asm xor si,si
	asm REP movsb
	asm pop ds
}


BOOL putengimage(int16 x,int16 y,int8 *buff,BYTE color,uint16 ycount,BYTE lshift,BYTE rshift)
{
	if(!buff || !ycount || ycount>16 || lshift+rshift>=8) return FALSE;
	uint16 xcount=8-(lshift+rshift);
	x+=lshift;
	if(x+(int16)xcount-1<0 || x>1023) return FALSE;
	int16 xw=1024;

	asm push es
	asm mov es,VideoSelector

	asm mov ax,y
	asm mov bx,xw
	asm mul bx
	asm add ax,x
	asm adc dl,0             // add carry
	asm mov bx,ax            // bx is offset
	setpage();
pass:
	asm mov esi,buff
	asm mov al,color
	asm xor ecx,ecx
	asm mov cx,ycount
emain:
	asm push cx
	asm push bx
	asm mov di,bx           // use di instead of bx
	asm mov bl,[esi]

	asm mov cl,lshift   ///////////////////
	asm shl bl,cl				///////////////////
	asm xor ecx,ecx
	asm mov cx,xcount
eloop:
	asm shl bl,1
	asm jnc next
	stosb_putmode();
	asm jmp next1
next:
	asm inc edi
next1:
	asm cmp di,0
	asm jnz next2
	asm inc dl
	setpage();
next2:
	asm loop eloop
	asm inc esi
	asm pop bx
	asm add bx,xw
	asm cmp bx,di
	asm jae next3
	asm inc dl
	setpage();
next3:
	asm pop cx
	asm loop emain

	asm pop es
	return TRUE;
}

BOOL puthanimage(int16 x,int16 y,int8 *buff,BYTE color,uint16 ycount,BYTE lshift,BYTE rshift)
{
	if(!buff || !ycount || ycount>16 || lshift+rshift>=16) return FALSE;
	uint16 xcount=16-(lshift+rshift);
	x+=lshift;
	if(x+(int16)xcount-1<0 || x>1023) return FALSE;
	int16 xw=1024;

	asm push es
	asm mov es,VideoSelector

	asm mov ax,y
	asm mov bx,xw
	asm mul bx
	asm add ax,x
	asm adc dl,0             // add carry
	setpage();
	asm mov bx,ax            // bx is offset
pass:
	asm mov esi,buff
	asm mov al,color
	asm xor ecx,ecx
	asm mov cx,ycount
hmain:
	asm push cx
	asm push bx
	asm mov di,bx           // use di instead of bx
	asm mov bh,[esi]
	asm mov bl,[esi+1]

	asm mov cl,lshift   ///////////////////
	asm shl bx,cl				///////////////////
	asm xor ecx,ecx
	asm mov cx,xcount
hloop:
	asm shl bx,1
	asm jnc next
	stosb_putmode();
	asm jmp next1
next:
	asm inc di
next1:
	asm cmp di,0
	asm jnz next2
	asm inc dl
	setpage();
next2:
	asm loop hloop
	asm inc esi
	asm inc esi
	asm pop bx
	asm add bx,xw
	asm cmp bx,di
	asm jae next3
	asm inc dl
	setpage();
next3:
	asm pop cx
	asm loop hmain

	asm pop es
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
void OutPort(int16 portid, int16 value)
{
	asm push dx
	asm push ax
	asm mov dx,portid
	asm mov ax,value
	asm out dx,ax
	asm pop ax
	asm pop dx
}
void OutPortb(int16 portid, uint8 value)
{
	asm push dx
	asm push ax
	asm mov dx,portid
	asm mov al,value
	asm out dx,al
	asm pop ax
	asm pop dx
}
int16 InPort(int16 portid)
{
	asm push dx
	asm push ax
	int16 value;
	asm mov dx,portid
	asm in ax,dx
	asm mov value,ax
	asm pop ax
	asm pop dx
	return value;
}
uint8 InPortb(int16 portid)
{
	asm push dx
	asm push ax
	uint8 value;
	asm mov dx,portid
	asm in al,dx
	asm mov value,al
	asm pop ax
	asm pop dx
	return value;
}


