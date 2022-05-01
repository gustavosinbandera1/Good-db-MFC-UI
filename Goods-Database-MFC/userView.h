#pragma once

// UserView dialog
class UsersView;
class UserView : public CDialogEx {
  DECLARE_DYNAMIC(UserView)

public:
  UserView(bool read_only = false,
           CWnd *pParent = nullptr); // standard constructor
  virtual ~UserView();

private:
  CString name;
  CString email;
  CString password;
  bool readOnly;
  UsersView *m_parent;
  virtual void PostNcDestroy();

// Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_ADD_EDIT_USER };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

  DECLARE_MESSAGE_MAP()
public:
  afx_msg void OnBnClickedOk();
  virtual BOOL OnInitDialog();
};
