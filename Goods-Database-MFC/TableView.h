#pragma once
#include "ListCtrlEx.h"
#include "HeaderCtrlEx.h"

class CTableView : public CListCtrlEx {
public:

//protected:
//	DECLARE_MESSAGE_MAP()

protected:
	//CFont m_NewHeaderFont;
 //  CHeaderCtrlEx m_HeaderCtrl;

	virtual bool IsDraw() override;
	virtual bool OnDraw(CDC* pDC, const CRect& r) override;
	virtual bool OnDraw(CDC* pDC) override;
	virtual bool IsNotifyItemDraw() override;
	
	virtual bool IsNotifySubItemDraw(int nItem,
		UINT nState,
		LPARAM lParam) override;

	virtual COLORREF TextColorForSubItem(int nItem,
		int nSubItem,
		UINT nState,
		LPARAM lParam) override;

	virtual COLORREF BkColorForSubItem(int nItem,
		int nSubItem,
		UINT nState,
		LPARAM lParam) override;

	virtual CFont* FontForSubItem(int /*nItem*/,
		int /*nSubItem*/,
		UINT /*nState*/,
		LPARAM /*lParam*/) override;


public:
	int&& getSelectedRow() {
		POSITION pos = this->GetFirstSelectedItemPosition();
		return this->GetNextSelectedItem(pos);
	}

	CFont m_NewHeaderFont;
	CHeaderCtrlEx m_HeaderCtrl;

};

