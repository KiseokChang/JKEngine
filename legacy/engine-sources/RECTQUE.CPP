#include "rectque.h"

RectQue::RectQue():
	DLLFrame<RectContainer>()
{}

RectQue::~RectQue()
{}

BOOL RectQue::PutRect(JKRect& rect)
{
	RectContainer *container = new RectContainer(rect);
	return PutNodeLast(container);
}

BOOL RectQue::PutRectFirst(JKRect& rect)
{
	RectContainer *container = new RectContainer(rect);
	return PutNodeFirst(container);
}

BOOL RectQue::GetRect(JKRect& rect,BOOL isdel)
{
	RectContainer *container=GetNodeFirst(isdel);
	if(container){
		rect=container->Rect;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

BOOL RectQue::GetRectLast(JKRect& rect,BOOL isdel)
{
	RectContainer *container=GetNodeLast(isdel);
	if(container){
		rect=container->Rect;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

uint32 RectQue::AppendRectQue(RectQue& plus)
{
	return AppendDLLFrame(plus);
}

//====================================================================

uint32 RectQue::MinusRect(JKRect minus)
{
	JKRect rect;
	RectQue *TempQue= new RectQue();
	TempQue->AppendRectQue(*this);
	while(TempQue->GetRect(rect)){
		if(!minus.IsContain(rect)){
			if(rect.IsContain(minus)||rect.IsTouch(minus)){
				if(rect.top<minus.top&&minus.top<=rect.bottom){
					PutRect(JKRect(rect.left,rect.top,rect.right,minus.top-1));
					if(rect.top<=minus.bottom&&minus.bottom<rect.bottom){
						if(rect.left<minus.left&&minus.left<=rect.right)
							PutRect(JKRect(rect.left,minus.top,minus.left-1,minus.bottom));
						if(rect.left<=minus.right&&minus.right<rect.right)
							PutRect(JKRect(minus.right+1,minus.top,rect.right,minus.bottom));
						PutRect(JKRect(rect.left,minus.bottom+1,rect.right,rect.bottom));
					}
					else{
						if(rect.left<minus.left&&minus.left<=rect.right)
							PutRect(JKRect(rect.left,minus.top,minus.left-1,rect.bottom));
						if(rect.left<=minus.right&&minus.right<rect.right)
							PutRect(JKRect(minus.right+1,minus.top,rect.right,rect.bottom));
					}
				}
				else{
					if(rect.top<=minus.bottom&&minus.bottom<rect.bottom){
						if(rect.left<minus.left&&minus.left<=rect.right)
							PutRect(JKRect(rect.left,rect.top,minus.left-1,minus.bottom));
						if(rect.left<=minus.right&&minus.right<rect.right)
							PutRect(JKRect(minus.right+1,rect.top,rect.right,minus.bottom));
						PutRect(JKRect(rect.left,minus.bottom+1,rect.right,rect.bottom));
					}
					else{
						if(rect.left<minus.left&&minus.left<=rect.right)
							PutRect(JKRect(rect.left,rect.top,minus.left-1,rect.bottom));
						if(rect.left<=minus.right&&minus.right<rect.right)
							PutRect(JKRect(minus.right+1,rect.top,rect.right,rect.bottom));
					}
				}
			}
			else PutRect(rect);
		}
	}
	delete TempQue;
	return NodeCount;
}

uint32 RectQue::MinusRect(RectQue& minus)
{
	JKRect temprect;
	RectContainer *container = minus.HeadNode;
	while(container) {
		temprect=container->Rect;
		if(!MinusRect(temprect)) break;
		container=container->next;
	}
	return NodeCount;
}

uint32 RectQue::MinimizeCount()
{
	RectQue *rectbuffer= new RectQue();
	rectbuffer->AppendRectQue(*this);

	RectContainer *container, *nextcontain;
	JKRect targetrect,remprect;
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
					rectbuffer->PutRect(JKRect(targetrect.left,targetrect.top,targetrect.right,remprect.top-1));
					targetrect.top=remprect.top;
				}
				else if(targetrect.top>remprect.top){
					rectbuffer->PutRect(JKRect(remprect.left,remprect.top,remprect.right,targetrect.top-1));
					container->Rect.top=remprect.top=targetrect.top;
				}
				if(targetrect.bottom>remprect.bottom){
					rectbuffer->PutRect(JKRect(targetrect.left,remprect.bottom+1,targetrect.right,targetrect.bottom));
					targetrect.bottom=remprect.bottom;
				}
				else if(targetrect.bottom<remprect.bottom){
					rectbuffer->PutRect(JKRect(remprect.left,targetrect.bottom+1,remprect.right,remprect.bottom));
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
uint32 RectQue::PlusRect(JKRect plus)
{
	RectQue *tempque= new RectQue();
	tempque->PutRect(plus);
	JKRect rect,plusrect;
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
					tempque->PutRect(JKRect(plusrect.left,plusrect.top,plusrect.right,rect.top-1));
					plusrect.top=rect.top;
				}
				else if(plusrect.top>rect.top){
					tempque->PutRect(JKRect(rect.left,rect.top,rect.right,plusrect.top-1));
					container->Rect.top=rect.top=plusrect.top;
				}
				if(plusrect.bottom>rect.bottom){
					tempque->PutRect(JKRect(plusrect.left,rect.bottom+1,plusrect.right,plusrect.bottom));
					plusrect.bottom=rect.bottom;
				}
				else if(plusrect.bottom<rect.bottom){
					tempque->PutRect(JKRect(rect.left,plusrect.bottom+1,rect.right,rect.bottom));
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

uint32 RectQue::PlusRect(RectQue& plus)
{
	JKRect temprect;
	RectContainer *container = plus.HeadNode;
	while(container) {
		temprect=container->Rect;
		PlusRect(temprect);
		container=container->next;
	}
	return NodeCount;
}
*/
uint32 RectQue::ClippingRegion(JKRect rect)
{
	uint32 count=NodeCount;
	JKRect temprect;
	for(uint32 i=0;i<count;i++){
		GetRect(temprect);
		temprect=rect.CalcOverlap(temprect);
		if(temprect.IsValid())
			PutRect(temprect);
	}
	return NodeCount;
}

RectQue *RectQue::RestRegion(JKRect& rect)
{
	RectQue *temp = new RectQue();
	if(temp) {
		temp->PutRect(rect);
		temp->MinusRect(*this);
	}
	return temp;
}

////////////////////////////////////////////////////////////////
RectQue *RectQue::Duplicate()
{
	RectQue *temp = new RectQue();
	if(temp) *temp=*this;
	return temp;
}

void RectQue::Offset(JKPoint& p)
{
	RectContainer *container = HeadNode;
	while(container) {
		container->Rect.Offset(p);
		container=container->next;
	}
}

RectQue *RectQue::OffsetBy(JKPoint& p)
{
	RectQue *temp=Duplicate();
	if(temp) temp->Offset(p);
	return temp;
}
