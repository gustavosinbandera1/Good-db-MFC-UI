// usersView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "usersView.h"
#include "userView.h"
#include "GoodsDbDoc.h"

// UsersView

IMPLEMENT_DYNCREATE(UsersView, CFormView)

UsersView::UsersView() : CFormView(IDD_FORM_USER) {}

UsersView::~UsersView() {}

void UsersView::DoDataExchange(CDataExchange *pDX) {
  CFormView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(UsersView, CFormView)
	ON_BN_CLICKED(IDC_ADD_USER, &UsersView::OnBnClickedAddUser)
END_MESSAGE_MAP()

// UsersView diagnostics

#ifdef _DEBUG
void UsersView::AssertValid() const { CFormView::AssertValid(); }

#ifndef _WIN32_WCE
void UsersView::Dump(CDumpContext &dc) const { CFormView::Dump(dc); }
#endif
#endif //_DEBUG

// UsersView message handlers


// for reset the ptr to UserView dialog
void UsersView::userViewDeleted(UserView * usrview) {
	if(m_userView && (m_userView.get() == usrview)) {
		m_userView.reset(nullptr);
	}
}

void UsersView::OnBnClickedAddUser() {
	CGoodsDbDoc *pDoc = GetDocument();
	if (pDoc != NULL) {
		m_userView = std::make_unique<UserView>(this);
		
		/*FIRST WAY*/
		//if (m_userView->DoModal() == IDOK) {
		//	AfxMessageBox(CString("do modal ok"));
		//}

		/*SECOND WAY !! MODAL LESS*/
		m_userView->Create(IDD_ADD_EDIT_USER, this);
		m_userView->ShowWindow(SW_SHOW);
		
	} else {
		AfxMessageBox(CString("There isn't document to perform the task "));
	}
}

CGoodsDbDoc * UsersView::GetDocument() const {
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGoodsDbDoc)));
	return (CGoodsDbDoc *)m_pDocument;
}
