#include "joglobal.h"
#include "basefunc.h"
#include "boolent.h"
#include "dateent.h"
#include "sollucvt.h"
#include "persnrec.h"

PersonRecord::PersonRecord():
	RecordBase(1,"PersonRecord"),ObserveIndex(-1)
{
	HuboManager *huboman;

	AddEntry(new EntryBase(0,"¬÷¡w",10));
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("·¡¥w"));
		huboman->Add(-1,new HuboBase("·©¥w"));
		huboman->Add(-1,new HuboBase("¬w¥w"));
		huboman->Add(-1,new HuboBase("¥w¸w"));
		huboman->Add(-1,new HuboBase("Ða¬a"));
		huboman->Add(-1,new HuboBase("º—¬a"));
		huboman->Add(-1,new HuboBase("¬w¬a"));
		huboman->Add(-1,new HuboBase("¶¥¬a"));
		huboman->Add(-1,new HuboBase("º…¶á"));
		huboman->Add(-1,new HuboBase("­¡¶á"));
		huboman->Add(-1,new HuboBase("º—¶á"));
		huboman->Add(-1,new HuboBase("”¶á"));
		huboman->Add(-1,new HuboBase("­¡w"));
		huboman->Add(-1,new HuboBase("º—w"));
		huboman->Add(-1,new HuboBase("”w"));
		huboman->Add(-1,new HuboBase("º…¸w"));
		huboman->Add(-1,new HuboBase("­¡¸w"));
		AddEntry(new EntryBase(1,"‰‹s",4,huboman));
	}
	huboman = new HuboManager();
	if(huboman){
		char buffer[10];
		uint16 len=strlen(BudaeName);
		strcpy(buffer,BudaeName);
		strcpy(buffer+len,"-HQ");
		huboman->Add(-1,new HuboBase(buffer));
		strcpy(buffer+len,"-A");
		huboman->Add(-1,new HuboBase(buffer));
		strcpy(buffer+len,"-B");
		huboman->Add(-1,new HuboBase(buffer));
		strcpy(buffer+len,"-C");
		huboman->Add(-1,new HuboBase(buffer));
		AddEntry(new EntryBase(2,"­¡­¢",9,huboman));
	}
	AddEntry(new EntryBase(3,"Š…¤å",11));

	AddEntry(new EntryBase(4,"ºËb‹¡",4));
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("          "));
		huboman->Add(-1,new HuboBase("Í¡®      "));
		huboman->Add(-1,new HuboBase("Í¡¤e¸w    "));
		huboman->Add(-1,new HuboBase("¦¬a®    "));
		huboman->Add(-1,new HuboBase("¬a®      "));
		huboman->Add(-1,new HuboBase("Ãbˆb®    "));
		huboman->Add(-1,new HuboBase("·A¬å¥w    "));
		huboman->Add(-1,new HuboBase("¢¬å¥w    "));
		huboman->Add(-1,new HuboBase("ŠaÑÅ¥w    "));
		huboman->Add(-1,new HuboBase("‰ÅÃb¥w    "));
		huboman->Add(-1,new HuboBase("¸b¸å¬á‹¡¥w"));
		huboman->Add(-1,new HuboBase("VCO       "));
		huboman->Add(-1,new HuboBase("HCO       "));
		huboman->Add(-1,new HuboBase("COM       "));
		huboman->Add(-1,new HuboBase("‹¡¢¥w    "));
		huboman->Add(-1,new HuboBase("¶…¸å¥w    "));
		huboman->Add(-1,new HuboBase("¸÷§¡¥w    "));
		huboman->Add(-1,new HuboBase("Èe´¢¥w    "));
		huboman->Add(-1,new HuboBase("ÑÁÐb¥w‹¡¥w"));
		huboman->Add(-1,new HuboBase("¹¡Ÿ¡¥w    "));
		AddEntry(new EntryBase(5,"»¢À‚",10,huboman));
	}
	AddEntry(new DateEntry(6,"·³”·©"));
	AddEntry(new DateEntry(7,"¥¡»¢·©"));
	AddEntry(new BooleanEntry(8,"´·/·q"));
	AddEntry(new DateEntry(9,"¬—‘e¶©·©"));
	AddEntry(new EntryBase(10,"º£¥—w¢¤åÑ¡",14));
	AddEntry(new EntryBase(11,"¸åÑÁ¤åÑ¡",14));

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("          "));
		huboman->Add(-1,new HuboBase("¬á¶‰Ëb¥i¯¡"));
		huboman->Add(-1,new HuboBase("¦¬e‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("”Š‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("·¥Àå‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("‰×º‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("”¸å‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("‰w‹¡•¡    "));
		huboman->Add(-1,new HuboBase("ˆw¶¥•¡    "));
		huboman->Add(-1,new HuboBase("Â—À÷¦‚•¡  "));
		huboman->Add(-1,new HuboBase("Â—À÷q•¡  "));
		huboman->Add(-1,new HuboBase("‰w¬w¦‚•¡  "));
		huboman->Add(-1,new HuboBase("‰w¬wq•¡  "));
		huboman->Add(-1,new HuboBase("¸åœa¦‚•¡  "));
		huboman->Add(-1,new HuboBase("¸åœaq•¡  "));
		huboman->Add(-1,new HuboBase("¹Aº•¡    "));
		AddEntry(new EntryBase(12,"¥¥¸â¯¡•¡",10,huboman));
	}
	AddEntry(new EntryBase(13,"¥¥¸â­A¦",40));

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("          "));
		huboman->Add(-1,new HuboBase("¬á¶‰Ëb¥i¯¡"));
		huboman->Add(-1,new HuboBase("¦¬e‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("”Š‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("·¥Àå‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("‰×º‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("”¸å‰×µb¯¡"));
		huboman->Add(-1,new HuboBase("‰w‹¡•¡    "));
		huboman->Add(-1,new HuboBase("ˆw¶¥•¡    "));
		huboman->Add(-1,new HuboBase("Â—À÷¦‚•¡  "));
		huboman->Add(-1,new HuboBase("Â—À÷q•¡  "));
		huboman->Add(-1,new HuboBase("‰w¬w¦‚•¡  "));
		huboman->Add(-1,new HuboBase("‰w¬wq•¡  "));
		huboman->Add(-1,new HuboBase("¸åœa¦‚•¡  "));
		huboman->Add(-1,new HuboBase("¸åœaq•¡  "));
		huboman->Add(-1,new HuboBase("¹Aº•¡    "));
		AddEntry(new EntryBase(14,"º­¡¯¡•¡",10,huboman));
	}
	AddEntry(new EntryBase(15,"º­¡­A¦",40));

	AddEntry(new EntryBase(16,"¸a‰b»w",19));

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("Ã¥¦"));
		huboman->Add(-1,new HuboBase("´ô·q"));
		huboman->Add(-1,new HuboBase("‰¦"));
		AddEntry(new EntryBase(17,"¦Ã¥",4,huboman));
	}

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("Ã¥¡¡"));
		huboman->Add(-1,new HuboBase("´ô·q"));
		huboman->Add(-1,new HuboBase("‰¡¡"));
		AddEntry(new EntryBase(18,"¡¡Ã¥",4,huboman));
	}
	AddEntry(new BooleanEntry(19,"¦¡¡¥iˆá"));
	AddEntry(new EntryBase(20,"Ñw¹A",15));
	AddEntry(new EntryBase(21,"•¢¸a",9));
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("¦¡¡µÁ •·ˆá"));
		huboman->Add(-1,new HuboBase("¦…ˆa       "));
		AddEntry(new EntryBase(22,"ºˆá",11,huboman));
	}
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("£¡Ñ¥"));
		huboman->Add(-1,new HuboBase("‰iÑ¥"));
		huboman->Add(-1,new HuboBase("•·ˆá"));
		huboman->Add(-1,new HuboBase("´¢Ñ¥"));
		AddEntry(new EntryBase(23,"‰iÑ¥‰Å‰",4,huboman));
	}
	AddEntry(new EntryBase(24,"´·¥·¡Ÿq",8));

	AddEntry(new EntryBase(60,"¸åÊ¥¢¬w·",1));
	AddEntry(new EntryBase(61,"¸åÊ¥¢Ða·",1));
	AddEntry(new EntryBase(62,"¸åÊÑÁ¢…®",3));
	AddEntry(new EntryBase(63,"Á·¤å",9));
	AddEntry(new EntryBase(25,"q·eˆá",2));
	AddEntry(new EntryBase(26,"q·eˆá",2));

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("¸aˆa"));
		huboman->Add(-1,new HuboBase("¸å­A"));
		huboman->Add(-1,new HuboBase("¶©­A"));
		huboman->Add(-1,new HuboBase("‰Å¬a"));
		AddEntry(new EntryBase(27,"»³­¡·A",4,huboman));
	}
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("´aÌaËa"));
		huboman->Add(-1,new HuboBase("´·µ¢  "));
		huboman->Add(-1,new HuboBase("Ðeµ¢  "));
		AddEntry(new EntryBase(28,"»³ÑwÈ",6,huboman));
	}

	AddEntry(new EntryBase(29,"ˆåÍw",5));
	AddEntry(new EntryBase(30,"¤w",5));
	AddEntry(new EntryBase(31,"¶©®·³",8));

	AddEntry(new EntryBase(32,"Ðb‰Á¡w",20));

	AddEntry(new EntryBase(33,"®·³¶¥",30));

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("‰¡¹©    "));
		huboman->Add(-1,new HuboBase("”¸    "));
		huboman->Add(-1,new HuboBase("”¹©    "));
		huboman->Add(-1,new HuboBase("”Ðb¶¥¸"));
		huboman->Add(-1,new HuboBase("”Ðb¶¥¹©"));
		AddEntry(new EntryBase(34,"ÂA¹·Ðbb",8,huboman));
	}
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("Ð·¬÷"));
		huboman->Add(-1,new HuboBase("¶AÐ·¬÷"));
		AddEntry(new EntryBase(35,"¬÷‰b",6,huboman));
	}

	AddEntry(new BooleanEntry(36,"b¹A"));
	AddEntry(new BooleanEntry(37,"¸÷Ðb"));
	AddEntry(new BooleanEntry(38,"ÊAÐb"));

	AddEntry(new EntryBase(39,"¯¥¸w",3));
	AddEntry(new EntryBase(40,"ÁAº—",3));
	AddEntry(new EntryBase(41,"¶E½¢¯¡b",3));
	AddEntry(new EntryBase(42,"µ¡Ÿe½¢¯¡b",3));
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("RH+ A "));
		huboman->Add(-1,new HuboBase("RH+ B "));
		huboman->Add(-1,new HuboBase("RH+ AB"));
		huboman->Add(-1,new HuboBase("RH+ O "));
		huboman->Add(-1,new HuboBase("RH- A "));
		huboman->Add(-1,new HuboBase("RH- B "));
		huboman->Add(-1,new HuboBase("RH- AB"));
		huboman->Add(-1,new HuboBase("RH- O "));
		AddEntry(new EntryBase(43,"ABOÑi´‚Ñw",6,huboman));
	}
	AddEntry(new EntryBase(44,"Ñi´s",7));
	AddEntry(new EntryBase(45,"»©¥w",16));
	AddEntry(new EntryBase(46,"Âá£¡",16));
	AddEntry(new EntryBase(47,"Ëb‹¡",16));
	AddEntry(new EntryBase(48,"­¡º",9));
	AddEntry(new EntryBase(49,"”q¤",9));
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("        "));
		huboman->Add(-1,new HuboBase("‹¡•¢Ša  "));
		huboman->Add(-1,new HuboBase("ÀåºŠa  "));
		huboman->Add(-1,new HuboBase("¦‰Ša    "));
		huboman->Add(-1,new HuboBase("¶¥¦‰Ša  "));
		huboman->Add(-1,new HuboBase("·¡¯iœqŠa"));
		huboman->Add(-1,new HuboBase("»w¬e•¡  "));
		huboman->Add(-1,new HuboBase("Àå•¡Ša  "));
		AddEntry(new EntryBase(50,"¹·Ìa",8,huboman));
	}
	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("    "));
		huboman->Add(-1,new HuboBase("­A"));
		huboman->Add(-1,new HuboBase("µw­A"));
		AddEntry(new EntryBase(51,"¯¥´w¸÷•¡",4,huboman));
	}

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("      "));
		huboman->Add(-1,new HuboBase("­¡‘e¶¥"));
		huboman->Add(-1,new HuboBase("Ša•¡­¡"));
		AddEntry(new EntryBase(52,"Àá¤é¸w­¡",6,huboman));
	}
	AddEntry(new EntryBase(53,"¶w",26));

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("      "));
		huboman->Add(-1,new HuboBase("¬w‹e¥w"));
		huboman->Add(-1,new HuboBase("KMPI  "));
		huboman->Add(-1,new HuboBase("1 —w‹s"));
		huboman->Add(-1,new HuboBase("2 —w‹s"));
		huboman->Add(-1,new HuboBase("3 —w‹s"));
		huboman->Add(-1,new HuboBase("4 —w‹s"));
		AddEntry(new EntryBase(54,"»¡Òá‰Å¯±",6,huboman));
	}

}

