#pragma once
#include <afxwin.h>


class CInPlaceEdit : public CEdit
{

public:
	CInPlaceEdit(int iItem, int iSubItem, CString sInitText);
	
public:	virtual BOOL PreTranslateMessage(MSG* pMsg);

public:	virtual ~CInPlaceEdit();

protected:	
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnNcDestroy();
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

	DECLARE_MESSAGE_MAP()

private:
	int m_iItem;
	int m_iSubItem;
	CString m_sInitText;
	BOOL    m_bESC;
};