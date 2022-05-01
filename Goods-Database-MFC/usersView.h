#pragma once
// UsersView form view
#include <memory>
#include "MultilineList.h"
#include "ListCtrlEx.h"

#include "TableView.h"

class CGoodsDbDoc;
class UserView;

extern char const* const USER_HEADER_STRING[];

class UsersView : public CFormView
{
	DECLARE_DYNCREATE(UsersView)

private:
	/*FormView for add or edit users*/
	std::unique_ptr<UserView> m_userView;
	CMultilineList m_table;
public:
	CTableView m_usersTable;
	bool m_fClickedList;

protected:

	UsersView();           // protected constructor used by dynamic creation
	virtual ~UsersView();

public:
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FORM_USER };
#endif
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif

protected:
	BOOL m_initialized;
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	
	CFont m_NewHeaderFont;
	CHeaderCtrlEx m_HeaderCtrl;

	DECLARE_MESSAGE_MAP()
public:
	/*For releasing dialog memory and avoid memory leak*/
	void userViewDeleted(UserView *usrview);
	afx_msg void OnBnClickedAddUser();
	CGoodsDbDoc* GetDocument() const;
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();

	void populateTable();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
private:
	CString m_name;
	CString m_email;
	CString m_password;
	CString m_password_r;

protected:

public:
	afx_msg void OnLvnItemchangedListUsers(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg LRESULT OnNotifyDescriptionEdited(WPARAM, LPARAM);
	CPoint InterviewListCursorPosition() const;
	afx_msg void OnKeydownListUsers(NMHDR *pNMHDR, LRESULT *pResult);
};


