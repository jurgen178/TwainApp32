// TwainThread.cpp : implementation file
//

#include "stdafx.h"
#include "TwainThread.h"


int CTwainThread::m_RefCnt = 0;

extern CWnd* g_pMainWnd;
extern int PipeClient(BITMAPINFOHEADER& bmiHeader, BYTE* buf, int bufLen);
extern bool bSendData;

// CTwainThread

IMPLEMENT_DYNCREATE(CTwainThread, CWinThread)

CTwainThread::CTwainThread()
{
	++m_RefCnt;
	m_Twain.m_pThread = this;
}

CTwainThread::~CTwainThread()
{
	--m_RefCnt;
}

BOOL CTwainThread::InitInstance()
{
	bSendData = false;

	if(m_RefCnt != 1)
	{
		return FALSE;
	}

	// ParentWnd für Twain erstellen (ToolWnd, das nicht in der Taskbar erscheint)
	// Twain benötigt ein Fenster zur Bearbeitung der Nachrichten
	CRect rect(0, 0, 0, 0);

	bool bRet = m_Wnd.CreateEx(WS_EX_TOOLWINDOW, 
					AfxRegisterWndClass(CS_CLASSDC|CS_SAVEBITS|CS_HREDRAW|CS_VREDRAW,
					0,(HBRUSH) (COLOR_WINDOW), 0), 
					_T(""), WS_CHILD,
					rect, m_pMainWnd, NULL, NULL);

	m_Twain.InitTwain(m_Wnd.m_hWnd);

	if (m_Twain.SelectSource())
	{
		return m_Twain.Acquire(-1);	// -1: alle Bilder übertragen
	}
	else
	{
		return FALSE;
	}
}

int CTwainThread::ExitInstance()
{
	const int exit(CWinThread::ExitInstance());

	if (!bSendData)
	{
		// cPicture ist blockiert und wartet auf eine Antwort in der named pipe.
		BITMAPINFOHEADER bmiHeader = { 0 };
		PipeClient(bmiHeader, NULL, 0);
	}

	g_pMainWnd->EndModalLoop(0);

	return exit;
}

BEGIN_MESSAGE_MAP(CTwainThread, CWinThread)
END_MESSAGE_MAP()

BOOL CTwainThread::PreTranslateMessage(MSG* pMsg)
{
	const BOOL bWinThread = CWinThread::PreTranslateMessage(pMsg);

	if(m_Twain.SourceEnabled()) //<<<TWAIN>>>
		if(m_Twain.ProcessMessage(*pMsg))			//process twain
			return TRUE;

	return bWinThread;
}
