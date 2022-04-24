#pragma once
// UsersView form view
class UsersView : public CFormView
{
	DECLARE_DYNCREATE(UsersView)

protected:
	UsersView();           // protected constructor used by dynamic creation
	virtual ~UsersView();

public:
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FORM_USER };
#endif
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};


