#pragma once

// CAddress dialog

class CAddress : public CDialog {
  DECLARE_DYNAMIC(CAddress)

public:
  CAddress(CWnd *pParent = nullptr); // standard constructor
  virtual ~CAddress();

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
	CComboBox combo_country;
	CComboBox combo_city;
	CComboBox combo_state;
	CComboBox combo_type;

private:
	void populateOptions();
};
