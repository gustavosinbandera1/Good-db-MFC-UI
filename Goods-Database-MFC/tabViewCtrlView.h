#pragma once
#include <afxtabview.h>
class CTabViewCtrlView : public CTabView {
  DECLARE_DYNCREATE(CTabViewCtrlView)

protected:
  CTabViewCtrlView();
  virtual ~CTabViewCtrlView();

public:
	CMFCTabCtrl tab_ctrl;
#ifdef _DEBUG
  virtual void AssertValid() const;
#ifndef _WIN32_WCE
  virtual void Dump(CDumpContext &dc) const;
#endif
#endif

public:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

protected:
  DECLARE_MESSAGE_MAP()
};
