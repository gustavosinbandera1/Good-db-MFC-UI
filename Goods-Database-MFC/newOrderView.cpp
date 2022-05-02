// newOrderView.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "newOrderView.h"
#include "GoodsDbDoc.h"

// NewOrderView

IMPLEMENT_DYNCREATE(NewOrderView, CFormView)

std::vector<CString> ITEM_HEADERS{ L"Sku", L"Description", L"Price", L"Quantity", L"Total" };


NewOrderView::NewOrderView() : CFormView(IDD_FORM_NEW_ORDER) {}

NewOrderView::~NewOrderView() {}

void NewOrderView::DoDataExchange(CDataExchange *pDX) {
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INVENTORY_LIST, m_newOrderObj);
}

CGoodsDbDoc * NewOrderView::GetDocument() const {
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGoodsDbDoc)));
	return (CGoodsDbDoc *)m_pDocument;
}

void NewOrderView::populateTable() {
	CGoodsDbDoc *pDoc = GetDocument();
	ref<set_member> mbr;
	int _quantity = 0;
	//if(pDoc != nullptr)
		large_set<Product> products = pDoc->getAllProduct();

	//const int numRows = products.size();
	m_newOrderObj.setHeaders(ITEM_HEADERS);



	int row = 0;
	do {
		mbr = products.getNext();
		if (mbr != nullptr) {
			ref<Product> p = mbr->obj;
			int sku = p->getSku();
			const char *desc = p->getDescription()->get_text();
			double price = p->getPrice();
			//double weight = p->getWeight();

			CString _sku;
			CString _price;
			CString _quantity;
			CString _total;

			_sku.Format(_T("%d"), sku);
			_price.Format(_T("%0.2f"), price);
			_quantity.Format(_T("%d"), 0);

			m_newOrderObj.InsertItem(row, _sku);
			m_newOrderObj.SetItemText(row, 1, CString(desc));
			m_newOrderObj.SetItemText(row, 2, _price);
			m_newOrderObj.SetItemText(row, 3, _quantity);
			m_newOrderObj.SetItemText(row, 3, _total);
			++row;
		}
	} while (mbr != NULL);
}

void NewOrderView::OnColumnclickListItems(NMHDR * pNMHDR, LRESULT * pResult) {
	//AfxMessageBox(CString("column HEADER click"));
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
}

void NewOrderView::OnClickListItem(NMHDR * pNMHDR, LRESULT * pResult) {
	CString dat;

	int selRow = m_newOrderObj.getSelectedRow();
	int selCol = m_newOrderObj.GetSelectedColumn();

	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	dat.Format(_T("output row: %d  col: %d"), selRow, selCol);
    AfxMessageBox(dat);


	if (selRow >= 0 || selCol >= 0) {
		
		UpdateData(FALSE);
	}

	*pResult = 0;
}

void NewOrderView::OnInitialUpdate() {
	CFormView::OnInitialUpdate();
	m_newOrderObj.SetExtendedStyle(
		LVS_EX_FULLROWSELECT
		| LVS_EX_GRIDLINES
		| LVS_EX_TRACKSELECT
		| LVS_SHOWSELALWAYS);

	populateTable();
	//m_NewHeaderFont.CreatePointFont(190, CString("MS Serif"));

	//CHeaderCtrl *pHeader = NULL;
	//pHeader = m_newOrderObj.GetHeaderCtrl();

	//if (pHeader == NULL)
	//	return;

	//VERIFY(m_HeaderCtrl.SubclassWindow(pHeader->m_hWnd));

	//////// A BIGGER FONT MAKES THE CONTROL BIGGER
	//m_HeaderCtrl.SetFont(&m_NewHeaderFont);

	//HDITEM hdItem;

	//hdItem.mask = HDI_FORMAT;

	//for (int i = 0; i < m_HeaderCtrl.GetItemCount(); i++) {
	//	m_HeaderCtrl.GetItem(i, &hdItem);
	//	hdItem.fmt |= HDF_OWNERDRAW;
	//	m_HeaderCtrl.SetItem(i, &hdItem);
	//}
}

BOOL NewOrderView::PreCreateWindow(CREATESTRUCT &cs) {
	// TODO: Add your specialized code here and/or call the base class
	return CFormView::PreCreateWindow(cs);
}

BEGIN_MESSAGE_MAP(NewOrderView, CFormView)
END_MESSAGE_MAP()
