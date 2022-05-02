#include "pch.h"
#include "TableView.h"

bool CTableCtrl::IsDraw() { return true; }

bool CTableCtrl::OnDraw(CDC *pDC) {
  // called for each item into the table
  // AfxMessageBox(CString("On draw called"));
  return false;
}

bool CTableCtrl::OnDraw(CDC *pDC, const CRect &r) {
  CBrush brush(RGB(0, 0, 0)); // cyan
  pDC->FillRect(r, &brush);
  return false; // do default drawing as well
}

bool CTableCtrl::IsNotifyItemDraw() { return true; }

bool CTableCtrl::IsNotifySubItemDraw(int /*nItem*/, UINT /*nState*/,
                                     LPARAM /*lParam*/) {
  return true;
}

COLORREF CTableCtrl::TextColorForSubItem(int nItem, int nSubItem,
                                         UINT /*nState*/, LPARAM /*lParam*/) {

  if (nItem % 2 ==  0)
    return RGB(255, 255, 255);
  else 
	  return RGB(255, 255, 255);

  return CLR_DEFAULT;
}

COLORREF CTableCtrl::BkColorForSubItem(int nItem, int nSubItem, UINT /*nState*/,
                                       LPARAM /*lParam*/) {
	if (nItem % 2 == 0)
		return RGB(31, 13, 222);
	else
		return RGB(222, 13, 13);
}

CFont *CTableCtrl::FontForSubItem(int, int, UINT, LPARAM) {
  // CFont *m_font = nullptr;
  // m_font->CreateFont(10, 0, 0, 0, FW_BOLD, 0, 1, 0, 0, 0, 0, 0, 0,
  // _T("Arial")); return m_font;
  return nullptr;
}
