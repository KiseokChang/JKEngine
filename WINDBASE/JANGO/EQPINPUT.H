#ifndef _EQPINPUT_H
	#define _EQPINPUT_H

#ifndef _BOMB_
	#define _BOMB_
		typedef struct {
			char Name[9];
			char Kind[9];
			char Dod[11];
			char Lot[22];
			char Born[11];
			char Place[15];
			char Count[5];
			char Use[5];
			char HaveYear[11];
			char SomoYear[11];
		}BOMB;
#endif

#define ID_SAVEFINISH USERMESSAGE+11
#define ID_SAVE       USERMESSAGE+12

#include "jkwindow.h"
#include "combobox.h"
#include "eqfilemn.h"

class EqpInput:public JKWindow
{
public:
	EqpInput(WORD ctrlid,JKControl *parent=0);
	~EqpInput();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	BOOL IsEmpty();
	void GetValue();
	void SetValue(int16 index);
	void ClearAllText();
	void CheckBomb();
	void PlaceAdd(Combobox &com);
	int16  PlaceWrite();

	void EvShowWindow();
	void EvCharDown(uint16 key,WORD modifier);
	void EvEditFinish(WORD /*ctrlid*/);
	void EvButtonClick(WORD ctrlid);

	BOMB Bomb;
	BOOL MODIFY;

};
#endif

