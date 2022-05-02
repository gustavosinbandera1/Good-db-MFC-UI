#include "pch.h"
#include "ListCtrlEx.h"
#include "resource.h"
#include "EditInPlace.h"

BEGIN_MESSAGE_MAP(CListCtrlEx, CListCtrl)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CListCtrlEx::OnNMCustomdraw)
	// ON_WM_LBUTTONDOWN()
	ON_NOTIFY_REFLECT(NM_DBLCLK, &CListCtrlEx::OnNMDblclk)
END_MESSAGE_MAP()

CListCtrlEx::CListCtrlEx() {}

CListCtrlEx::~CListCtrlEx() {}

void CListCtrlEx::setHeaders(std::vector<CString> &headers) {
  // for (auto & header : headers) {
  //}

  int index;
  for (std::vector<CString>::iterator it = headers.begin(); it != headers.end();
       ++it) {
    index = it - headers.begin();
    InsertColumn(index, it->GetString(), LVCFMT_LEFT, -1, index);
  }

  for (int i = 0; i <= index; i++) {
    SetColumnWidth(i, LVSCW_AUTOSIZE_USEHEADER);
  }
}

void CListCtrlEx::OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult) {
  /*pointer to customDraw object*/
  LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
  /*pointer to struct containing listview objects and customDraw object*/
  LPNMLVCUSTOMDRAW pNMLVCUSTOMDRAW = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);

  //// we'll copy the device context into hdc
  //// but won't convert it to a pDC* until (and if)
  //// we need it as this requires a bit of work
  //// internally for MFC to create temporary CDC
  //// objects
  HDC hdc = pNMLVCUSTOMDRAW->nmcd.hdc;
  CDC *pDC = NULL;

  //// here is the item info
  //// note that we don't get the subitem
  //// number here, as this may not be
  //// valid data except when we are
  //// handling a sub item notification
  //// so we'll do that separately in
  //// the appropriate case statements
  //// below.
  int nItem = pNMLVCUSTOMDRAW->nmcd.dwItemSpec;
  UINT nState = pNMLVCUSTOMDRAW->nmcd.uItemState;
  LPARAM lParam = pNMLVCUSTOMDRAW->nmcd.lItemlParam;
  auto drawStage = pNMLVCUSTOMDRAW->nmcd.dwDrawStage;

  // next we set up flags that will control
  // the return value for *pResult
  bool bNotifyPostPaint = false;
  bool bNotifyItemDraw = false;
  bool bNotifySubItemDraw = false;
  bool bSkipDefault = false;
  bool bNewFont = false;

  switch (drawStage) {
  case CDDS_PREPAINT: {
    // pDC = CDC::FromHandle(pNMCD->hdc);
    // CRect rect(200, 200, 300, 300);

    // GetClientRect(&rect);
    // pDC->FillSolidRect(&rect, RGB(255, 0, 0));

    //*pResult = CDRF_NOTIFYITEMDRAW;

    if (IsDraw()) {
      OnDraw(pDC);
    }
  }

  case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {

    // AfxMessageBox(CString("CDDS_ITEMPREPAINT | CDDS_SUBITEM: "));
    // Sub Item PrePaint
    // set sub item number (data will be valid now)
    int nSubItem = pNMLVCUSTOMDRAW->iSubItem;

    m_pOldSubItemFont = NULL;

    bNotifyPostPaint =
        IsNotifySubItemPostPaint(nItem, nSubItem, nState, lParam);

    // set up the colors to use
    pNMLVCUSTOMDRAW->clrText =
        TextColorForSubItem(nItem, nSubItem, nState, lParam);

    pNMLVCUSTOMDRAW->clrTextBk =
        BkColorForSubItem(nItem, nSubItem, nState, lParam);

    // set up a different font to use, if any
    CFont *pNewFont = FontForSubItem(nItem, nSubItem, nState, lParam);

    if (pNewFont) {
      if (!pDC)
        pDC = CDC::FromHandle(hdc);
      m_pOldSubItemFont = pDC->SelectObject(pNewFont);

      bNotifyPostPaint = true; // need to restore font
    }

    // do we want to draw the item ourselves?
    if (IsSubItemDraw(nItem, nSubItem, nState, lParam)) {
      if (!pDC)
        pDC = CDC::FromHandle(hdc);
      if (OnSubItemDraw(pDC, nItem, nSubItem, nState, lParam)) {

        // we drew it all ourselves
        // so don't do default
        bSkipDefault = true;
      }
    }
  }

  break;

  case CDDS_ITEMPOSTPAINT | CDDS_SUBITEM: {
    // Sub Item PostPaint
    // set sub item number (data will be valid now)
    int nSubItem = pNMLVCUSTOMDRAW->iSubItem;

    // restore old font if any
    if (m_pOldSubItemFont) {
      if (!pDC)
        pDC = CDC::FromHandle(hdc);
      pDC->SelectObject(m_pOldSubItemFont);

      m_pOldSubItemFont = NULL;
    }

    // do we want to do any extra drawing?
    if (IsSubItemPostDraw()) {
      if (!pDC)
        pDC = CDC::FromHandle(hdc);
      OnSubItemPostDraw(pDC, nItem, nSubItem, nState, lParam);
    }
  } break;

  case CDDS_ITEMPOSTPAINT: {
    // Item PostPaint
    // restore old font if any
    if (m_pOldItemFont) {
      if (!pDC)
        pDC = CDC::FromHandle(hdc);
      pDC->SelectObject(m_pOldItemFont);
      m_pOldItemFont = NULL;
    }

    // do we want to do any extra drawing?
    if (IsItemPostDraw()) {
      if (!pDC)
        pDC = CDC::FromHandle(hdc);
      OnItemPostDraw(pDC, nItem, nState, lParam);
    }
  } break;

  case CDDS_POSTPAINT: {
    // Item PostPaint
    // do we want to do any extra drawing?
    if (IsPostDraw()) {
      if (!pDC)
        pDC = CDC::FromHandle(hdc);
      CRect r(pNMLVCUSTOMDRAW->nmcd.rc);

      OnPostDraw(pDC);
    }
  } break;

  default:
    break;
  }

  *pResult = 0;
  *pResult |= CDRF_NOTIFYPOSTPAINT;
  *pResult |= CDRF_NOTIFYITEMDRAW;
  *pResult |= CDRF_NOTIFYSUBITEMDRAW;
}

