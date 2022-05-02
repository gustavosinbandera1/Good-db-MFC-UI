#pragma once
#include <afxcmn.h>
#include "HeaderCtrlEx.h"
#include <vector>

class CListCtrlEx : public CListCtrl {
public:
	CListCtrlEx();
	virtual ~CListCtrlEx();

protected:
	DECLARE_MESSAGE_MAP()
public:
	void setHeaders(std::vector<std::pair<CString, bool>> &headers);
	
protected:
	CFont* m_pOldItemFont;
	CFont* m_pOldSubItemFont;
	std::vector<std::pair<CString, bool>> headers;


	afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);

	//
	// Callbacks for whole control
	//

	// do we want to do the drawing ourselves?
	virtual bool IsDraw() { return false; }
	// if we are doing the drawing ourselves
	// override and put the code in here
	// and return TRUE if we did indeed do
	// all the drawing ourselves
	virtual bool OnDraw(CDC* /*pDC*/) { return false; }
	virtual bool OnDraw(CDC* /*pDC*/, const CRect& r) { return false; }
	// do we want to handle custom draw for
	// individual items
	virtual bool IsNotifyItemDraw() { return false; }
	// do we want to be notified when the
	// painting has finished
	virtual bool IsNotifyPostPaint() { return false; }
	// do we want to do any drawing after
	// the list control is finished
	virtual bool IsPostDraw() { return false; }
	// if we are doing the drawing afterwards ourselves
	// override and put the code in here
	// the return value is not used here
	virtual bool OnPostDraw(CDC* /*pDC*/) { return false; }

	//
	// Callbacks for each item
	//

	// return a pointer to the font to use for this item.
	// return NULL to use default
	virtual CFont* FontForItem(int /*nItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return NULL; }
	// return the text color to use for this item
	// return CLR_DEFAULT to use default
	virtual COLORREF TextColorForItem(int /*nItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return CLR_DEFAULT; }
	// return the background color to use for this item
	// return CLR_DEFAULT to use default
	virtual COLORREF BkColorForItem(int /*nItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return CLR_DEFAULT; }
	// do we want to do the drawing for this item ourselves?
	virtual bool IsItemDraw(int /*nItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }
	// if we are doing the drawing ourselves
	// override and put the code in here
	// and return TRUE if we did indeed do
	// all the drawing ourselves
	virtual bool OnItemDraw(CDC* /*pDC*/, int /*nItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }
	// do we want to handle custom draw for
	// individual sub items
	virtual bool IsNotifySubItemDraw(int /*nItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }
	// do we want to be notified when the
	// painting has finished
	virtual bool IsNotifyItemPostPaint(int /*nItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }
	// do we want to do any drawing after
	// the list control is finished
	virtual bool IsItemPostDraw() { return false; }
	// if we are doing the drawing afterwards ourselves
	// override and put the code in here
	// the return value is not used here
	virtual bool OnItemPostDraw(CDC* /*pDC*/, int /*nItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }

	//
	// Callbacks for each sub item
	//

	// return a pointer to the font to use for this sub item.
	// return NULL to use default
	virtual CFont* FontForSubItem(int /*nItem*/, int /*nSubItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return NULL; }
	// return the text color to use for this sub item
	// return CLR_DEFAULT to use default
	virtual COLORREF TextColorForSubItem(int /*nItem*/, int /*nSubItem*/, UINT /*nState*/, LPARAM /*lParam*/) { 
		//AfxMessageBox(CString("jejeje"));
		return CLR_DEFAULT; }
	// return the background color to use for this sub item
	// return CLR_DEFAULT to use default
	virtual COLORREF BkColorForSubItem(int /*nItem*/, int /*nSubItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return CLR_DEFAULT; }
	// do we want to do the drawing for this sub item ourselves?
	virtual bool IsSubItemDraw(int /*nItem*/, int /*nSubItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }
	// if we are doing the drawing ourselves
	// override and put the code in here
	// and return TRUE if we did indeed do
	// all the drawing ourselves
	virtual bool OnSubItemDraw(CDC* /*pDC*/, int /*nItem*/, int /*nSubItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }
	// do we want to be notified when the
	// painting has finished
	virtual bool IsNotifySubItemPostPaint(int /*nItem*/, int /*nSubItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }
	// do we want to do any drawing after
	// the list control is finished
	virtual bool IsSubItemPostDraw() { return false; }
	// if we are doing the drawing afterwards ourselves
	// override and put the code in here
	// the return value is not used here
	virtual bool OnSubItemPostDraw(CDC* /*pDC*/, int /*nItem*/, int /*nSubItem*/, UINT /*nState*/, LPARAM /*lParam*/) { return false; }

public:
	// methods for editable list
	int GetRowFromPoint(CPoint &point, int *col) const;
	CEdit* EditSubLabel(int nItem, int nCol);
	void OnEndLabelEdit(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnNMDblclk(NMHDR *pNMHDR, LRESULT *pResult);
};