PersonRecord::~PersonRecord()
{}

void PersonRecord::InitData()
{
	RecordBase::InitData();
	ObserveIndex=-1;
}


RecordBase *PersonRecord::CreateEqualRecord()
{
	RecordBase *rec= new PersonRecord();
	if(rec) rec->Assign(this);
	return rec;
}

BOOL PersonRecord::Assign(RecordBase *rec)
{
	if(RecordBase::Assign(rec)){
		ObserveIndex=((PersonRecord *)rec)->ObserveIndex;
		return TRUE;
	}
	return FALSE;
}

void PersonRecord::Get(FILE *fp)
{
	RecordBase::Get(fp);
	fread(&ObserveIndex,sizeof(ObserveIndex),1,fp);
}

void PersonRecord::Put(FILE *fp)
{
	RecordBase::Put(fp);
	fwrite(&ObserveIndex,sizeof(ObserveIndex),1,fp);
}

uint16 PersonRecord::GetFileSize()
{
	uint16 total=RecordBase::GetFileSize();
	return total+sizeof(ObserveIndex);
}


//========================================================

uint16 PersonRecord::GetBudae(char *str)
{
	char Buffer[10];
	if(!str){
		EntryBase *ent;
		ent=GetEntry(2);
		if(ent){
			strncpy(Buffer,ent->Data,ent->BufferSize);
			Buffer[ent->BufferSize]=0;
		}
	}
	else{
		strncpy(Buffer,str,10);
		Buffer[10]=0;
	}
	strupr(Buffer);
	::DeleteWhiteSpace(Buffer);

	uint16 budae=0;
	uint16 len=strlen(BudaeName);
	if(!strncmp(Buffer,BudaeName,len)){
		if(!strcmp(Buffer+len,"-HQ")) budae=0;
		else if(!strcmp(Buffer+len,"-A")) budae=1;
		else if(!strcmp(Buffer+len,"-B")) budae=2;
		else if(!strcmp(Buffer+len,"-C")) budae=3;
	}
	return budae;
}

