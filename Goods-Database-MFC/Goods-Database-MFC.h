#pragma once
#include "resource.h" // main symbols
#include "dbManager.h"  //database application


UINT MyThread(LPVOID param);


class CGoodsDbApp : public CWinAppEx {
public:
  CGoodsDbApp() noexcept;
  ~CGoodsDbApp();
  CMultiDocTemplate *pTabbedView;

  int number;
  CWinThread *thread;

  // Overrides
public:
  virtual BOOL InitInstance();
  virtual int ExitInstance();

  CMultiDocTemplate *pTabbedDocTmplt;
  CMultiDocTemplate *pDocTemplate;

  // Implementation
  UINT m_nAppLook;
  BOOL m_bHiColorIcons;

  virtual void PreLoadState();
  virtual void LoadCustomState();
  virtual void SaveCustomState();

  afx_msg void OnAppAbout();
  DECLARE_MESSAGE_MAP()
};

extern CGoodsDbApp theApp;
