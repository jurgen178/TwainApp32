
// TwainApp32Dlg.cpp : implementation file
//

#include "stdafx.h"
#include "TwainApp32.h"
#include "TwainApp32Dlg.h"
#include "afxdialogex.h"
#include "TwainThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTwainApp32Dlg dialog


CTwainApp32Dlg::CTwainApp32Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TWAINAPP32_DIALOG, pParent)
{
	//m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

BEGIN_MESSAGE_MAP(CTwainApp32Dlg, CDialogEx)
	ON_WM_WINDOWPOSCHANGING()
END_MESSAGE_MAP()


// CTwainApp32Dlg message handlers

BOOL CTwainApp32Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	AfxBeginThread(RUNTIME_CLASS(CTwainThread));

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CTwainApp32Dlg::OnWindowPosChanging(WINDOWPOS* wpos)
{
	wpos->flags &= ~SWP_SHOWWINDOW;
	CDialogEx::OnWindowPosChanging(wpos);
}