BOOL PersonRecord::IsGanbu()
{
	EntryBase *ent = GetEntry(1);
	if(ent&&ent->HuboMan){
		int16 context=ent->HuboMan->GetIndexByText(ent->Data);
		if(context>=9) return TRUE;
	}
	return FALSE;
}

BOOL PersonRecord::IsHasa()
{
	EntryBase *ent = GetEntry(1);
	if(ent&&ent->HuboMan){
		int16 context=ent->HuboMan->GetIndexByText(ent->Data);
		if(context>=4&&context<9) return TRUE;
	}
	return FALSE;
}

BOOL PersonRecord::IsSabyong()
{
	EntryBase *ent = GetEntry(1);
	if(ent&&ent->HuboMan){
		int16 context=ent->HuboMan->GetIndexByText(ent->Data);
		if(context<4) return TRUE;
	}
	return FALSE;
}

BOOL PersonRecord::GetBirthDay(struct date& birthday,BOOL IsSolar,int16 ViewYear)
{
	DateEntry *ent = (DateEntry *)GetEntry(9);
	if(ent){
		char result=ent->StrToDate(ent->Data,birthday);
		if(ViewYear>0) birthday.da_year=ViewYear;
		if(!result){
			SolLuCvt da;
			struct date tempday;
			BooleanEntry *boolent = (BooleanEntry *)GetEntry(8);
			if(boolent){
				if(boolent->DataBoolean&&IsSolar){
					if(da.LunaToSol(birthday,tempday)) {
						if(ViewYear>0&&tempday.da_year!=ViewYear){
							birthday.da_year--;
							if(!da.LunaToSol(birthday,tempday)) return FALSE;
						}
						birthday=tempday;
					}
					else return FALSE;
				}
				else if(!boolent->DataBoolean&&!IsSolar) {
					if(da.SolToLuna(birthday,tempday)){
						if(ViewYear>0&&tempday.da_year!=ViewYear){
							birthday.da_year++;
							if(!da.SolToLuna(birthday,tempday)) return FALSE;
						}
						birthday=tempday;
					}
					else return FALSE;
				}
				return TRUE;
			}
		}
		else return (result&0x08)? FALSE:TRUE;
	}
	return FALSE;
}

BOOL PersonRecord::GetJunYokDay(struct date& junyokday)
{
	DateEntry *ent = (DateEntry *)GetEntry(6);
	if(ent){
		char result=ent->StrToDate(ent->Data,junyokday);
		if(result&0x0e) return FALSE;
		junyokday.da_year+=2;
		junyokday.da_mon+=2;
		if(junyokday.da_mon>12){
			junyokday.da_year++;
			junyokday.da_mon-=12;
		}
		return TRUE;
	}
	return FALSE;
}

int16 PersonRecord::Compare(RecordBase *base,WORD entid)
{
	if(entid==9){
		if(IsEqual(base)){
			struct date thisbirth,otherbirth;
			if(((PersonRecord *)base)->GetBirthDay(otherbirth)){
				if(GetBirthDay(thisbirth,TRUE,otherbirth.da_year)){
					thisbirth.da_year=otherbirth.da_year;
					DateEntry thisent(9,"",&thisbirth),otherent(9,"",&otherbirth);
					return thisent.Compare(&otherent);
				}
			}
		}
		return 2;
	}
	return RecordBase::Compare(base,entid);
}
