#pragma once
#include <afxcmn.h>

class CHeaderCtrlEx : public CHeaderCtrl {
	// Construction
public:
	CHeaderCtrlEx();
	virtual ~CHeaderCtrlEx();
	// Attributes
public:

protected:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	DECLARE_MESSAGE_MAP()
public:
};

