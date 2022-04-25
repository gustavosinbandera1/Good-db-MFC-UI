// userView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "GoodsDbDoc.h"
#include "userView.h"
#include "afxdialogex.h"
#include "usersView.h"

// UserView dialog

IMPLEMENT_DYNAMIC(UserView, CDialogEx)

UserView::UserView(CWnd *pParent )
    : CDialogEx(IDD_ADD_EDIT_USER, pParent), name(_T("")), email(_T("")),
      password(_T("")), passwordR(_T("")) {
	m_parent = dynamic_cast<UsersView*>(pParent);
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
  DDX_Text(pDX, IDC_PASSWORD_R, passwordR);
  DDV_MaxChars(pDX, passwordR, 30);
}


BEGIN_MESSAGE_MAP(UserView, CDialogEx)
	ON_BN_CLICKED(IDOK, &UserView::OnBnClickedOk)
END_MESSAGE_MAP()

// UserView message handlers


void UserView::OnBnClickedOk()
{
	UpdateData(TRUE);
	try {
		m_parent->GetDocument()->addUser(this->name, this->email, this->password);
		AfxMessageBox(CString("Saved here"));
		EndDialog(IDOK);

		m_parent->Invalidate();
		m_parent->UpdateWindow();
	}
	catch (CString e) {
		AfxMessageBox(e);
	}
}

void UserView::PostNcDestroy() {
	CDialogEx::PostNcDestroy();
	if (m_parent) {
		m_parent->userViewDeleted(this);
	}
}
