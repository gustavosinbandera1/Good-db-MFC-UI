#include "pch.h"
#include "TableView.h"

bool CTableView::IsDraw() { return true; }

bool CTableView::OnDraw(CDC *pDC) {
  // called for each item into the table
  // AfxMessageBox(CString("On draw called"));
  return false;
}
bool CTableView::OnDraw(CDC *pDC, const CRect &r) {
  CBrush brush(RGB(0, 0, 0)); // cyan
  pDC->FillRect(r, &brush);
  return false; // do default drawing as well
}

bool CTableView::IsNotifyItemDraw() { return true; }

bool CTableView::IsNotifySubItemDraw(int /*nItem*/, UINT /*nState*/,
                                     LPARAM /*lParam*/) {
  return true;
}

COLORREF CTableView::TextColorForSubItem(int nItem, int nSubItem,
                                         UINT /*nState*/, LPARAM /*lParam*/) {

  if (nItem == 0)
    return RGB(255, 0, 255);
  else if (nItem == 1)
    return RGB(0, 255, 0);
  else if (nItem == 2)
    return RGB(0, 0, 255);

  return CLR_DEFAULT;
}

COLORREF CTableView::BkColorForSubItem(int nItem, int nSubItem, UINT /*nState*/,
                                       LPARAM /*lParam*/) {
  if (nItem == 0)
    return RGB(0, 0, 255);
  else if (nItem == 1)
    return RGB(255, 255, 0);
  else if (nItem == 2)
    return RGB(0, 255, 255);
  return CLR_DEFAULT;
}

CFont *CTableView::FontForSubItem(int, int, UINT, LPARAM) {
  // CFont *m_font = nullptr;
  // m_font->CreateFont(10, 0, 0, 0, FW_BOLD, 0, 1, 0, 0, 0, 0, 0, 0,
  // _T("Arial")); return m_font;
  return nullptr;
}
