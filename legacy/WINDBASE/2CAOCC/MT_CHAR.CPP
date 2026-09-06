#include "mt_char.h"

MetaTextOut::MetaTextOut(char *text):
	SVMetaElement(SVMETA_TEXTOUT),
	Position(0,0),TextBuffer(0),FontSize(16,16),
	TextAlign(FALSE),RotateAngle(0),SlantAngle(0),Color(0)
{
	SetText(text);
}
MetaTextOut::~MetaTextOut()
{}

BOOL MetaTextOut::SetText(char *buf)
{
	if(buf){
		if(TextBuffer) {
			delete[] TextBuffer;
			TextBuffer=0;
		}
		TextBuffer = new char[strlen(buf)+1];
		if(TextBuffer) {
			strcpy(TextBuffer,buf);
			return TRUE;
		}
	}
	return FALSE;
}

void MetaTextOut::Draw(JKDC& dc,RealCoordManager& coordman,JKRect32)
{
	if(TextBuffer){
		JKRect32 restrict(0,0,coordman.ClientSize.x-1,coordman.ClientSize.y-1);
		JKPoint32 pixp(Position);
		if(!coordman.RealToPixel(pixp,FALSE)) return;
		JKPoint32 pixsize(FontSize);
		coordman.RealToPixel_SX(pixsize.x);
		coordman.RealToPixel_SY(pixsize.y);

		dc.SetTextColor(Color);
		GlobalVFontMan->ResetCTM();
		GlobalVFontMan->SetFontSize(pixsize.x,pixsize.y);
		GlobalVFontMan->RotateFont(RotateAngle);
		GlobalVFontMan->SlantFont(SlantAngle);
		GlobalVFontMan->TextAlign=TextAlign;
		GlobalVFontMan->DrawString(dc,JKPoint(pixp.x,pixp.y),TextBuffer);
	}
}

BOOL MetaTextOut::GetDrawRealRect(JKRect32& rect)
{
	if(TextBuffer){
		rect.left=Position.x;
		rect.right=Position.x+FontSize.x*strlen(TextBuffer)-1;
		if(TextAlign){
			rect.top=Position.y;
			rect.bottom=Position.y+FontSize.y-1;
		}
		else{
			rect.top=Position.y-FontSize.y-1;
			rect.bottom=Position.y;
		}
		return TRUE;
	}
	return FALSE;
}

uint32 MetaTextOut::GetFileSize()
{
	return sizeof(int32)+(TextBuffer? strlen(TextBuffer):0)
			+sizeof(Position)+sizeof(FontSize)+sizeof(TextAlign)
			+sizeof(RotateAngle)+sizeof(SlantAngle)+sizeof(Color);
}

BOOL MetaTextOut::Get(FILE *fp)
{
	if(fp){
		int32 textlen;
		fread(&textlen,sizeof(textlen),1,fp);
		if(TextBuffer) delete[] TextBuffer;
		TextBuffer = new char[textlen+1];
		if(textlen) fread(TextBuffer,textlen,1,fp);
		TextBuffer[textlen]=0;
		fread(&Position,sizeof(Position),1,fp);
		fread(&FontSize,sizeof(FontSize),1,fp);
		fread(&TextAlign,sizeof(TextAlign),1,fp);
		fread(&RotateAngle,sizeof(RotateAngle),1,fp);
		fread(&SlantAngle,sizeof(SlantAngle),1,fp);
		fread(&Color,sizeof(Color),1,fp);
		return TRUE;
	}
	return FALSE;
}

BOOL MetaTextOut::Put(FILE *fp)
{
	if(fp){
		int32 textlen= TextBuffer? strlen(TextBuffer):0;
		fwrite(&textlen,sizeof(textlen),1,fp);
		if(textlen) fwrite(TextBuffer,textlen,1,fp);
		TextBuffer[textlen]=0;
		fwrite(&Position,sizeof(Position),1,fp);
		fwrite(&FontSize,sizeof(FontSize),1,fp);
		fwrite(&TextAlign,sizeof(TextAlign),1,fp);
		fwrite(&RotateAngle,sizeof(RotateAngle),1,fp);
		fwrite(&SlantAngle,sizeof(SlantAngle),1,fp);
		fwrite(&Color,sizeof(Color),1,fp);
		return TRUE;
	}
	return FALSE;
}

BOOL MetaTextOut::IsEqual(SVMetaElement *elem,uint16 compkind)
{
	if(SVMetaElement::IsEqual(elem,compkind)){
		MetaTextOut *metatext=(MetaTextOut *)elem;
		if(TextBuffer&&metatext->TextBuffer&&!strcmp(TextBuffer,metatext->TextBuffer)){
			if(Position!=metatext->Position) return FALSE;
			if(FontSize!=metatext->FontSize) return FALSE;
			if(TextAlign!=metatext->TextAlign) return FALSE;
			if(RotateAngle!=metatext->RotateAngle) return FALSE;
			if(SlantAngle!=metatext->SlantAngle) return FALSE;
			if(Color!=metatext->Color) return FALSE;
			return TRUE;
		}
	}
	return FALSE;
}

SVMetaElement *MetaTextOut::Duplicate()
{
	MetaTextOut *metatext = new MetaTextOut(TextBuffer);
	if(metatext){
		metatext->Position    = Position;
		metatext->FontSize    = FontSize;
		metatext->TextAlign   = TextAlign;
		metatext->RotateAngle = RotateAngle;
		metatext->SlantAngle  = SlantAngle;
		metatext->Color       = Color;
		return metatext;
	}
	return 0;
}


