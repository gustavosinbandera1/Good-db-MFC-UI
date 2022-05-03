#pragma once
// CAddress dialog
class UsersView;
class CAddressView : public CDialog {
  DECLARE_DYNAMIC(CAddressView)

public:
  CAddressView(CWnd *pParent); // standard constructor
  virtual ~CAddressView();

// Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_DIALOG_ADD_ADDRESS };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support
  DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnCountryChange();
	afx_msg void OnCityChange();
	afx_msg void OnStateChange();
	afx_msg void OnTypeChange();
	virtual BOOL OnInitDialog();

private:
	UsersView *m_parent;
	CComboBox combo_country;
	CComboBox combo_city;
	CComboBox combo_state;
	CComboBox combo_type;

private:
	void populateOptions();
public:
	afx_msg void OnBnClickedOk();
	virtual void PostNcDestroy();
};
