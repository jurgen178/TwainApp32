
// TwainApp32.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CTwainApp32App:
// See TwainApp32.cpp for the implementation of this class
//

class CTwainApp32App : public CWinApp
{
public:
	CTwainApp32App();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CTwainApp32App theApp;
