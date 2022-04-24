#include "pch.h"
#include "framework.h"
#include "Goods-Database-MFC.h"

#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWndEx)
ON_WM_CREATE()
ON_COMMAND(ID_TEST_TEST1, &CMainFrame::OnTestTest1)
END_MESSAGE_MAP()

CMainFrame::CMainFrame() noexcept {
  theApp.m_nAppLook =
      theApp.GetInt(_T("ApplicationLook"), ID_VIEW_APPLOOK_VS_2008);

  pCurrentMenu = new CMenu;
}

CMainFrame::~CMainFrame() {}

void CMainFrame::ShowProductMenu() {}

void CMainFrame::ShowUserMenu() {
  SetMenu(NULL);
  pCurrentMenu->DestroyMenu();
  pCurrentMenu->LoadMenuW(IDR_MENU_USER);
  SetMenu(pCurrentMenu);
  DrawMenuBar();
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CMDIFrameWndEx::OnCreate(lpCreateStruct) == -1)
    return -1;

  BOOL bNameValid;

  if (!m_wndMenuBar.Create(this)) {
    TRACE0("Failed to create menubar\n");
    return -1; // fail to create
  }

  m_wndMenuBar.SetPaneStyle(m_wndMenuBar.GetPaneStyle() | CBRS_SIZE_DYNAMIC |
                            CBRS_TOOLTIPS | CBRS_FLYBY);

  //// prevent the menu bar from taking the focus on activation
  CMFCPopupMenu::SetForceMenuFocus(FALSE);

  if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT,
                             WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER |
                                 CBRS_TOOLTIPS | CBRS_FLYBY |
                                 CBRS_SIZE_DYNAMIC) ||
      !m_wndToolBar.LoadToolBar(theApp.m_bHiColorIcons ? IDR_MAINFRAME_256
                                                       : IDR_MAINFRAME)) {
    TRACE0("Failed to create toolbar\n");
    return -1; // fail to create
  }

  CString strToolBarName;
  bNameValid = strToolBarName.LoadString(IDS_TOOLBAR_STANDARD);
  ASSERT(bNameValid);

  m_wndToolBar.SetWindowText(strToolBarName);
  m_wndMenuBar.EnableDocking(CBRS_ALIGN_ANY);
  m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
  EnableDocking(CBRS_ALIGN_ANY);
  DockPane(&m_wndMenuBar);
  DockPane(&m_wndToolBar);

  return 0;
}

void CMainFrame::OnTestTest1() { ShowUserMenu(); }
