#include "pch.h"
#include "tabViewCtrlView.h"

#include "productsView.h"
#include "usersView.h"
#include "ordersView.h"
#include "newOrderView.h"

IMPLEMENT_DYNCREATE(CTabViewCtrlView, CTabView)

BEGIN_MESSAGE_MAP(CTabViewCtrlView, CTabView)
ON_WM_CREATE()
END_MESSAGE_MAP()

CTabViewCtrlView::CTabViewCtrlView() {}

CTabViewCtrlView::~CTabViewCtrlView() {}

#ifdef _DEBUG
void CTabViewCtrlView::AssertValid() const { CTabView::AssertValid(); }

#ifndef _WIN32_WCE
void CTabViewCtrlView::Dump(CDumpContext &dc) const { CTabView::Dump(dc); }
#endif
#endif // _DEBUG

int CTabViewCtrlView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CTabView::OnCreate(lpCreateStruct) == -1)
    return -1;

  CRect rect;
  rect.SetRectEmpty();

  CMFCTabCtrl tab_ctrl;
  tab_ctrl.DestroyWindow();

  // creates the tab control and attaches it to the CMFCTabCtrl object
  tab_ctrl.Create(CMFCTabCtrl::STYLE_3D_VS2005, rect, this, 1,
                  CMFCTabCtrl::LOCATION_TOP);
  // tab_ctrl.EnableTabSwap(FALSE);

  // attach a view to the document
  AddView(RUNTIME_CLASS(UsersView), _T(" Users "), 0);
  AddView(RUNTIME_CLASS(ProductsView), _T(" Products "), 1);
  AddView(RUNTIME_CLASS(OrdersView), _T(" Orders "), 2);
  AddView(RUNTIME_CLASS(NewOrderView), _T(" New Order "), 2);

  return 0;
}
