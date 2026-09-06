#ifndef _STRKQUE_H
#define _STRKQUE_H

#include "tmpltdll.h"
#include "jkpoint.h"

class STROKE{
public:
	STROKE(STROKE& strk)
	{ Set(strk.StartPoint,strk.EndPoint,strk.Code);	}

	STROKE(JKPoint& sp=JKPoint(),JKPoint& ep=JKPoint(),unsigned code=0)
	{ Set(sp,ep,code);	}

	void Set(JKPoint& sp,JKPoint& ep,unsigned code)
	{
		StartPoint=sp; EndPoint=ep; Code=code;
	}

	STROKE& operator =(STROKE& other)
	{
		StartPoint=other.StartPoint;
		EndPoint=other.EndPoint;
		Code=other.Code;
		return *this;
	}

	BOOL IsEqual(STROKE& other,BYTE compkind=0x07)
	{
		BOOL isequal=TRUE;
		if(isequal&&compkind&0x01&& StartPoint!=other.StartPoint) isequal=FALSE;
		if(isequal&&compkind&0x02&& EndPoint!=other.EndPoint) isequal=FALSE;
		if(isequal&&compkind&0x04&& Code!=other.Code) isequal=FALSE;
		return isequal;
	}

	JKPoint StartPoint;
	JKPoint EndPoint;
	unsigned Code;
};

class StrokeContainer
{
public:
	StrokeContainer(STROKE strk=STROKE()):
		Stroke(strk),prev(0),next(0)
	{}

	BOOL IsEqual(StrokeContainer *node,unsigned compkind=0x0007)
	{
		if(!node) return FALSE;
		return Stroke.IsEqual(node->Stroke,LOBYTE(compkind));
	}

	StrokeContainer *Duplicate()
	{
		return new StrokeContainer(Stroke);
	}


	STROKE Stroke;
	StrokeContainer *prev;
	StrokeContainer *next;
};


class StrokeQue: public DLLFrame<StrokeContainer>
{
public:
	StrokeQue();
 ~StrokeQue();

	BOOL GetStroke(STROKE& strk,BOOL isdel=TRUE);
	BOOL GetStrokeLast(STROKE& strk,BOOL isdel=TRUE);

	BOOL PutStroke(STROKE& strk);
	BOOL PutStrokeFirst(STROKE& strk);

};

#endif