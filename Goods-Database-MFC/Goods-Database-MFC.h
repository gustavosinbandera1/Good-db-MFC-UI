#pragma once
#include "resource.h" // main symbols
#include "dbManager.h"  //database application


static UINT task_Thread(LPVOID param);


class CGoodsDbApp : public CWinAppEx {
public:
  CGoodsDbApp() noexcept;
  ~CGoodsDbApp();

  int number;
  CWinThread *threadConnection;
  void taskThread(void);

  // Overrides
public:
  virtual BOOL InitInstance();
  virtual int ExitInstance();
  
  CMultiDocTemplate *pTabbedDocTmplt;

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
