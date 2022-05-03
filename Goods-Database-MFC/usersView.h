#pragma once
#include <memory>
#include "CAddress.h"
#include "TableView.h"
//#include "CAddress.h"

class CGoodsDbDoc;
class CAddressView;
class UserView;

extern char const *const USER_HEADER_STRING[];

class UsersView : public CFormView {
  DECLARE_DYNCREATE(UsersView)

private:
  /*FormView for add or edit users*/
  std::unique_ptr<UserView> m_userView;
  std::unique_ptr<CAddressView> m_userAddress;
  CString m_name;
  CString m_email;
  CString m_password;
  CString m_password_r;

public:
  bool m_fClickedList;

protected:
  UsersView();
  virtual ~UsersView();

public:
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_FORM_USER };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

  CFont m_NewHeaderFont;
  CHeaderCtrlEx m_HeaderCtrl;
  CTableCtrl m_usersTable;

  DECLARE_MESSAGE_MAP()
public:
  /*For releasing dialog memory and avoid memory leak*/
  CGoodsDbDoc *GetDocument() const;
  void userViewDeleted(UserView *usrview);
  void addressViewDeleted(CAddressView *addrView);
  void populateTable();

public:
  afx_msg LRESULT OnNotifyDescriptionEdited(WPARAM, LPARAM);
  afx_msg void OnBnClickedAddUser();
  virtual BOOL PreCreateWindow(CREATESTRUCT &cs);
  virtual void OnInitialUpdate();
  afx_msg void OnColumnclickListUsers(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnClickListUsers(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnBnClickedButtonAddAddress();
};
