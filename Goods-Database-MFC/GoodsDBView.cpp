
// GoodsDBView.cpp : implementation of the CGoodsDbView class
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "Goods-Database-MFC.h"
#endif

#include "GoodsDbDoc.h"
#include "GoodsDBView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CGoodsDbView

IMPLEMENT_DYNCREATE(CGoodsDbView, CView)

BEGIN_MESSAGE_MAP(CGoodsDbView, CView)
ON_WM_CONTEXTMENU()
ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// CGoodsDbView construction/destruction

CGoodsDbView::CGoodsDbView() noexcept {
  // TODO: add construction code here
}

CGoodsDbView::~CGoodsDbView() {}

BOOL CGoodsDbView::PreCreateWindow(CREATESTRUCT &cs) {
  // TODO: Modify the Window class or styles here by modifying
  //  the CREATESTRUCT cs

  return CView::PreCreateWindow(cs);
}

// CGoodsDbView drawing

void CGoodsDbView::OnDraw(CDC * /*pDC*/) {
  CGoodsDbDoc *pDoc = GetDocument();
  ASSERT_VALID(pDoc);
  if (!pDoc)
    return;

  // TODO: add draw code for native data here
}

void CGoodsDbView::OnRButtonUp(UINT /* nFlags */, CPoint point) {
  ClientToScreen(&point);
  OnContextMenu(this, point);
}

void CGoodsDbView::OnContextMenu(CWnd * /* pWnd */, CPoint point) {
#ifndef SHARED_HANDLERS
  theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x,
                                                point.y, this, TRUE);
#endif
}

// CGoodsDbView diagnostics

#ifdef _DEBUG
void CGoodsDbView::AssertValid() const { CView::AssertValid(); }

void CGoodsDbView::Dump(CDumpContext &dc) const { CView::Dump(dc); }

CGoodsDbDoc *CGoodsDbView::GetDocument() const // non-debug version is inline
{
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGoodsDbDoc)));
  return (CGoodsDbDoc *)m_pDocument;
}
#endif //_DEBUG

// CGoodsDbView message handlers
