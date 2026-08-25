#ifndef _STATVIEW_H
#define _STATVIEW_H

#include "real2pix.h"
#include "mt_plln.h"
#include "mt_char.h"
#include "calccord.h"
#include "statmeta.h"
#include "memdc.h"
#include "jkwindow.h"

class StatusView:public JKWindow
{
public:
	StatusView(JKRect& rect,JKControl *parent=0);
 ~StatusView();

	JKRect CalcClientRect();

	void InitWindow();
	void SetupWindow();
	void EvShowWindow();

	void PaintWindow(JKDC& dc,JKRect& paintrect);
	void PaintClient(JKDC& dc,JKRect&);

	void EvCtrlOnlyMove(WORD ctrlid);
	void EvCtrlLeftClick(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvKeyDown(WORD key,WORD);
	void EvOnlyMove(JKPoint&);
	void EvNcOnlyMove(JKPoint&);
	void EvElOnlyMove(WORD);

	BOOL GetRealSquareSize(JKPoint32& size);

	void EvLeftClick(JKPoint& p);
	void EvLeftDragging(JKPoint& p);
	void EvLeftRelease(JKPoint& p);
	void EvNcLeftRelease(JKPoint& p);

	void EvRightClick(JKPoint& p);
	void EvRightDragging(JKPoint& p);
	void EvRightRelease(JKPoint& p);
	void EvNcRightRelease(JKPoint& p);

	void EvButtonClick(WORD ctrlid);
	void EvEditFinish(WORD ctrlid);

	void EvCommand(WORD cmdid,WORD data,DWORD detail,WORD option);

	BOOL AdjustRealRect(JKRect32& rect);
	void SetClientRealRect(JKRect32 rect);
	void SetZoomOut(JKPoint32& p);


	BOOL GetCheck(uint16 i,BOOL& check);
  BOOL SetCheck(uint16 i,BOOL check);
	int16 GetTargetResKind(WORD resid);
	int16 GetBattalionResKind(WORD resid);
	void DrawBlock(JKDC& dc,JKPoint32 blocksize);
	void DrawBase(JKDC& dc,JKRect32 paintrect=JKRect32(0,0,-1,-1));
	void DrawStatMeta(JKDC& dc,JKRect32 paintrect=JKRect32(0,0,-1,-1));
	void DrawBattalion(JKDC& dc);
	void DrawXBattalion(JKDC& dc);
	void DrawTarget(JKDC& dc);
	void DrawAirControl(JKDC& dc,JKRect32 paintrect=JKRect32(0,0,-1,-1));

	void MakeBaseImage();
	void MakeStatImage();

	void EvChangeBase();
	void EvChangeStat();

	void EvLBSelectEntry(WORD ctrlid,uint16 index);

	void EvCheckToggle(WORD ctrlid,BOOL status);

	void ShowExplain(WORD ctrlid);
	void HideExplain();

  void SendCommand(char *message,WORD command);

	void AdjustZoomRatio();

	JKMemoryDC *BaseMemDC;
	JKMemoryDC *StatMemDC;

	CoordCalculator CoordCalc;
	RealCoordManager RealCoordMan;
	JKPoint32 BlockSize;
	JKPoint32 MoveInterval;

	StatViewMeta PaintStatMeta;
	MetaPolyLine MtPolyLine;
	MetaTextOut  MtText;

	BOOL IsSVEditing;
	JKPoint32 WorkingPoint;

	WORD WorkKind;
	BOOL Working;
	JKPoint WorkingStart,WorkingEnd;


  int32 MessageIndex;
};
#endif

