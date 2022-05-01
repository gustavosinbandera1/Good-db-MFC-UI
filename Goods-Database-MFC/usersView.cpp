#include "pch.h"
#include "Goods-Database-MFC.h"
#include "usersView.h"
#include "userView.h"
#include "GoodsDbDoc.h"
#include <cstdlib>
#include "resource.h"

IMPLEMENT_DYNCREATE(UsersView, CFormView)
char const *const USER_HEADER_STRING[] = {"Name", "Email", "Password", NULL};

UsersView::UsersView() : CFormView(IDD_FORM_USER)
, m_name(_T(""))
, m_email(_T(""))
, m_password(_T(""))
, m_password_r(_T(""))
{ m_initialized = FALSE; }

UsersView::~UsersView() {}

void UsersView::DoDataExchange(CDataExchange *pDX) {
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ITEMS, m_table);
	DDX_Text(pDX, IDC_EDIT_NAME, m_name);
	DDX_Text(pDX, IDC_EDIT_EMAIL, m_email);
	DDX_Text(pDX, IDC_EDIT_PASSWORD, m_password);
	DDX_Control(pDX, IDC_LIST_USERS, m_usersTable);
}

BEGIN_MESSAGE_MAP(UsersView, CFormView)
ON_BN_CLICKED(IDC_ADD_USER, &UsersView::OnBnClickedAddUser)
ON_WM_SIZE()
ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_USERS, &UsersView::OnLvnItemchangedListUsers)
ON_MESSAGE(WM_NOTIFY_DESCRIPTION_EDITED, OnNotifyDescriptionEdited)
//ON_NOTIFY(LVN_KEYDOWN, IDC_LIST_USERS, &UsersView::OnKeydownListUsers)
END_MESSAGE_MAP()

// UsersView diagnostics

#ifdef _DEBUG
void UsersView::AssertValid() const { CFormView::AssertValid(); }

#ifndef _WIN32_WCE
void UsersView::Dump(CDumpContext &dc) const { CFormView::Dump(dc); }
#endif
#endif //_DEBUG

// UsersView message handlers

// for reset the ptr to UserView dialog
void UsersView::userViewDeleted(UserView *usrview) {
  if (m_userView && (m_userView.get() == usrview)) {
    m_userView.reset(nullptr);
  }
}

void UsersView::OnBnClickedAddUser() {
  CGoodsDbDoc *pDoc = GetDocument();
  if (pDoc != NULL) {
    bool readOnly = true;

    m_userView = std::make_unique<UserView>(readOnly, this);
    m_userView->Create(IDD_ADD_EDIT_USER, this);
    m_userView->ShowWindow(SW_SHOW);
  } else {
    AfxMessageBox(CString("There isn't document to perform the task "));
  }
}

CGoodsDbDoc *UsersView::GetDocument() const {
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGoodsDbDoc)));
  return (CGoodsDbDoc *)m_pDocument;
}

BOOL UsersView::PreCreateWindow(CREATESTRUCT &cs) {
  // TODO: Add your specialized code here and/or call the base class
  return CFormView::PreCreateWindow(cs);
}

static void AddData(CListCtrl &ctrl, int row, int col, CString str) {
  LVITEM lv;
  lv.iItem = row;
  lv.iSubItem = col;
  lv.pszText = (wchar_t *)str.GetString();
  lv.mask = LVIF_TEXT;
  if (col == 0)
    ctrl.InsertItem(&lv);
  else
    ctrl.SetItem(&lv);
}

