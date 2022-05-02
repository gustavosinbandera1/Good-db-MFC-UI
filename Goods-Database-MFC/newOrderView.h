#pragma once
#include "TableView.h"

class CGoodsDbDoc;

class NewOrderView : public CFormView {
  DECLARE_DYNCREATE(NewOrderView)

protected:
  NewOrderView(); // protected constructor used by dynamic creation
  virtual ~NewOrderView();

public:
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_FORM_NEW_ORDER };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support
  CFont m_NewHeaderFont;
  CHeaderCtrlEx m_HeaderCtrl;
  CTableCtrl m_newOrderObj;
  DECLARE_MESSAGE_MAP()
public:
	CGoodsDbDoc *GetDocument() const;
	void populateTable();

	

public:
	afx_msg LRESULT OnNotifyDescriptionEdited(WPARAM, LPARAM);

	afx_msg void OnColumnclickListItems(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnClickListItem(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
};
