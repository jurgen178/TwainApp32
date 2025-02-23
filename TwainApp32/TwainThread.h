#pragma once
#include "twainlib.h"

// CTwainThread

class CTwainThread : public CWinThread
{
	DECLARE_DYNCREATE(CTwainThread)

protected:
	CTwainThread();           // protected constructor used by dynamic creation
	virtual ~CTwainThread();

protected:
	CWnd m_Wnd;
	CTwain m_Twain;

public:
	static int m_RefCnt;

public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

protected:
	DECLARE_MESSAGE_MAP()
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};


