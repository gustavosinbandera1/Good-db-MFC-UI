// newOrderView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "newOrderView.h"


// NewOrderView

IMPLEMENT_DYNCREATE(NewOrderView, CFormView)

NewOrderView::NewOrderView() : CFormView(IDD_FORM_NEW_ORDER) {}

NewOrderView::~NewOrderView() {}

void NewOrderView::DoDataExchange(CDataExchange *pDX) {
  CFormView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(NewOrderView, CFormView)
END_MESSAGE_MAP()

// NewOrderView diagnostics

#ifdef _DEBUG
void NewOrderView::AssertValid() const { CFormView::AssertValid(); }

#ifndef _WIN32_WCE
void NewOrderView::Dump(CDumpContext &dc) const { CFormView::Dump(dc); }
#endif
#endif //_DEBUG

// NewOrderView message handlers
