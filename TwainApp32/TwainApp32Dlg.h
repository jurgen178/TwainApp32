
// TwainApp32Dlg.h : header file
//

#pragma once


// CTwainApp32Dlg dialog
class CTwainApp32Dlg : public CDialogEx
{
// Construction
public:
	CTwainApp32Dlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TWAINAPP32_DIALOG };
#endif

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	void OnWindowPosChanging(WINDOWPOS* wpos);
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};
