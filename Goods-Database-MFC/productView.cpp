// productView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "GoodsDbDoc.h"
#include "productView.h"
#include "productsView.h"
#include "afxdialogex.h"
//#include "usersView.h"

// ProductView dialog

IMPLEMENT_DYNAMIC(ProductView, CDialogEx)

ProductView::ProductView(bool read_only, CWnd *pParent)
    : CDialogEx(IDD_ADD_EDIT_PRODUCT, pParent)
	, m_sku(0)
	, m_description(_T(""))
	, m_price(0)
	, m_weight(0)
	, readOnly(read_only) {
  m_parent = dynamic_cast<ProductsView *>(pParent);
}

ProductView::~ProductView() {
}

void ProductView::DoDataExchange(CDataExchange *pDX) {
  CDialogEx::DoDataExchange(pDX);

  DDX_Text(pDX, IDC_SKU, m_sku);
  DDV_MinMaxInt(pDX, m_sku, 0, 999999);
  DDX_Text(pDX, IDC_DESCRIPTION, m_description);
  DDV_MaxChars(pDX, m_description, 30);
  DDX_Text(pDX, IDC_PRICE, m_price);
  DDV_MinMaxDouble(pDX, m_price, 0, 9999999.9);
  DDX_Text(pDX, IDC_WEIGHT, m_weight);
  DDV_MinMaxDouble(pDX, m_weight, 0, 999999.9);
}

void ProductView::PostNcDestroy() {
  CDialogEx::PostNcDestroy();
  if (m_parent) {
    m_parent->productViewDeleted(this);
  }
}

BEGIN_MESSAGE_MAP(ProductView, CDialogEx)
ON_BN_CLICKED(IDOK, &ProductView::OnBnClickedOk)
END_MESSAGE_MAP()

// ProductView message handlers

void ProductView::OnBnClickedOk() {
  // TODO: Add your control notification handler code here
  UpdateData(TRUE);
  try {
    m_parent->GetDocument()->addProduct(m_description, m_price, m_weight);
    //AfxMessageBox(CString("Saved here"));
    EndDialog(IDOK);

    m_parent->Invalidate();
    m_parent->UpdateWindow();
  } catch (CString e) {
    AfxMessageBox(e);
  }

  CDialogEx::OnOK();
}


BOOL ProductView::OnInitDialog() {
	CDialogEx::OnInitDialog();
	if (readOnly) {
		CEdit *Ce = (CEdit *)(this->GetDlgItem(IDC_SKU));
		Ce->SetReadOnly(TRUE);

		Ce = (CEdit *)(this->GetDlgItem(IDC_DESCRIPTION));
		Ce->SetReadOnly(TRUE);

		Ce = (CEdit *)(this->GetDlgItem(IDC_PRICE));
		Ce->SetReadOnly(TRUE);

		Ce = (CEdit *)(this->GetDlgItem(IDC_WEIGHT));
		Ce->SetReadOnly(TRUE);
	}
	// TODO:  Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}
