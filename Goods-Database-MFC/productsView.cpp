// productsView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "productsView.h"

// ProductsView

IMPLEMENT_DYNCREATE(ProductsView, CFormView)

ProductsView::ProductsView() : CFormView(IDD_FORM_PRODUCT) {}

ProductsView::~ProductsView() {}

void ProductsView::DoDataExchange(CDataExchange *pDX) {
  CFormView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(ProductsView, CFormView)
END_MESSAGE_MAP()

// ProductsView diagnostics

#ifdef _DEBUG
void ProductsView::AssertValid() const { CFormView::AssertValid(); }

#ifndef _WIN32_WCE
void ProductsView::Dump(CDumpContext &dc) const { CFormView::Dump(dc); }
#endif
#endif //_DEBUG

// ProductsView message handlers
