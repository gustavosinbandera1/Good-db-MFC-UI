
// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include "PropertiesWnd.h"

class CMainFrame : public CMDIFrameWndEx {
  DECLARE_DYNAMIC(CMainFrame)
public:
  CMainFrame() noexcept;

public:
  virtual ~CMainFrame();
  afx_msg void ShowProductMenu();
  afx_msg void ShowUserMenu();

protected:
  CMFCMenuBar m_wndMenuBar;
  CMFCToolBar m_wndToolBar;

  CMenu *pCurrentMenu;
  // Generated message map functions
protected:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

  DECLARE_MESSAGE_MAP()

public:
  afx_msg void OnTestTest1();
};
