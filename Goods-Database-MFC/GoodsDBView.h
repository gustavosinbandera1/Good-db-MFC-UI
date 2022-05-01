
// GoodsDBView.h : interface of the CGoodsDbView class
//

#pragma once

class CGoodsDbView : public CView {
protected: // create from serialization only
  CGoodsDbView() noexcept;
  DECLARE_DYNCREATE(CGoodsDbView)

  // Attributes
public:
  CGoodsDbDoc *GetDocument() const;

  // Operations
public:
  // Overrides
public:
  virtual void OnDraw(CDC *pDC); // overridden to draw this view
  virtual BOOL PreCreateWindow(CREATESTRUCT &cs);

protected:
  // Implementation
public:
  virtual ~CGoodsDbView();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext &dc) const;
#endif

protected:
  // Generated message map functions
protected:
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
  DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG // debug version in GoodsDBView.cpp
inline CGoodsDbDoc *CGoodsDbView::GetDocument() const {
  return reinterpret_cast<CGoodsDbDoc *>(m_pDocument);
}
#endif
