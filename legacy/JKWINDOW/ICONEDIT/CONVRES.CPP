#include "jkmodule.h"
#include "jkinit.h"
#include "jkapp.h"
#include "16to256.h"


void ConvertImageRes16To256(char *fname)
{
	if(!fname) return;
	unsigned count;
	FILE *fp=fopen(fname,"rb");
	if(!fp) return;
	fread(&count,sizeof(count),1,fp);
	fclose(fp);

	fp=fopen("temp.res","wb");
	if(fp) {
		fwrite(&count,sizeof(count),1,fp);

		long size;
		ResourceBase *res;
		BYTE *planimg,*packimg;
		for(unsigned i=0;i<count;i++){
			res=new ResourceBase(0,RES_IMAGE,fname,RT_FILE | RT_PACKFILE,i);
			if(res){
				BYTE *planimg = res->AllocRealData();
				if(planimg)	{
					unsigned width=*((unsigned *)planimg)+1;
					unsigned height=*((unsigned *)(planimg+2))+1;
					size = PackImageSize(JKRect(0,0,width-1,height-1));
					if(size) {
						packimg = new BYTE[size];
						if(packimg){
							if(PlanToPack(planimg,packimg)){
								fwrite(&size,sizeof(size),1,fp);
								fwrite(packimg,size,1,fp);
							}
							delete[] packimg;
						}
					}
					res->FreeRealData(planimg);
				}
				delete res;
			}
		}
		fclose(fp);
	}
}




EventHandler *CreateApplication()
{
	return 0;
}

void main(int argc, char **argv)
{
	JKModule::SetArgument(argc,argv);
	if(InitJKProgram()) {
		if(argc>=2) ConvertImageRes16To256(argv[1]);
		CloseJKProgram();
	}
	JKModule::ClearArgument();
}
