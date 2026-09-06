#include "rect32qu.h"

Rect32Que::Rect32Que():
	DLLFrame<RectContainer>()
{}

Rect32Que::~Rect32Que()
{}

BOOL Rect32Que::PutRect(JKRect32& rect)
{
	RectContainer *container = new RectContainer(rect);
	return PutNodeLast(container);
}

BOOL Rect32Que::PutRectFirst(JKRect32& rect)
{
	RectContainer *container = new RectContainer(rect);
	return PutNodeFirst(container);
}

BOOL Rect32Que::GetRect(JKRect32& rect,BOOL isdel)
{
	RectContainer *container=GetNodeFirst(isdel);
	if(container){
		rect=container->Rect;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

BOOL Rect32Que::GetRectLast(JKRect32& rect,BOOL isdel)
{
	RectContainer *container=GetNodeLast(isdel);
	if(container){
		rect=container->Rect;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

uint32 Rect32Que::AppendRectQue(Rect32Que& plus)
{
	return AppendDLLFrame(plus);
}

//====================================================================

uint32 Rect32Que::MinusRect(JKRect32 minus)
{
	JKRect32 rect;
	Rect32Que *TempQue= new Rect32Que();
	TempQue->AppendRectQue(*this);
	while(TempQue->GetRect(rect)){
		if(!minus.IsContain(rect)){
			if(rect.IsContain(minus)||rect.IsTouch(minus)){
				if(rect.top<minus.top&&minus.top<=rect.bottom){
					PutRect(JKRect32(rect.left,rect.top,rect.right,minus.top-1));
					if(rect.top<=minus.bottom&&minus.bottom<rect.bottom){
						if(rect.left<minus.left&&minus.left<=rect.right)
							PutRect(JKRect32(rect.left,minus.top,minus.left-1,minus.bottom));
						if(rect.left<=minus.right&&minus.right<rect.right)
							PutRect(JKRect32(minus.right+1,minus.top,rect.right,minus.bottom));
						PutRect(JKRect32(rect.left,minus.bottom+1,rect.right,rect.bottom));
					}
					else{
						if(rect.left<minus.left&&minus.left<=rect.right)
							PutRect(JKRect32(rect.left,minus.top,minus.left-1,rect.bottom));
						if(rect.left<=minus.right&&minus.right<rect.right)
							PutRect(JKRect32(minus.right+1,minus.top,rect.right,rect.bottom));
					}
				}
				else{
					if(rect.top<=minus.bottom&&minus.bottom<rect.bottom){
						if(rect.left<minus.left&&minus.left<=rect.right)
							PutRect(JKRect32(rect.left,rect.top,minus.left-1,minus.bottom));
						if(rect.left<=minus.right&&minus.right<rect.right)
							PutRect(JKRect32(minus.right+1,rect.top,rect.right,minus.bottom));
						PutRect(JKRect32(rect.left,minus.bottom+1,rect.right,rect.bottom));
					}
					else{
						if(rect.left<minus.left&&minus.left<=rect.right)
							PutRect(JKRect32(rect.left,rect.top,minus.left-1,rect.bottom));
						if(rect.left<=minus.right&&minus.right<rect.right)
							PutRect(JKRect32(minus.right+1,rect.top,rect.right,rect.bottom));
					}
				}
			}
			else PutRect(rect);
		}
	}
	delete TempQue;
	return NodeCount;
}

uint32 Rect32Que::MinusRect(Rect32Que& minus)
{
	JKRect32 temprect;
	RectContainer *container = minus.HeadNode;
	while(container) {
		temprect=container->Rect;
		if(!MinusRect(temprect)) break;
		container=container->next;
	}
	return NodeCount;
}

uint32 Rect32Que::MinimizeCount()
{
	Rect32Que *rectbuffer= new Rect32Que();
	rectbuffer->AppendRectQue(*this);

	RectContainer *container, *nextcontain;
	JKRect32 targetrect,remprect;
	while(rectbuffer->GetRect(targetrect)) {
		if(!targetrect.IsValid()) continue;
		container = HeadNode;
		while(container) {
			remprect=container->Rect;
			nextcontain=container->next;
			if(targetrect.bottom<remprect.top){
				RectContainer *tempcontain = new RectContainer(targetrect);
				if(!container->prev) HeadNode=tempcontain;
				AttachPrev(container,tempcontain);
				NodeCount++;
				break;
			}
			else if(targetrect.top<=remprect.bottom){
				if(targetrect.top<remprect.top){
					rectbuffer->PutRect(JKRect32(targetrect.left,targetrect.top,targetrect.right,remprect.top-1));
					targetrect.top=remprect.top;
				}
				else if(targetrect.top>remprect.top){
					rectbuffer->PutRect(JKRect32(remprect.left,remprect.top,remprect.right,targetrect.top-1));
					container->Rect.top=remprect.top=targetrect.top;
				}
				if(targetrect.bottom>remprect.bottom){
					rectbuffer->PutRect(JKRect32(targetrect.left,remprect.bottom+1,targetrect.right,targetrect.bottom));
					targetrect.bottom=remprect.bottom;
				}
				else if(targetrect.bottom<remprect.bottom){
					rectbuffer->PutRect(JKRect32(remprect.left,targetrect.bottom+1,remprect.right,remprect.bottom));
					container->Rect.bottom=remprect.bottom=targetrect.bottom;
				}
				if(targetrect.right<remprect.left-1){
					container->Rect=targetrect;
					targetrect=remprect;
				}
				else if(targetrect.left<=remprect.right+1){
					delete Detach(container);
					targetrect.left=min(targetrect.left,remprect.left);
					targetrect.right=max(targetrect.right,remprect.right);
				}
			}
			container=nextcontain;
		}
		if(!container) PutRect(targetrect);
	}
	delete rectbuffer;

	container = HeadNode;
	while(container&&container->next) {
		nextcontain=container->next;
		remprect=container->Rect;
		targetrect=nextcontain->Rect;
		if(remprect.left==targetrect.left && remprect.right==targetrect.right){
			if(remprect.bottom==targetrect.top-1){
				delete Detach(container);
				targetrect.top=nextcontain->Rect.top=remprect.top;
			}
		}
		container=nextcontain;
	}
	return NodeCount;
}

/*
uint32 Rect32Que::PlusRect(JKRect32 plus)
{
	Rect32Que *tempque= new Rect32Que();
	tempque->PutRect(plus);
	JKRect32 rect,plusrect;
	RectContainer *container,*tempcontain,*nextcontain;
	while(tempque->GetRect(plusrect)) {
		container = HeadNode;
		while(container) {
			rect=container->Rect;
			nextcontain=container->next;
			if(plusrect.bottom<rect.top){
				tempcontain = new RectContainer(plusrect);
				if(!container->prev) HeadNode=tempcontain;
				AttachPrev(container,tempcontain);
				NodeCount++;
				break;
			}
			else if(plusrect.top>rect.bottom){
			}
			else{
				if(plusrect.top<rect.top){
					tempque->PutRect(JKRect32(plusrect.left,plusrect.top,plusrect.right,rect.top-1));
					plusrect.top=rect.top;
				}
				else if(plusrect.top>rect.top){
					tempque->PutRect(JKRect32(rect.left,rect.top,rect.right,plusrect.top-1));
					container->Rect.top=rect.top=plusrect.top;
				}
				if(plusrect.bottom>rect.bottom){
					tempque->PutRect(JKRect32(plusrect.left,rect.bottom+1,plusrect.right,plusrect.bottom));
					plusrect.bottom=rect.bottom;
				}
				else if(plusrect.bottom<rect.bottom){
					tempque->PutRect(JKRect32(rect.left,plusrect.bottom+1,rect.right,rect.bottom));
					container->Rect.bottom=rect.bottom=plusrect.bottom;
				}
				if(plusrect.right<rect.left-1){
					container->Rect=plusrect;
					plusrect=rect;
				}
				else if(plusrect.left>rect.right+1){
				}
				else {
					delete Detach(container);
					plusrect.left=min(plusrect.left,rect.left);
					plusrect.right=max(plusrect.right,rect.right);
				}
			}
			container=nextcontain;
		}
		if(!container) PutRect(plusrect);
	}
	delete tempque;

	container = HeadNode;
	while(container&&container->next) {
		nextcontain=container->next;
		rect=container->Rect;
		plusrect=nextcontain->Rect;
		if(rect.left==plusrect.left && rect.right==plusrect.right){
			if(rect.bottom==plusrect.top-1){
				delete Detach(container);
				plusrect.top=nextcontain->Rect.top=rect.top;
			}
		}
		container=nextcontain;
	}
	return NodeCount;
}

uint32 Rect32Que::PlusRect(Rect32Que& plus)
{
	JKRect32 temprect;
	RectContainer *container = plus.HeadNode;
	while(container) {
		temprect=container->Rect;
		PlusRect(temprect);
		container=container->next;
	}
	return NodeCount;
}
*/
uint32 Rect32Que::ClippingRegion(JKRect32 rect)
{
	uint32 count=NodeCount;
	JKRect32 temprect;
	for(uint32 i=0;i<count;i++){
		GetRect(temprect);
		temprect=rect.CalcOverlap(temprect);
		if(temprect.IsValid())
			PutRect(temprect);
	}
	return NodeCount;
}

Rect32Que *Rect32Que::RestRegion(JKRect32& rect)
{
	Rect32Que *temp = new Rect32Que();
	if(temp) {
		temp->PutRect(rect);
		temp->MinusRect(*this);
	}
	return temp;
}

////////////////////////////////////////////////////////////////
Rect32Que *Rect32Que::Duplicate()
{
	Rect32Que *temp = new Rect32Que();
	if(temp) *temp=*this;
	return temp;
}

void Rect32Que::Offset(JKPoint32& p)
{
	RectContainer *container = HeadNode;
	while(container) {
		container->Rect.Offset(p);
		container=container->next;
	}
}

Rect32Que *Rect32Que::OffsetBy(JKPoint32& p)
{
	Rect32Que *temp=Duplicate();
	if(temp) temp->Offset(p);
	return temp;
}
