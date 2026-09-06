#include <stdio.h>
#include "mt_plln.h"
#include "statmeta.h"

void main(int argc,char *argv[])
{
	if(argc!=2&&argc!=3) return;
	int32 delta=3;
	if(argc==3)	delta=atol(argv[2]);
	StatViewMeta StatMeta(argv[1]);
	MetaPolyLine *metaplln;

	SVMetaElement *node=StatMeta.HeadNode;
	while(node){
		if(node->MetaType==SVMETA_POLYLINE){
			metaplln = (MetaPolyLine *)node;
			metaplln->MinimizePointCount(delta);
		}
		node=node->next;
	}
  StatMeta.Save();
}

