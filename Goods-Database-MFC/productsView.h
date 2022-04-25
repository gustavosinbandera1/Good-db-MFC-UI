#pragma once
// ProductsView form view
#include <memory>

class CGoodsDbDoc;
class ProductView;

class ProductsView : public CFormView
{
	DECLARE_DYNCREATE(ProductsView)
private:
	std::unique_ptr<ProductView> m_productView;
protected:
	ProductsView();           // protected constructor used by dynamic creation
	virtual ~ProductsView();

public:
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FORM_PRODUCT };
#endif
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif

public: 
	CGoodsDbDoc* GetDocument() const;
	void productViewDeleted(ProductView *productview);
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedAddProduct();
};


