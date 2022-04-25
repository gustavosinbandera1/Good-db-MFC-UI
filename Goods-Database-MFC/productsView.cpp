// productsView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "productsView.h"
#include "usersView.h"
#include "productView.h"
#include "GoodsDbDoc.h"


// ProductsView

IMPLEMENT_DYNCREATE(ProductsView, CFormView)

ProductsView::ProductsView() : CFormView(IDD_FORM_PRODUCT) {}

ProductsView::~ProductsView() {}


void ProductsView::DoDataExchange(CDataExchange *pDX) {
  CFormView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(ProductsView, CFormView)
	ON_BN_CLICKED(IDC_ADD_PRODUCT, &ProductsView::OnBnClickedAddProduct)
END_MESSAGE_MAP()

// ProductsView diagnostics

#ifdef _DEBUG
void ProductsView::AssertValid() const { CFormView::AssertValid(); }

#ifndef _WIN32_WCE
void ProductsView::Dump(CDumpContext &dc) const { CFormView::Dump(dc); }
#endif
#endif //_DEBUG


void ProductsView::productViewDeleted(ProductView * productview) {
	if (m_productView && (m_productView.get() == productview)) {
		m_productView.reset(nullptr);
	}
}


CGoodsDbDoc * ProductsView::GetDocument() const {
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGoodsDbDoc)));
	return (CGoodsDbDoc *)m_pDocument;
}

void ProductsView::OnBnClickedAddProduct() {
	CGoodsDbDoc *pDoc = GetDocument();
	if (pDoc != NULL) {
		m_productView = std::make_unique<ProductView>(this);

		/*FIRST WAY*/
		if (m_productView->DoModal() == IDOK) {
			AfxMessageBox(CString("do modal ok"));
		}

		/*SECOND WAY !! MODAL LESS*/
		//m_productView->Create(IDD_FORM_PRODUCT, this);
		//m_productView->ShowWindow(SW_SHOW);
		AfxMessageBox(CString("There isn' -------------- "));
	}
	else {
		AfxMessageBox(CString("There isn't document to perform the task "));
	}
}
