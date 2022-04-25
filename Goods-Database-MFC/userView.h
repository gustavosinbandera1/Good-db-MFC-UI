#pragma once

// UserView dialog
class UsersView;
class UserView : public CDialogEx {
  DECLARE_DYNAMIC(UserView)

public:
  UserView(CWnd *pParent = nullptr); // standard constructor
  virtual ~UserView();

private:
  UsersView* m_parent;
  virtual void PostNcDestroy();

// Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_ADD_EDIT_USER };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

  DECLARE_MESSAGE_MAP()
public:
	// user name
	CString name;
	// user email
	CString email;
	// user password
	CString password;
	// password repeat
	CString passwordR;
	afx_msg void OnBnClickedOk();
	
};
