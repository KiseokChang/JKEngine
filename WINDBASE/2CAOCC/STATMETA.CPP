#include "mt_plln.h"
#include "mt_char.h"
#include "statmeta.h"

const char MetaFilePrefix[12+1]="StatMetaFile";

StatViewMeta::StatViewMeta(char *fname):
	DLLFrame<SVMetaElement>()
{
	if(!fname||!fname[0]) ChangeFileName("noname.smf");
	else ChangeFileName(fname);
}

StatViewMeta::~StatViewMeta()
{}

BOOL StatViewMeta::ChangeFileName(char *fname)
{
	if(fname&&fname[0]) {
		strncpy(MetaFileName,fname,MAXPATH);
		MetaFileName[MAXPATH]=0;
		return Load();
	}
	return FALSE;
}

void StatViewMeta::Draw(JKDC& dc,RealCoordManager& coordman,JKRect32 paintrect)
{
	SVMetaElement *node=HeadNode;
	while(node){
		node->Draw(dc,coordman,paintrect);
		node=node->next;
	}
}

BOOL StatViewMeta::Draw(JKDC& dc,RealCoordManager& coordman,char *fname,JKRect32 paintrect)
{
	if(fname&&fname[0]) {
		FILE *fp = fopen(fname,"rb");
		if(fp){
			char buffer[12+1];
			fread(buffer,12+1,1,fp);
			buffer[12]=0;
			if(!strcmp(buffer,MetaFilePrefix)) {
				uint32 count;
				fread(&count,sizeof(count),1,fp);
				WORD metatype;
				uint32 fsize;
				MetaPolyLine metapolyline;
				MetaTextOut metatextout;
				while(count--){
					fread(&metatype,sizeof(metatype),1,fp);
					fread(&fsize,sizeof(fsize),1,fp);
					switch(metatype){
						case SVMETA_POLYLINE:
							metapolyline.Get(fp);
							metapolyline.Draw(dc,coordman,paintrect);
						break;
						case SVMETA_TEXTOUT:
							metatextout.Get(fp);
							metatextout.Draw(dc,coordman,paintrect);
						break;
						default:
							fseek(fp,fsize,SEEK_CUR);
						break;
					}
				}
			}
			fclose(fp);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL StatViewMeta::Load()
{
	ClearNode();
	FILE *fp = fopen(MetaFileName,"rb");
	if(fp){
		char buffer[12+1];
		fread(buffer,12+1,1,fp);
		buffer[12]=0;
		if(!strcmp(buffer,MetaFilePrefix)) {
			uint32 count;
			fread(&count,sizeof(count),1,fp);
			WORD metatype;
			uint32 fsize;
			SVMetaElement *metaelem;
			while(count--){
				fread(&metatype,sizeof(metatype),1,fp);
				fread(&fsize,sizeof(fsize),1,fp);
				switch(metatype){
					case SVMETA_POLYLINE:
						metaelem = new MetaPolyLine();
						if(metaelem){
							metaelem->Get(fp);
							PutNodeLast(metaelem);
						}
						else fseek(fp,fsize,SEEK_CUR);
					break;
					case SVMETA_TEXTOUT:
						metaelem = new MetaTextOut();
						if(metaelem){
							metaelem->Get(fp);
							PutNodeLast(metaelem);
						}
						else fseek(fp,fsize,SEEK_CUR);
					break;
					default:
						fseek(fp,fsize,SEEK_CUR);
					break;
				}
			}
		}
		fclose(fp);
		return TRUE;
	}
	return FALSE;
}
BOOL StatViewMeta::Save()
{
	FILE *fp = fopen(MetaFileName,"wb");
	if(fp){
		fwrite(MetaFilePrefix,12+1,1,fp);
		uint32 count=NodeCount;;
		fwrite(&count,sizeof(count),1,fp);
		uint32 fsize;
		SVMetaElement *node=HeadNode;
		while(node){
			fwrite(&node->MetaType,sizeof(node->MetaType),1,fp);
			fsize=node->GetFileSize();
			fwrite(&fsize,sizeof(fsize),1,fp);
			node->Put(fp);
			node=node->next;
		}
		fclose(fp);
		return TRUE;
	}
	return FALSE;
}

