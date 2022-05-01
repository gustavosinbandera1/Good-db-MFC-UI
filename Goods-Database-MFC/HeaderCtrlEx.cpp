#include "pch.h"
#include "HeaderCtrlEx.h"

CHeaderCtrlEx::CHeaderCtrlEx() {

}

CHeaderCtrlEx::~CHeaderCtrlEx() {

}

BEGIN_MESSAGE_MAP(CHeaderCtrlEx, CHeaderCtrl)
END_MESSAGE_MAP()


void CHeaderCtrlEx::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{

   ASSERT(lpDrawItemStruct->CtlType == ODT_HEADER);

   HDITEM hdi;
   TCHAR  lpBuffer[256];

   hdi.mask = HDI_TEXT;
   hdi.pszText = lpBuffer;
   hdi.cchTextMax = 256;

   GetItem(lpDrawItemStruct->itemID, &hdi);

   	
	CDC* pDC;
	pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

	//THIS FONT IS ONLY FOR DRAWING AS LONG AS WE DON'T DO A SetFont(...)
	pDC->SelectObject(GetStockObject(SYSTEM_FIXED_FONT));
   // Draw the button frame.
   ::DrawFrameControl(lpDrawItemStruct->hDC, 
      &lpDrawItemStruct->rcItem, DFC_BUTTON, DFCS_BUTTONPUSH);

	UINT uFormat = DT_CENTER;
	//DRAW THE TEXT
   ::DrawText(lpDrawItemStruct->hDC, lpBuffer, CString(lpBuffer).GetLength() , 
      &lpDrawItemStruct->rcItem, uFormat);

   pDC->SelectStockObject(SYSTEM_FONT);

}

