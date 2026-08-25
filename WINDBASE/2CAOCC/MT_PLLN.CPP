#include "mt_plln.h"

MetaPolyLine::MetaPolyLine():
	SVMetaElement(SVMETA_POLYLINE),Color(Black),Width(0)
{}
MetaPolyLine::~MetaPolyLine()
{}

void MetaPolyLine::Draw(JKDC& dc,RealCoordManager& coordman,JKRect32 paintrect)
{
	if(PolyPoint.NodeCount>1){
		JKPoint32 oldp,p;
		Point32Container *node=PolyPoint.HeadNode;
		p=node->Point;
		node=node->next;
		while(node){
			oldp=p;
			p=node->Point;
			SVLine(dc,coordman,oldp,p,Color,Width,paintrect);
			node=node->next;
		}
	}
}

BOOL MetaPolyLine::GetDrawRealRect(JKRect32& rect)
{
	return PolyPoint.GetRect(rect);
}

uint32 MetaPolyLine::GetFileSize()
{
	return sizeof(Color)+sizeof(Width)+sizeof(PolyPoint.NodeCount)+PolyPoint.NodeCount*sizeof(JKPoint32);
}

BOOL MetaPolyLine::Get(FILE *fp)
{
	if(fp){
		fread(&Width,sizeof(Width),1,fp);
		fread(&Color,sizeof(Color),1,fp);

		PolyPoint.ClearNode();
		uint32 pointcount;
		fread(&pointcount,sizeof(pointcount),1,fp);
		JKPoint32 p;
		while(pointcount--){
			fread(&p,sizeof(p),1,fp);
			PolyPoint.PutPoint(p);
		}
		return TRUE;
	}
	return FALSE;
}

BOOL MetaPolyLine::Put(FILE *fp)
{
	if(fp){
		fwrite(&Width,sizeof(Width),1,fp);
		fwrite(&Color,sizeof(Color),1,fp);

		uint32 pointcount=PolyPoint.NodeCount;;
		fwrite(&pointcount,sizeof(pointcount),1,fp);
		JKPoint32 p;
		Point32Container *node=PolyPoint.HeadNode;
		while(node){
			p=node->Point;
			fwrite(&p,sizeof(p),1,fp);
			node=node->next;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL MetaPolyLine::IsEqual(SVMetaElement *elem,uint16 compkind)
{
	if(SVMetaElement::IsEqual(elem,compkind)){
		MetaPolyLine *polyln=(MetaPolyLine *)elem;
		if(Color==polyln->Color&&Width==polyln->Width) return PolyPoint.IsEqual(polyln->PolyPoint,0);
	}
	return FALSE;
}

SVMetaElement *MetaPolyLine::Duplicate()
{
	MetaPolyLine *meta = new MetaPolyLine();
	if(meta){
		meta->Color=Color;
		meta->Width=Width;
		meta->PolyPoint=PolyPoint;
		return meta;
	}
	return 0;
}

uint32 MetaPolyLine::MinimizePointCount(int32 delta)
{
	Point32Que tempque;
	tempque.AppendPointQue(PolyPoint);
	JKPoint32 oldp,point,tp(0,0);
	if(tempque.GetPoint(oldp)){
		PolyPoint.PutPoint(oldp);
		BOOL isskip=FALSE;
		while(tempque.GetPoint(point)){
			tp=oldp-point;
			if((tp.x>0&&tp.x>delta)||(tp.x<0&&tp.x<-delta) ||
				(tp.y>0&&tp.y>delta)||(tp.y<0&&tp.y<-delta)){
				oldp=point;
				isskip=FALSE;
				PolyPoint.PutPoint(oldp);
			}
			else isskip=TRUE;
		}
		if(isskip) PolyPoint.PutPoint(point);
	}
  return PolyPoint.NodeCount;
}
