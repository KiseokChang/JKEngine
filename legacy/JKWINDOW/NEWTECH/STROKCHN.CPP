#include "pointchn.h"
#include "strokchn.h"

StrokeChain::StrokeChain(JKRect32&  rect,WORD refid,WORD objtype):
	RectChain(rect,refid,objtype),DataCode(-1)
{}

StrokeChain::~StrokeChain()
{}

void StrokeChain::AddPoint(JKPoint32& p)
{
	if(!NodeCount) {
		DataRect=JKRect32(p,p);
	}
	else{
		if(DataRect.left  > p.x) DataRect.left  = p.x;
		if(DataRect.right < p.x) DataRect.right = p.x;
		if(DataRect.top   > p.y) DataRect.top   = p.y;
		if(DataRect.bottom< p.y) DataRect.bottom= p.y;
	}
	AddObject(new PointChain(p));
}

JKPoint32 StrokeChain::CalcThreshold01(uint32 width,uint32 height)
{
	return JKPoint32(width/10,height/10);
}

uint32 StrokeChain::Resize_Filter(JKRect32 normal)
{
	if(!NodeCount) return 0;
	if(DataRect.IsEmpty()||normal.IsEmpty()) return 0;
	PointChain *chain=(PointChain *)HeadNode;
	JKPoint32 p=chain->DataPoint;
	p.x= (p.x-DataRect.left)*normal.Width()/DataRect.Width()+normal.left;
	p.y= (p.y-DataRect.top)*normal.Height()/DataRect.Height()+normal.top;
	chain->DataPoint=p;

	chain=(PointChain *)(chain->next);
	while(chain){
		JKPoint32 p=chain->DataPoint;
		p.x= (p.x-DataRect.left)*normal.Width()/DataRect.Width()+normal.left;
		p.y= (p.y-DataRect.top)*normal.Height()/DataRect.Height()+normal.top;

		JKPoint32 dif=p-((PointChain *)(chain->prev))->DataPoint;
		JKPoint32 thres=CalcThreshold01((uint32)normal.Width()+1,(uint32)normal.Height()+1);

		if(abs(dif.x)>=thres.x || abs(dif.y)>=thres.y){
			chain->DataPoint=p;
			chain=(PointChain *)(chain->next);
		}
		else{
			DWORD objid=chain->ObjectID;
			chain=(PointChain *)(chain->next);
			DestroySubObject(objid);
		}
	}
	return NodeCount;
}

int32 StrokeChain::FindStroke()
{
	if(NodeCount){
		if(DataRect.IsEmpty()) return -1;
		return NodeCount;
	}
	return -1;
}

BOOL StrokeChain::ProcessInput()
{
	uint32 RealCount=Resize_Filter(DataRect);
	if(RealCount){
		DataCode=FindStroke();
		return TRUE;
	}
	return FALSE;
}

