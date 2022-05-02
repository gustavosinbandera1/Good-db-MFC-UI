#pragma once
// ProductsView form view
#include <memory>
#include "ListCtrlEx.h"
#include "TableView.h"

class CGoodsDbDoc;
class ProductView;

class ProductsView : public CFormView {
  DECLARE_DYNCREATE(ProductsView)
private:
  std::unique_ptr<ProductView> m_productView;
  CString m_sSku;
  CString m_sDescription;
  double m_dPrice;
  double m_dWeight;

protected:
  ProductsView(); // protected constructor used by dynamic creation
  virtual ~ProductsView();

public:
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_FORM_PRODUCT };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

  CFont m_NewHeaderFont;
  CHeaderCtrlEx m_HeaderCtrl;
  CTableCtrl m_productsTable;
  DECLARE_MESSAGE_MAP()

public:
  CGoodsDbDoc *GetDocument() const;
  void productViewDeleted(ProductView *productview);
  void populateTable();

  virtual void OnUpdate(CView * /*pSender*/, LPARAM /*lHint*/,
                        CObject * /*pHint*/);

public:
	afx_msg LRESULT OnNotifyDescriptionEdited(WPARAM, LPARAM);
	afx_msg void OnBnClickedAddProduct();
	afx_msg void OnColumnclickListProducts(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnClickListProducts(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
};
