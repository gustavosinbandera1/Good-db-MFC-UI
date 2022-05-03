#include "pch.h"
#include "Goods-Database-MFC.h"
#include "usersView.h"
#include "userView.h"
#include "GoodsDbDoc.h"
#include <cstdlib>
#include "resource.h"

IMPLEMENT_DYNCREATE(UsersView, CFormView)

//second param read/write description
std::vector<std::pair<CString, bool>> USER_HEADERS{ {L"Name", true }, {L"Email", true}, {L"Password", true} };

enum class USER_HEADERS_POS {
	NAME,
	EMAIL,
	PASSWORD,
	NUM_HEADERS
};
UsersView::UsersView() : CFormView(IDD_FORM_USER)
, m_name(_T(""))
, m_email(_T(""))
, m_password(_T(""))
, m_password_r(_T(""))
{ }


UsersView::~UsersView() {}

void UsersView::DoDataExchange(CDataExchange *pDX) {
	CFormView::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_NAME, m_name);
	DDX_Text(pDX, IDC_EDIT_EMAIL, m_email);
	DDX_Text(pDX, IDC_EDIT_PASSWORD, m_password);
	DDX_Control(pDX, IDC_LIST_USERS, m_usersTable);
}

BEGIN_MESSAGE_MAP(UsersView, CFormView)
ON_BN_CLICKED(IDC_ADD_USER, &UsersView::OnBnClickedAddUser)
ON_WM_SIZE()
//ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_USERS, &UsersView::OnLvnItemchangedListUsers)
ON_MESSAGE(WM_NOTIFY_DESCRIPTION_EDITED, OnNotifyDescriptionEdited)
ON_NOTIFY(LVN_COLUMNCLICK, IDC_LIST_USERS, &UsersView::OnColumnclickListUsers)
ON_NOTIFY(NM_CLICK, IDC_LIST_USERS, &UsersView::OnClickListUsers)
END_MESSAGE_MAP()

// UsersView diagnostics

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
    bool readOnly = false;

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
  m_usersTable.SetExtendedStyle(
	  LVS_EX_FULLROWSELECT
	  | LVS_EX_GRIDLINES
	  | LVS_EX_TRACKSELECT
	  | LVS_SHOWSELALWAYS);

  populateTable();
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
  CGoodsDbDoc *pDoc = GetDocument();
  ref<set_member> mbr;
  large_set<Person> persons = pDoc->getAllUser();

  const int numRows = persons.size();
  m_usersTable.setHeaders(USER_HEADERS);

  int row = 0;
  do {
    mbr = persons.getNext();
    if (mbr != nullptr) {
      ref<Person> p = mbr->obj;
      const char *name = p->getName()->get_text();
      const char *email = p->getEmail()->get_text();
      const char *pwd = p->getPassword()->get_text();

	  m_usersTable.InsertItem(row, CString(name));
	  m_usersTable.SetItemText(row, 1, CString(email));
	  m_usersTable.SetItemText(row, 2, CString(pwd));
      ++row;
    }
  } while (mbr != NULL);
}

// OnNotifyDescriptionEdited()
LRESULT UsersView::OnNotifyDescriptionEdited(WPARAM wParam, LPARAM lParam) {
  LV_DISPINFO *dispinfo = reinterpret_cast<LV_DISPINFO *>(lParam);
  int row = dispinfo->item.iItem;
  int col = dispinfo->item.iSubItem;

  m_usersTable.OnEndLabelEdit(wParam, lParam);
  
  switch (col) {
	  case static_cast<int>(USER_HEADERS_POS::NAME) :
		m_name = m_usersTable.GetItemText(row, col);
    break;
  case static_cast<int>(USER_HEADERS_POS::EMAIL):
    m_email = m_usersTable.GetItemText(row, col);
    break;
  case static_cast<int>(USER_HEADERS_POS::PASSWORD):
    m_password = m_usersTable.GetItemText(row, col);
    break;

  default:
    break;
  }

  UpdateData(false);
  return 0;
}

void UsersView::OnColumnclickListUsers(NMHDR *pNMHDR, LRESULT *pResult)
{
	//AfxMessageBox(CString("column HEADER click"));
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
}


void UsersView::OnClickListUsers(NMHDR *pNMHDR, LRESULT *pResult) {
	CString dat;

	int selRow = m_usersTable.getSelectedRow();
	int selCol = m_usersTable.GetSelectedColumn();
	
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	
	//dat.Format(_T("output row: %d  col: %d"), selRow, selCol);
   //AfxMessageBox(dat);
	
   
   if (selRow >= 0 || selCol >= 0) {
		m_name = m_usersTable.GetItemText(selRow, 0);
		m_email = m_usersTable.GetItemText(selRow, 1);
		m_password = m_usersTable.GetItemText(selRow, 2);
		UpdateData(FALSE);
	}

	*pResult = 0;
}
