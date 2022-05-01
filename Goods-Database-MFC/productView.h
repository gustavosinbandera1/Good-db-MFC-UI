#pragma once

// ProductView dialog
class ProductsView;
class ProductView : public CDialogEx {
  DECLARE_DYNAMIC(ProductView)

private:
	virtual void PostNcDestroy();

private:
	int m_sku;
	CString m_description;
	double m_price;
	double m_weight;
	bool readOnly;
	ProductsView* m_parent;

public:
  ProductView(bool read_only = false, CWnd *pParent = nullptr); // standard constructor
  virtual ~ProductView();

// Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_ADD_EDIT_PRODUCT };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

  DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	virtual BOOL OnInitDialog();
};
