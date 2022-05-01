#include "pch.h"
#include "Goods-Database-MFC.h"
#include "GoodsDbDoc.h"
#include "userView.h"
#include "afxdialogex.h"
#include "usersView.h"

// UserView dialog

IMPLEMENT_DYNAMIC(UserView, CDialogEx)

UserView::UserView(bool read_only, CWnd *pParent)
    : CDialogEx(IDD_ADD_EDIT_USER, pParent)
	, name(_T(""))
	, email(_T(""))
	, password(_T(""))
	, readOnly(read_only) {
	m_parent = dynamic_cast<UsersView *>(pParent);
}

UserView::~UserView() {}

void UserView::DoDataExchange(CDataExchange *pDX) {
  CDialogEx::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_NAME, name);
  DDV_MaxChars(pDX, name, 30);
  DDX_Text(pDX, IDC_EMAIL, email);
  DDV_MaxChars(pDX, email, 30);
  DDX_Text(pDX, IDC_PASSWORD, password);
  DDV_MaxChars(pDX, password, 30);
}

BEGIN_MESSAGE_MAP(UserView, CDialogEx)
ON_BN_CLICKED(IDOK, &UserView::OnBnClickedOk)
END_MESSAGE_MAP()

// UserView message handlers

void UserView::OnBnClickedOk() {
  UpdateData(TRUE);
  try {
    m_parent->GetDocument()->addUser(this->name, this->email, this->password);
    //AfxMessageBox(CString("Saved here"));
    EndDialog(IDOK);

    m_parent->Invalidate();
    m_parent->UpdateWindow();
  } catch (CString e) {
    AfxMessageBox(e);
  }
}

void UserView::PostNcDestroy() {
  CDialogEx::PostNcDestroy();
  if (m_parent) {
    m_parent->userViewDeleted(this);
  }
}

BOOL UserView::OnInitDialog() {
  CDialogEx::OnInitDialog();
  if (readOnly == true) {
    CEdit *Ce = (CEdit *)(this->GetDlgItem(IDC_NAME));
    Ce->SetReadOnly(TRUE);

    Ce = (CEdit *)(this->GetDlgItem(IDC_EMAIL));
    Ce->SetReadOnly(TRUE);

    Ce = (CEdit *)(this->GetDlgItem(IDC_PASSWORD));
    Ce->SetReadOnly(TRUE);

    Ce = (CEdit *)(this->GetDlgItem(IDC_PASSWORD_R));
    Ce->SetReadOnly(TRUE);
  }

  AfxMessageBox(CString("Init Dialog .............. "));
  // TODO:  Add extra initialization here



  return TRUE; // return TRUE unless you set the focus to a control
               // EXCEPTION: OCX Property Pages should return FALSE
}
