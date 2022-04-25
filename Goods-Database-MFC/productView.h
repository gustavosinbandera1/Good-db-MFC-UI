#pragma once

// ProductView dialog
class ProductsView;
class ProductView : public CDialogEx {
  DECLARE_DYNAMIC(ProductView)

private:
	ProductsView* m_parent;
	virtual void PostNcDestroy();

public:
  ProductView(CWnd *pParent = nullptr); // standard constructor
  virtual ~ProductView();

// Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_ADD_EDIT_PRODUCT };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

  DECLARE_MESSAGE_MAP()
public:
	int m_sku;
	CString m_description;
	double m_price;
	double m_weight;
	afx_msg void OnBnClickedOk();
};
