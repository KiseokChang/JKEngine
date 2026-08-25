#include "listbox.h"

class EqpKind:public JKListBox
{
public:
	EqpKind(JKRect& rect,WORD ctlid,JKControl *parent);
	~EqpKind();
	void InitEquip();
	void SetupWindow();
	void InitPopupList();

	void SendControlMessage(WORD main,WORD data=0,DWORD detail=0,WORD option=0);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);

public:
	char Kind[9];
};

