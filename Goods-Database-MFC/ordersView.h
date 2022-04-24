#pragma once

// OrdersView form view

class OrdersView : public CFormView {
  DECLARE_DYNCREATE(OrdersView)

protected:
  OrdersView(); // protected constructor used by dynamic creation
  virtual ~OrdersView();

public:
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_FORM_ORDER };
#endif
#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext &dc) const;
#endif
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

  DECLARE_MESSAGE_MAP()
};
