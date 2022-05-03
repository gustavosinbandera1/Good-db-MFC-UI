// CAddress.cpp : implementation file
//

#include "pch.h"
#include "Goods-Database-MFC.h"
#include "CAddress.h"
#include "afxdialogex.h"


// CAddress dialog

IMPLEMENT_DYNAMIC(CAddress, CDialog)

CAddress::CAddress(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DIALOG_ADD_ADDRESS, pParent) {

}

CAddress::~CAddress()
{
}

void CAddress::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_COUNTRY, combo_country);
	DDX_Control(pDX, IDC_COMBO_CITY, combo_city);
	DDX_Control(pDX, IDC_COMBO_STATE, combo_state);
	DDX_Control(pDX, IDC_COMBO_TYPE, combo_type);
}


BEGIN_MESSAGE_MAP(CAddress, CDialog)
	ON_CBN_SELCHANGE(IDC_COMBO_COUNTRY, &CAddress::OnCountryChange)
	ON_CBN_SELCHANGE(IDC_COMBO_CITY, &CAddress::OnCityChange)
	ON_CBN_SELCHANGE(IDC_COMBO_STATE, &CAddress::OnStateChange)
	ON_CBN_SELCHANGE(IDC_COMBO_TYPE, &CAddress::OnTypeChange)
END_MESSAGE_MAP()


// CAddress message handlers

void CAddress::OnCountryChange() {
  // TODO: Add your control notification handler code here
}

void CAddress::OnCityChange() {
  // TODO: Add your control notification handler code here
}

void CAddress::OnStateChange() {
  // TODO: Add your control notification handler code here
}

void CAddress::OnTypeChange() {
  // TODO: Add your control notification handler code here
}


BOOL CAddress::OnInitDialog()
{
	CDialog::OnInitDialog();
	populateOptions();
	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CAddress::populateOptions() {
	CString str;
	for (int i = 0; i < 10; i++) {
		str.Format(_T("Country %d"), i);
		combo_country.AddString(str);
	}

	for (int i = 0; i < 10; i++) {
		str.Format(_T("City %d"), i);
		combo_city.AddString(str);
	}

	for (int i = 0; i < 10; i++) {
		str.Format(_T("State %d"), i);
		combo_state.AddString(str);
	}

	for (int i = 0; i < 3; i++) {
		str.Format(_T("type %d"), i);
		combo_type.AddString(str);
	}
}
