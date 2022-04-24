// usersView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "usersView.h"

// UsersView

IMPLEMENT_DYNCREATE(UsersView, CFormView)

UsersView::UsersView() : CFormView(IDD_FORM_USER) {}

UsersView::~UsersView() {}

void UsersView::DoDataExchange(CDataExchange *pDX) {
  CFormView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(UsersView, CFormView)
END_MESSAGE_MAP()

// UsersView diagnostics

#ifdef _DEBUG
void UsersView::AssertValid() const { CFormView::AssertValid(); }

#ifndef _WIN32_WCE
void UsersView::Dump(CDumpContext &dc) const { CFormView::Dump(dc); }
#endif
#endif //_DEBUG

// UsersView message handlers
