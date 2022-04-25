
// Goods-Database-MFC.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "Goods-Database-MFC.h"
#include "MainFrm.h"

#include "ChildFrm.h"
#include "GoodsDbDoc.h"
#include "GoodsDBView.h"
#include "tabViewCtrlView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CGoodsDbApp

BEGIN_MESSAGE_MAP(CGoodsDbApp, CWinAppEx)
ON_COMMAND(ID_APP_ABOUT, &CGoodsDbApp::OnAppAbout)
// Standard file based document commands
ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
END_MESSAGE_MAP()

// CGoodsDbApp construction

CGoodsDbApp::CGoodsDbApp() noexcept {
	//number = 0;
	//thread = NULL;
	//AfxMessageBox(CString("Hola")); // login here "should be"
    m_bHiColorIcons = TRUE;

  // support Restart Manager
  m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_ALL_ASPECTS;
#ifdef _MANAGED
  // If the application is built using Common Language Runtime support (/clr):
  //     1) This additional setting is needed for Restart Manager support to
  //     work properly. 2) In your project, you must add a reference to
  //     System.Windows.Forms in order to build.
  System::Windows::Forms::Application::SetUnhandledExceptionMode(
      System::Windows::Forms::UnhandledExceptionMode::ThrowException);
#endif

  // TODO: replace application ID string below with unique ID string;
  // recommended format for string is
  // CompanyName.ProductName.SubProduct.VersionInformation
  SetAppID(_T("GoodsDatabaseMFC.AppID.NoVersion"));

  // TODO: add construction code here,
  // Place all significant initialization in InitInstance
  //thread = AfxBeginThread(MyThread, this);
}

CGoodsDbApp::~CGoodsDbApp() {
	/*number = 101;
	if (thread != NULL) {
		::WaitForSingleObject(thread, 0xFFFFFF);
	}*/
}

// The one and only CGoodsDbApp object

CGoodsDbApp theApp;

// CGoodsDbApp initialization

BOOL CGoodsDbApp::InitInstance() {
  // InitCommonControlsEx() is required on Windows XP if an application
  // manifest specifies use of ComCtl32.dll version 6 or later to enable
  // visual styles.  Otherwise, any window creation will fail.
  INITCOMMONCONTROLSEX InitCtrls;
  InitCtrls.dwSize = sizeof(InitCtrls);
  // Set this to include all the common control classes you want to use
  // in your application.
  InitCtrls.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&InitCtrls);

  CWinAppEx::InitInstance();

  EnableTaskbarInteraction(FALSE);

  SetRegistryKey(_T("Local AppWizard-Generated Applications"));
  LoadStdProfileSettings(4); // Load standard INI file options (including MRU)

  InitContextMenuManager();

  InitKeyboardManager();

  InitTooltipManager();
  CMFCToolTipInfo ttParams;
  ttParams.m_bVislManagerTheme = TRUE;
  theApp.GetTooltipManager()->SetTooltipParams(
      AFX_TOOLTIP_TYPE_ALL, RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);




  pTabbedDocTmplt = new CMultiDocTemplate(
      IDR_GoodsDatabaseMFCTYPE, RUNTIME_CLASS(CGoodsDbDoc),
      RUNTIME_CLASS(CChildFrame), // custom MDI child frame
      RUNTIME_CLASS(CTabViewCtrlView));
  if (!pTabbedDocTmplt)
    return FALSE;
  AddDocTemplate(pTabbedDocTmplt);


  // create main MDI Frame window
  CMainFrame *pMainFrame = new CMainFrame;
  if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME)) {
    delete pMainFrame;
    return FALSE;
  }
  m_pMainWnd = pMainFrame;

  m_pMainWnd->DragAcceptFiles();



  // Parse command line for standard shell commands, DDE, file open
  CCommandLineInfo cmdInfo;
  ParseCommandLine(cmdInfo);

  // Enable DDE Execute open
  EnableShellOpen();
  RegisterShellFileTypes(TRUE);

  // Dispatch commands specified on the command line.  Will return FALSE if
  // app was launched with /RegServer, /Register, /Unregserver or /Unregister.
  if (!ProcessShellCommand(cmdInfo))
    return FALSE;
  // The main window has been initialized, so show and update it
  pMainFrame->ShowWindow(m_nCmdShow);
  pMainFrame->UpdateWindow();

  threadConnection = AfxBeginThread(task_Thread, this);

  //thread = AfxBeginThread(MyThread, this);
 // DB_MANAGER->connect();
  return TRUE;
}

int CGoodsDbApp::ExitInstance() {
  // TODO: handle additional resources you may have added
  return CWinAppEx::ExitInstance();
}

// CGoodsDbApp message handlers

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx {
public:
  CAboutDlg() noexcept;

// Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_ABOUTBOX };
#endif

protected:
  virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

  // Implementation
protected:
  DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX) {}

void CAboutDlg::DoDataExchange(CDataExchange *pDX) {
  CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// App command to run the dialog
void CGoodsDbApp::OnAppAbout() {
  CAboutDlg aboutDlg;
  aboutDlg.DoModal();
}

// CGoodsDbApp customization load/save methods

void CGoodsDbApp::PreLoadState() {
  BOOL bNameValid;
  CString strName;
  bNameValid = strName.LoadString(IDS_EDIT_MENU);
  ASSERT(bNameValid);
  GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EDIT);
}

void CGoodsDbApp::LoadCustomState() {}

void CGoodsDbApp::SaveCustomState() {}

void CGoodsDbApp::taskThread(void) {
	DB_MANAGER->connect();
	CMDIFrameWnd *pFrame = (CMDIFrameWnd*)AfxGetApp()->GetMainWnd();
	CWnd* pWndMain = AfxGetMainWnd();
	
	while (true) {
		Sleep(1000);
		number++;
		if (number >= 255) number = 0;
	}
}

// CGoodsDbApp message handlers

UINT task_Thread(LPVOID param) {
	static_cast<CGoodsDbApp*>(param)->taskThread();
	return 0;
}