int CListCtrlEx::GetRowFromPoint(CPoint &point, int *col) const {
  int column = 0;
  int row = HitTest(point, NULL);

  if (col)
    *col = 0;

  // Make sure that the ListView is in LVS_REPORT
  if ((GetWindowLong(m_hWnd, GWL_STYLE) & LVS_TYPEMASK) != LVS_REPORT) {
    return row;
  }

  // Get the top and bottom row visible
  row = GetTopIndex();
  int bottom = row + GetCountPerPage();

  if (bottom > GetItemCount()) {
    bottom = GetItemCount();
  }

  // Get the number of columns
  // CHeaderCtrl *pHeader = (CHeaderCtrl *)GetDlgItem(0);
  CHeaderCtrl *pHeader = GetHeaderCtrl();
  int nColumnCount = pHeader->GetItemCount();

  // Loop through the visible rows
  for (; row <= bottom; row++) {
    // Get bounding rectangle of item and check whether point falls in it.
    CRect rect;
    GetItemRect(row, &rect, LVIR_BOUNDS);

    if (rect.PtInRect(point)) {
      // Find the column
      for (column = 0; column < nColumnCount; column++) {
        int colwidth = GetColumnWidth(column);

        if (point.x >= rect.left && point.x <= (rect.left + colwidth)) {
          if (col)
            *col = column;
          return row;
        }

        rect.left += colwidth;
      }
    }
  }

  return -1;
}

CEdit *CListCtrlEx::EditSubLabel(int nItem, int nCol) {
  // The returned pointer should not be saved, make sure item visible
  if (!EnsureVisible(nItem, TRUE))
    return NULL;

  // Make sure that column number is valid
  /*CHeaderCtrl *pHeader = (CHeaderCtrl *)GetDlgItem(0);*/
  CHeaderCtrl *pHeader = GetHeaderCtrl();
  int nColumnCount = pHeader->GetItemCount();
  if (nCol >= nColumnCount || GetColumnWidth(nCol) < 5)
    return NULL;

  // Get the column offset
  int offset = 0;
  for (int i = 0; i < nCol; i++) {
    offset += GetColumnWidth(i);
  }

  CRect rect;
  GetItemRect(nItem, &rect, LVIR_BOUNDS);

  // Scroll horizontally if we need to expose the column
  CRect rcClient;
  GetClientRect(&rcClient);

  if (offset + rect.left < 0 || offset + rect.left > rcClient.right) {
    CSize size;
    size.cx = offset + rect.left;
    size.cy = 0;
    Scroll(size);
    rect.left -= size.cx;
  }

  // Get Column alignment
  LV_COLUMN lvcol;
  lvcol.mask = LVCF_FMT;
  GetColumn(nCol, &lvcol);
  DWORD dwStyle;

  if ((lvcol.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_LEFT) {
    dwStyle = ES_LEFT;
  } else if ((lvcol.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_RIGHT) {
    dwStyle = ES_RIGHT;
  } else {
    dwStyle = ES_CENTER;
  }
  // dwStyle = ES_LEFT;
  rect.left += offset + 4;
  rect.right = rect.left + GetColumnWidth(nCol) - 3;

  if (rect.right > rcClient.right) {
    rect.right = rcClient.right;
  }

  dwStyle |= WS_BORDER | WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;

  CEdit *pEdit = new EditInPlace(nItem, nCol, GetItemText(nItem, nCol));
  // pEdit->Create(dwStyle, rect, this, IDC_LIST_USERS);
  pEdit->Create(dwStyle, rect, this, NULL);
  return pEdit;
}

void CListCtrlEx::OnEndLabelEdit(WPARAM wParam, LPARAM lParam) {
  LV_DISPINFO *dispinfo = reinterpret_cast<LV_DISPINFO *>(lParam);
  //// Persist the selected attachment details upon updating its text
  this->SetItemText(dispinfo->item.iItem, dispinfo->item.iSubItem,
                    dispinfo->item.pszText);
}

void CListCtrlEx::OnLButtonDown(UINT nFlags, CPoint point) {
  CListCtrl::OnLButtonDown(nFlags, point);
  int colnum;
  int index = GetRowFromPoint(point, &colnum);
  if (index != -1) {
    EditSubLabel(index, colnum);
  }
}

void CListCtrlEx::OnNMDblclk(NMHDR *pNMHDR, LRESULT *pResult) {
  // AfxMessageBox(CString("DBle click"));
  LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
  // TODO: Add your control notification handler code here
  if (pNMItemActivate->iItem != -1)
    EditSubLabel(pNMItemActivate->iItem, pNMItemActivate->iSubItem);
  *pResult = 0;
}