void UsersView::OnInitialUpdate() {
  CFormView::OnInitialUpdate();
  // populateTable();
  FillListCtrl(m_usersTable);

  // m_NewHeaderFont is of type CFont
  m_NewHeaderFont.CreatePointFont(190, CString("MS Serif"));

  CHeaderCtrl *pHeader = NULL;
  pHeader = m_usersTable.GetHeaderCtrl();

  if (pHeader == NULL)
    return;

  VERIFY(m_HeaderCtrl.SubclassWindow(pHeader->m_hWnd));

  // A BIGGER FONT MAKES THE CONTROL BIGGER
  m_HeaderCtrl.SetFont(&m_NewHeaderFont);

  HDITEM hdItem;

  hdItem.mask = HDI_FORMAT;

  for (int i = 0; i < m_HeaderCtrl.GetItemCount(); i++) {
    m_HeaderCtrl.GetItem(i, &hdItem);

    hdItem.fmt |= HDF_OWNERDRAW;

    m_HeaderCtrl.SetItem(i, &hdItem);
  }
}

void UsersView::populateTable() {
  //CGoodsDbDoc *pDoc = GetDocument();
  //ref<set_member> mbr;
  //large_set<Person> persons = pDoc->getAllUser();

  //const int numRows = persons.size();
  //const int numCols =
  //    ( sizeof(USER_HEADER_STRING) / sizeof(USER_HEADER_STRING[0]) ) - 1;

  ////m_table.SetSize(numCols, numRows);
  //m_table.SetSize(numCols, numRows);
  //for (int c = 0; c < numCols; c++) {
  //  m_table.SetColHeading(c, CString(USER_HEADER_STRING[c]));
  //  m_table.SetColWidth(c, 250);
  //}

  //int col = 0;
  //int row = 0;
  //do {
  //  mbr = persons.getNext();
  //  if (mbr != nullptr) {
  //    ref<Person> p = mbr->obj;
  //    const char *name = p->getName()->get_text();
  //    const char *email = p->getEmail()->get_text();
  //    const char *pwd = p->getPassword()->get_text();

  //    m_table.SetCellText(0, row, CString(name));
  //    m_table.SetCellText(1, row, CString(email));
  //    m_table.SetCellText(2, row, CString(pwd));
  //    ++row;
  //  }
  //} while (mbr != NULL);
}

BOOL UsersView::OnCommand(WPARAM wParam, LPARAM lParam) {
  //if ((HIWORD(wParam) == LBN_SELCHANGE) && (LOWORD(wParam) == IDC_ITEMS)) {
  //  int selRow = m_table.GetSelRow();
  //  if (selRow >= 0) {
  //    m_name = m_table.GetCellText(0, selRow);
  //    m_email = m_table.GetCellText(1, selRow);
  //    m_password = m_table.GetCellText(2, selRow);
  //    UpdateData(FALSE);
  //  }
  //}
  return CFormView::OnCommand(wParam, lParam);
}

// Obtain cursor position and offset it to position it at interview list control
CPoint UsersView::InterviewListCursorPosition() const {
  DWORD pos = GetMessagePos();
  CPoint pt(LOWORD(pos), HIWORD(pos));
  ScreenToClient(&pt);

  CRect rect;
  CWnd *pWnd = GetDlgItem(IDC_LIST_USERS);
  pWnd->GetWindowRect(&rect);
  ScreenToClient(&rect);

  pt.x -= rect.left;
  pt.y -= rect.top;
  return pt;
}

void UsersView::OnLvnItemchangedListUsers(NMHDR *pNMHDR, LRESULT *pResult) {
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	int nItem = m_usersTable.getSelectedRow();
	int nColumn =  m_usersTable.GetSelectedColumn();
	
	CString text = m_usersTable.GetItemText(nItem, 0);
	//if(!text.IsEmpty())
		//AfxMessageBox(text);
	//CString data;
	//if (nItem > 0) {
		//data.Format(_T("col : %d and first text = %s"), nItem, text);
		//AfxMessageBox(data);
	//}

	*pResult = 0;
}

// OnNotifyDescriptionEdited()
LRESULT UsersView::OnNotifyDescriptionEdited(WPARAM wParam, LPARAM lParam) {
  m_usersTable.OnEndLabelEdit(wParam, lParam);
  return 0;
}
