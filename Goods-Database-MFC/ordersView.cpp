// ordersView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "ordersView.h"

// OrdersView

IMPLEMENT_DYNCREATE(OrdersView, CFormView)

OrdersView::OrdersView() : CFormView(IDD_FORM_ORDER) {}

OrdersView::~OrdersView() {}

void OrdersView::DoDataExchange(CDataExchange *pDX) {
  CFormView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(OrdersView, CFormView)
END_MESSAGE_MAP()

// OrdersView diagnostics

#ifdef _DEBUG
void OrdersView::AssertValid() const { CFormView::AssertValid(); }

#ifndef _WIN32_WCE
void OrdersView::Dump(CDumpContext &dc) const { CFormView::Dump(dc); }
#endif
#endif //_DEBUG

// OrdersView message handlers
