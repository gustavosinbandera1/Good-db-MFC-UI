#include "pch.h"
#include "Goods-Database-MFC.h"
#include "CAddress.h"
#include "afxdialogex.h"
#include "usersView.h"
#include "GoodsDbDoc.h"
// CAddress dialog

IMPLEMENT_DYNAMIC(CAddressView, CDialog)

CAddressView::CAddressView(CWnd *pParent)
    : CDialog(IDD_DIALOG_ADD_ADDRESS, pParent) {
  m_parent = dynamic_cast<UsersView *>(pParent);
}

CAddressView::~CAddressView() {}

void CAddressView::DoDataExchange(CDataExchange *pDX) {
  CDialog::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_COMBO_COUNTRY, combo_country);
  DDX_Control(pDX, IDC_COMBO_CITY, combo_city);
  DDX_Control(pDX, IDC_COMBO_STATE, combo_state);
  DDX_Control(pDX, IDC_COMBO_TYPE, combo_type);
}

BEGIN_MESSAGE_MAP(CAddressView, CDialog)
ON_CBN_SELCHANGE(IDC_COMBO_COUNTRY, &CAddressView::OnCountryChange)
ON_CBN_SELCHANGE(IDC_COMBO_CITY, &CAddressView::OnCityChange)
ON_CBN_SELCHANGE(IDC_COMBO_STATE, &CAddressView::OnStateChange)
ON_CBN_SELCHANGE(IDC_COMBO_TYPE, &CAddressView::OnTypeChange)
ON_BN_CLICKED(IDOK, &CAddressView::OnBnClickedOk)
END_MESSAGE_MAP()

// CAddress message handlers

void CAddressView::OnCountryChange() {
  // TODO: Add your control notification handler code here
}

void CAddressView::OnCityChange() {
  // TODO: Add your control notification handler code here
}

void CAddressView::OnStateChange() {
  // TODO: Add your control notification handler code here
}

void CAddressView::OnTypeChange() {
  // TODO: Add your control notification handler code here
}

BOOL CAddressView::OnInitDialog() {
  CDialog::OnInitDialog();
  populateOptions();
  return TRUE; // return TRUE unless you set the focus to a control
               // EXCEPTION: OCX Property Pages should return FALSE
}

void CAddressView::populateOptions() {
  CString str;
  for (int i = 0; i < 10; i++) {
    str.Format(_T("Country %d"), i);
    combo_country.AddString(str);
  }

  for (int i = 0; i < 10; i++) {
    str.Format(_T("City %d"), i);
    combo_city.AddString(str);
  }

  for (int i = 0; i < 10; i++) {
    str.Format(_T("State %d"), i);
    combo_state.AddString(str);
  }

  for (int i = 0; i < 3; i++) {
    str.Format(_T("type %d"), i);
    combo_type.AddString(str);
  }
}

void CAddressView::OnBnClickedOk() {
  // TODO: Add your control notification handler code here
  // CDialog::OnOK();
  UpdateData(TRUE);
  CGoodsDbDoc *pDoc = m_parent->GetDocument();
  try {

    pDoc->addUser(CString("h"), CString("h"), CString("h"));
    // EndDialog(IDOK);
    m_parent->Invalidate();
    m_parent->UpdateWindow();
  } catch (CString e) {
    AfxMessageBox(e);
  }
}

void CAddressView::PostNcDestroy() {
  // TODO: Add your specialized code here and/or call the base class
  CDialog::PostNcDestroy();
  if (m_parent) {
    m_parent->addressViewDeleted(this);
  }
}
