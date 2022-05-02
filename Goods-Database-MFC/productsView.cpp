#include "pch.h"
#include "Goods-Database-MFC.h"
#include "productsView.h"
#include "usersView.h"
#include "productView.h"
#include "GoodsDbDoc.h"


IMPLEMENT_DYNCREATE(ProductsView, CFormView)

std::vector<CString> PRODUCT_HEADERS{ L"Sku", L"Description", L"Price", L"Weight" };


ProductsView::ProductsView() : CFormView(IDD_FORM_PRODUCT), m_sSku(_T(""))
, m_sDescription(_T(""))
, m_dPrice(0)
, m_dWeight(0)
{ }


ProductsView::~ProductsView() {}

void ProductsView::DoDataExchange(CDataExchange *pDX) {
	CFormView::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_SKU, m_sSku);
	DDX_Text(pDX, IDC_EDIT_DESCRIPTION, m_sDescription);
	DDX_Text(pDX, IDC_EDIT_PRICE, m_dPrice);
	DDV_MinMaxDouble(pDX, m_dPrice, 0, 999999);
	DDX_Text(pDX, IDC_EDIT_WEIGHT, m_dWeight);
	DDV_MinMaxDouble(pDX, m_dWeight, 0, 999999);
	DDX_Control(pDX, IDC_LIST_PRODUCTS, m_productsTable);
}

BEGIN_MESSAGE_MAP(ProductsView, CFormView)
	ON_BN_CLICKED(IDC_ADD_PRODUCT, &ProductsView::OnBnClickedAddProduct)
	ON_MESSAGE(WM_NOTIFY_DESCRIPTION_EDITED, OnNotifyDescriptionEdited)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_LIST_PRODUCTS, &ProductsView::OnColumnclickListProducts)
	ON_NOTIFY(NM_CLICK, IDC_LIST_PRODUCTS, &ProductsView::OnClickListProducts)
END_MESSAGE_MAP()






void ProductsView::productViewDeleted(ProductView * productview) {
	if (m_productView && (m_productView.get() == productview)) {
		m_productView.reset(nullptr);
	}
}

void ProductsView::OnBnClickedAddProduct() {
  CGoodsDbDoc *pDoc = GetDocument();
  if (pDoc != NULL) {
    bool readOnly = false;
    m_productView = std::make_unique<ProductView>(readOnly, this);

    m_productView->Create(IDD_ADD_EDIT_PRODUCT, this);
    m_productView->ShowWindow(SW_SHOW);
  } else {
    AfxMessageBox(CString("There isn't document to perform the task "));
  }
}

CGoodsDbDoc * ProductsView::GetDocument() const {
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGoodsDbDoc)));
	return (CGoodsDbDoc *)m_pDocument;
}

BOOL ProductsView::PreCreateWindow(CREATESTRUCT &cs) {
	// TODO: Add your specialized code here and/or call the base class
	return CFormView::PreCreateWindow(cs);
}


void ProductsView::OnInitialUpdate() {
	CFormView::OnInitialUpdate();
	m_productsTable.SetExtendedStyle(
		LVS_EX_FULLROWSELECT
		| LVS_EX_GRIDLINES
		| LVS_EX_TRACKSELECT
		| LVS_SHOWSELALWAYS);

	populateTable();
	m_NewHeaderFont.CreatePointFont(190, CString("MS Serif"));

	CHeaderCtrl *pHeader = NULL;
	pHeader = m_productsTable.GetHeaderCtrl();

	if (pHeader == NULL)
		return;

	VERIFY(m_HeaderCtrl.SubclassWindow(pHeader->m_hWnd));

	////// A BIGGER FONT MAKES THE CONTROL BIGGER
	m_HeaderCtrl.SetFont(&m_NewHeaderFont);

	HDITEM hdItem;

	hdItem.mask = HDI_FORMAT;

	for (int i = 0; i < m_HeaderCtrl.GetItemCount(); i++) {
		m_HeaderCtrl.GetItem(i, &hdItem);
		hdItem.fmt |= HDF_OWNERDRAW;
		m_HeaderCtrl.SetItem(i, &hdItem);
	}
}

void ProductsView::populateTable() {
	CGoodsDbDoc *pDoc = GetDocument();
	ref<set_member> mbr;
	large_set<Product> products = pDoc->getAllProduct();

	const int numRows = products.size();
	m_productsTable.setHeaders(PRODUCT_HEADERS);

	int row = 0;
	do {
		mbr = products.getNext();
		if (mbr != nullptr) {
			ref<Product> p = mbr->obj;
			int sku = p->getSku();
			const char *desc = p->getDescription()->get_text();
			double price = p->getPrice();
			double weight = p->getWeight();
			
			CString _sku;
			CString _price;
			CString _weight;

			_sku.Format(_T("%d"), sku);
			_price.Format(_T("%0.2f"), price);
			_weight.Format(_T("%0.2F"), weight);

			m_productsTable.InsertItem(row, _sku);
			m_productsTable.SetItemText(row, 1, CString(desc));
			m_productsTable.SetItemText(row, 2, _price);
			m_productsTable.SetItemText(row, 3, _weight);
			++row;
		}
	} while (mbr != NULL);
}





void ProductsView::OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/) {
	//AfxMessageBox(CString("Update from document to Products view"));
	// TODO: Add your specialized code here and/or call the base class
}




// OnNotifyDescriptionEdited()
LRESULT ProductsView::OnNotifyDescriptionEdited(WPARAM wParam, LPARAM lParam) {
	m_productsTable.OnEndLabelEdit(wParam, lParam);
	return 0;
}

void ProductsView::OnColumnclickListProducts(NMHDR *pNMHDR, LRESULT *pResult)
{
	//AfxMessageBox(CString("column HEADER click"));
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
}

void ProductsView::OnClickListProducts(NMHDR *pNMHDR, LRESULT *pResult) {
	CString dat;

	int selRow = m_productsTable.getSelectedRow();
	int selCol = m_productsTable.GetSelectedColumn();

	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	//dat.Format(_T("output row: %d  col: %d"), selRow, selCol);
  // AfxMessageBox(dat);


	if (selRow >= 0 || selCol >= 0) {
		m_sSku = m_productsTable.GetItemText(selRow, 0);
		m_sDescription = m_productsTable.GetItemText(selRow, 1);
		m_dPrice = _tstof(m_productsTable.GetItemText(selRow, 2));
		m_dWeight = _tstof(m_productsTable.GetItemText(selRow, 3));
		UpdateData(FALSE);
	}

	*pResult = 0;
}
