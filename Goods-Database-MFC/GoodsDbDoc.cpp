
// GoodsDbDoc.cpp : implementation of the CGoodsDbDoc class
//

#include "pch.h"
#include "framework.h"

// SHARED_HANDLERS can be defined in an ATL project implementing preview,
// thumbnail and search filter handlers and allows sharing of document code with
// that project.
#ifndef SHARED_HANDLERS
#include "Goods-Database-MFC.h"
#endif

#include "GoodsDbDoc.h"
#include "ordersDB.h"
#include <propkey.h>
//#include "order.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CGoodsDbDoc

IMPLEMENT_DYNCREATE(CGoodsDbDoc, CDocument)

BEGIN_MESSAGE_MAP(CGoodsDbDoc, CDocument)
END_MESSAGE_MAP()


CGoodsDbDoc::CGoodsDbDoc() noexcept {
	dbManager = DB_MANAGER;
}

CGoodsDbDoc::~CGoodsDbDoc() {}


BOOL CGoodsDbDoc::OnNewDocument() {
  if (!CDocument::OnNewDocument())
    return FALSE;

  return TRUE;
}

// CGoodsDbDoc serialization

void CGoodsDbDoc::Serialize(CArchive &ar) {
  if (ar.IsStoring()) {
    // TODO: add storing code here
  } else {
    // TODO: add loading code here
  }
}

#ifdef SHARED_HANDLERS

// Support for thumbnails
void CGoodsDbDoc::OnDrawThumbnail(CDC &dc, LPRECT lprcBounds) {
  // Modify this code to draw the document's data
  dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

  CString strText = _T("TODO: implement thumbnail drawing here");
  LOGFONT lf;

  CFont *pDefaultGUIFont =
      CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT));
  pDefaultGUIFont->GetLogFont(&lf);
  lf.lfHeight = 36;

  CFont fontDraw;
  fontDraw.CreateFontIndirect(&lf);

  CFont *pOldFont = dc.SelectObject(&fontDraw);
  dc.DrawText(strText, lprcBounds, DT_CENTER | DT_WORDBREAK);
  dc.SelectObject(pOldFont);
}

// Support for Search Handlers
void CGoodsDbDoc::InitializeSearchContent() {
  CString strSearchContent;
  // Set search contents from document's data.
  // The content parts should be separated by ";"

  // For example:  strSearchContent = _T("point;rectangle;circle;ole object;");
  SetSearchContent(strSearchContent);
}

void CGoodsDbDoc::SetSearchContent(const CString &value) {
  if (value.IsEmpty()) {
    RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
  } else {
    CMFCFilterChunkValueImpl *pChunk = nullptr;
    ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
    if (pChunk != nullptr) {
      pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
      SetChunkValue(pChunk);
    }
  }
}

#endif // SHARED_HANDLERS

// CGoodsDbDoc diagnostics

#ifdef _DEBUG
void CGoodsDbDoc::AssertValid() const { CDocument::AssertValid(); }

void CGoodsDbDoc::Dump(CDumpContext &dc) const { CDocument::Dump(dc); }
#endif //_DEBUG

// CGoodsDbDoc commands

void CGoodsDbDoc::addUser(CString name, CString email, CString password) {
	dbManager->addPerson(name, email, password);
	AfxMessageBox(CString("User saved"));
}

void CGoodsDbDoc::printAllUser() const {
	dbManager->printAllPerson();
	//DB_MANAGER->printAllPerson();
}

large_set<Person> CGoodsDbDoc::getAllUser() {
	return dbManager->getAllPerson();
}

void CGoodsDbDoc::deleteUser(CString email) {

}



void CGoodsDbDoc::addProduct(CString description, double price, double weight) {
	dbManager->addProduct(description, price, weight);
}

void CGoodsDbDoc::printAllProduct() {

}

large_set<Product> CGoodsDbDoc::getAllProduct() {
	return dbManager->getAllProduct();
}

void CGoodsDbDoc::deleteProduct() {

}

void CGoodsDbDoc::addDetail(ref<Order> order, char const * productSku, int quantity) {
}
