
// TwainApp32.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "TwainApp32.h"
#include "TwainApp32Dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CWnd* g_pMainWnd;

// CTwainApp32App

BEGIN_MESSAGE_MAP(CTwainApp32App, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CTwainApp32App construction

CTwainApp32App::CTwainApp32App()
{
}


// The one and only CTwainApp32App object

CTwainApp32App theApp;


// CTwainApp32App initialization

BOOL CTwainApp32App::InitInstance()
{
	CWinApp::InitInstance();

	CTwainApp32Dlg dlg;
	g_pMainWnd = m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();

	// Since the dialog has been closed, return FALSE so that we exit the
	// application, rather than start the application's message pump.
	return FALSE;
}

