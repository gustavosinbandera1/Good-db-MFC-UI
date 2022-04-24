#pragma once

// NewOrderView form view

class NewOrderView : public CFormView {
  DECLARE_DYNCREATE(NewOrderView)

protected:
  NewOrderView(); // protected constructor used by dynamic creation
  virtual ~NewOrderView();

public:
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_FORM_NEW_ORDER };
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
