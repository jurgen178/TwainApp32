#include "stdafx.h"
#include "twainlib.h"
#include "TwainThread.h"

bool bSendData = false;

CTwain::CTwain()
  :	m_hTwainDLL(NULL),
	m_pDSMProc(NULL),
	m_bSourceSelected(FALSE),
	m_bDSOpen(FALSE),
	m_bDSMOpen(FALSE),
	m_bSourceEnabled(FALSE),
	m_bModalUI(TRUE),
	m_nImageCount(1),
	m_pThread(NULL)
{
}

CTwain::~CTwain()
{
	ReleaseTwain();
}


#define BUFSIZE 512

int PipeClient(BITMAPINFOHEADER& bmiHeader, BYTE* buf, int bufLen)
{
	HANDLE hPipe;
	BOOL   fSuccess = FALSE;
	DWORD cbWritten;
	DWORD dwMode;
	LPCTSTR lpszPipename = _T("\\\\.\\pipe\\cpictureScanPipe");

	// Try to open a named pipe; wait for it, if necessary. 

	while (1)
	{
		hPipe = CreateFile(
			lpszPipename,   // pipe name 
			GENERIC_READ |  // read and write access 
			GENERIC_WRITE,
			0,              // no sharing 
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe 
			0,              // default attributes 
			NULL);          // no template file 

	  // Break if the pipe handle is valid. 

		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		// Exit if an error other than ERROR_PIPE_BUSY occurs. 

		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			//_tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
			return -1;
		}

		// All pipe instances are busy, so wait for 2 seconds. 

		if (!WaitNamedPipe(lpszPipename, 2000))
		{
			//printf("Could not open pipe: 2 second wait timed out.");
			return -1;
		}
	}

	// The pipe connected; change to message-read mode. 

	dwMode = PIPE_READMODE_BYTE;
	fSuccess = SetNamedPipeHandleState(
		hPipe,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 
	if (!fSuccess)
	{
		//_tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
		return -1;
	}

	// Send a message to the pipe server. 

	fSuccess = WriteFile(
		hPipe,                  // pipe handle 
		&bmiHeader,
		sizeof(bmiHeader),
		&cbWritten,             // bytes written 
		NULL);                  // not overlapped 

	if (!fSuccess)
	{
		//_tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
		return -1;
	}

	fSuccess = WriteFile(
		hPipe,                  // pipe handle 
		buf,
		bufLen,
		&cbWritten,             // bytes written 
		NULL);                  // not overlapped 

	if (!fSuccess)
	{
		//_tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
		return -1;
	}

	CloseHandle(hPipe);

	return 0;
}

#define WIDTH_DWORD_ALIGNED(pixel)    (((((pixel) * 24) + 31) >> 3) & ~0x03)

void GetRGBFromHBitmap(HANDLE hBitmap, BITMAPINFOHEADER& bmiHeader, BYTE*& buf, int& bufLen)
{
	BYTE *lpVoid = (BYTE*)GlobalLock(hBitmap);
	BITMAPINFOHEADER *pbmiHeader = (BITMAPINFOHEADER*)lpVoid;
	if (lpVoid)
	{
		//copy the bitmap header
		memcpy(&bmiHeader, pbmiHeader, sizeof(BITMAPINFOHEADER));

		BYTE* pDib = NULL;
		const int uiWidthBytes3 = WIDTH_DWORD_ALIGNED(bmiHeader.biWidth);
		bufLen = uiWidthBytes3 * bmiHeader.biHeight;
		buf = new BYTE[bufLen];
		int nr = 0;

		if (bmiHeader.biCompression == BI_RGB && bmiHeader.biBitCount == 24)
		{
			nr = uiWidthBytes3 * bmiHeader.biHeight;

			pDib = new BYTE[nr];
			if (pDib == NULL)
			{
				::GlobalUnlock(lpVoid);
				delete[] buf;
				buf = NULL;
				bufLen = 0;
				return;
			}

			memcpy(buf, lpVoid + sizeof(BITMAPINFOHEADER), nr);
		}
		else
			if (bmiHeader.biCompression == BI_RGB && bmiHeader.biBitCount == 8)
			{
				const int uiWidthBytes = ((bmiHeader.biWidth * 8) + 31) / 32 * 4;
				nr = uiWidthBytes * bmiHeader.biHeight;

				pDib = new BYTE[nr];
				if (pDib == NULL)
				{
					::GlobalUnlock(lpVoid);
					delete[] buf;
					buf = NULL;
					bufLen = 0;
					return;
				}

				// Nach dem BITMAPINFOHEADER folgt die Palette (3*256, DWORD ausgerichtet, also 1024 Bytes)
				// Die Palette ist i=p[i] (identity palette), z.B. Eintrag 5 ist 5,5,5
				// Daher brauchen die Werte nicht über die Palette auf RGB-Werte umgesetzt zu werden,
				// sondern können direkt verwendet werden
				memcpy(pDib, lpVoid + sizeof(BITMAPINFOHEADER) + 1024, nr);


				nr = 0;
				int nr3 = 0;
				BYTE* dib_buf = NULL;
				BYTE* mbuf2 = NULL;

				for (unsigned int y = bmiHeader.biHeight; y != 0; --y)
				{
					// vertical flip
					// write from the end <-> read from the beginning
					dib_buf = pDib + nr;
					mbuf2 = buf + nr3;

					for (register unsigned int x = bmiHeader.biWidth; x != 0; --x)
					{
						const BYTE b = *dib_buf;
						++dib_buf;
						*mbuf2 = b;
						++mbuf2;
						*mbuf2 = b;
						++mbuf2;
						*mbuf2 = b;
						++mbuf2;
					}
					nr += uiWidthBytes;	// always start at DWORD boundary
					nr3 += uiWidthBytes3;
				}

				bmiHeader.biBitCount = 24;
			}
			else
			{
				::GlobalUnlock(lpVoid);
				delete[] buf;
				buf = NULL;
				bufLen = 0;
				return;
			}


		delete[] pDib;

		::GlobalUnlock(lpVoid);
	}
}

void CTwain::SetImage(HANDLE hBitmap, TW_IMAGEINFO& info)
{
	BYTE* buf = NULL;
	int bufLen = 0;
	BITMAPINFOHEADER bmiHeader = { 0 };

	GetRGBFromHBitmap(hBitmap, bmiHeader, buf, bufLen);
	PipeClient(bmiHeader, buf, bufLen);
	bSendData = true;

	delete[] buf;

	if (CanClose())
		CloseDS();

	if (m_pThread)
		m_pThread->PostThreadMessage(WM_QUIT, 0, 0);
}


/*
Initializes TWAIN interface . Is already called from the constructor. 
It should be called again if ReleaseTwain is called.

  hWnd is the window which has to subclassed in order to receive
  Twain messages. Normally - this would be your main application window.

*/
BOOL CTwain::InitTwain(HWND hWnd)
{
	if(IsValidDriver()) 
		return TRUE;

	memset(&m_AppId,0,sizeof(m_AppId));
	if(!IsWindow(hWnd))
		return FALSE;

	m_hMessageWnd = hWnd;
	
	m_hTwainDLL = LoadLibraryA("TWAIN_32.DLL");

	if(m_hTwainDLL != NULL)
		if(!(m_pDSMProc = (DSMENTRYPROC)GetProcAddress(m_hTwainDLL, MAKEINTRESOURCEA(1))))
		{
			FreeLibrary(m_hTwainDLL);
			m_hTwainDLL = NULL;
		}

	if(IsValidDriver())
	{
		// Expects all the fields in m_AppId to be set except for the id field.
		m_AppId.Id = 0; // Initialize to 0 (Source Manager
		// will assign real value)
		m_AppId.ProtocolMajor = TWON_PROTOCOLMAJOR;
		m_AppId.ProtocolMinor = TWON_PROTOCOLMINOR;
		m_AppId.SupportedGroups = DG_IMAGE | DG_CONTROL;
		
		m_bDSMOpen = CallTwainProc(&m_AppId,NULL,DG_CONTROL,DAT_PARENT,MSG_OPENDSM,(TW_MEMREF)&m_hMessageWnd);
		return TRUE;
	}
	else
	{
		//AfxMessageBox(IDS_TWAIN_LIB_NOT_LOADED);
		return FALSE;
	}
}

/*
Releases the twain interface . Need not be called unless you
want to specifically shut it down.
*/
void CTwain::ReleaseTwain()
{
	if(IsValidDriver())
	{
		CloseDSM();
		FreeLibrary(m_hTwainDLL);
		m_hTwainDLL = NULL;
		m_pDSMProc = NULL;
	}
}

/*
Returns true if a valid driver has been loaded
*/
BOOL CTwain::IsValidDriver() const
{
	return (m_hTwainDLL && m_pDSMProc);
}
/*
* Fucntion: CallDSMEntry
* Author:	Nancy Letourneau / J.F.L. Peripherals Inc.
* Input:  
*		Function - 
*		pApp - 
*		pSrc - 
*		DG -
*		DAT -
*		MSG -
*		pData -
* Output: 
*		TW_UINT16 - Value of Item field of container. 
* Comments:
*
*/
TW_UINT16 CTwain::CallDSMEntry(pTW_IDENTITY pApp, pTW_IDENTITY pSrc,
										TW_UINT32 DG, TW_UINT16 DAT, TW_UINT16 MSG, TW_MEMREF pData)
{
	TW_UINT16 twRC = (*m_pDSMProc)(pApp, pSrc, DG, DAT, MSG, pData);

	if((twRC != TWRC_SUCCESS)&&(DAT!=DAT_EVENT))
	{
		VERIFY((*m_pDSMProc)(pApp, pSrc, DG_CONTROL, DAT_STATUS, MSG_GET, 
					(TW_MEMREF)&m_Status) == TWRC_SUCCESS);
		TRACE("CallDSMEntry function: call failed with RC = %d, CC = %d.\n", 
					twRC, m_Status);
	}
	return twRC;
}
/*
Entry point into Twain. For a complete description of this
routine  please refer to the Twain specification 1.8
*/
BOOL CTwain::CallTwainProc(pTW_IDENTITY pOrigin,pTW_IDENTITY pDest,
					   TW_UINT32 DG,TW_UINT16 DAT,TW_UINT16 MSG,
					   TW_MEMREF pData)
{
	if(IsValidDriver())
	{
		const USHORT ret_val = (*m_pDSMProc)(pOrigin,pDest,DG,DAT,MSG,pData);

		m_returnCode = ret_val;
		if(ret_val == TWRC_FAILURE)
		{
			(*m_pDSMProc)(pOrigin,pDest,DG_CONTROL,DAT_STATUS,MSG_GET,&m_Status);
		}

		return (ret_val == TWRC_SUCCESS);
	}
	else
	{
		m_returnCode = TWRC_FAILURE;
		return FALSE;
	}
}

/*
Called to display a dialog box to select the Twain source to use.
This can be overridden if a list of all sources is available
to the application. These sources can be enumerated by Twain.
it is not yet supportted by CTwain.
*/
BOOL CTwain::SelectSource()
{
	memset(&m_Source, 0, sizeof(m_Source));
	if(!SourceSelected())
		SelectDefaultSource();

	return m_bSourceSelected = CallTwainProc(&m_AppId,NULL,DG_CONTROL,DAT_IDENTITY,MSG_USERSELECT,&m_Source);
}

/*
Called to select the default source
*/
BOOL CTwain::SelectDefaultSource()
{
	return m_bSourceSelected = CallTwainProc(&m_AppId,NULL,DG_CONTROL,DAT_IDENTITY,MSG_GETDEFAULT,&m_Source);
}

/*
Closes the Data Source
*/
void CTwain::CloseDS()
{
	if(DSOpen())
	{
		DisableSource();
		CallTwainProc(&m_AppId,NULL,DG_CONTROL,DAT_IDENTITY,MSG_CLOSEDS,(TW_MEMREF)&m_Source);
		m_bDSOpen = FALSE;
	}
}

/*
Closes the Data Source Manager
*/
void CTwain::CloseDSM()
{
	if(DSMOpen())
	{
		CloseDS();
		CallTwainProc(&m_AppId,NULL,DG_CONTROL,DAT_PARENT,MSG_CLOSEDSM,(TW_MEMREF)&m_hMessageWnd);
		m_bDSMOpen = FALSE;
	}
}

/*
Returns true if the Data Source Manager is Open
*/
BOOL CTwain::DSMOpen() const
{
	return IsValidDriver() && m_bDSMOpen;
}

/*
Returns true if the Data Source is Open
*/
BOOL CTwain::DSOpen() const
{
	return IsValidDriver() && DSMOpen() && m_bDSOpen;
}

/*
Opens a Data Source supplied as the input parameter
*/
BOOL CTwain::OpenSource(TW_IDENTITY *pSource)
{
	if(pSource) 
	{
		m_Source = *pSource;
	}
	if(DSMOpen())
	{
		if(!SourceSelected())
		{
			SelectDefaultSource();
		}
		m_bDSOpen = CallTwainProc(&m_AppId,NULL,DG_CONTROL,DAT_IDENTITY,MSG_OPENDS,(TW_MEMREF)&m_Source);
	}
	return DSOpen();
}

/*
Should be called from the main message loop of the application. Can always be called,
it will not process the message unless a scan is in progress.
*/

// The NEW ProcessMessage() function.
BOOL CTwain::ProcessMessage(MSG msg)
{
//typedef struct {
//   TW_MEMREF  pEvent;    /* Windows pMSG or Mac pEvent.                 */
//   TW_UINT16  TWMessage; /* TW msg from data source, e.g. MSG_XFERREADY */
//} TW_EVENT, FAR * pTW_EVENT;

	TW_EVENT twEvent;
	twEvent.pEvent = (TW_MEMREF)&msg;
	twEvent.TWMessage = MSG_NULL;

	CallTwainProc(&m_AppId,&m_Source,DG_CONTROL,DAT_EVENT,MSG_PROCESSEVENT,(TW_MEMREF)&twEvent);
	TW_UINT16 twRC = GetRC();
	if(twRC != TWRC_NOTDSEVENT)
	{
		TranslateMessage(twEvent);
		return TRUE;
	}

	return twRC == TWRC_DSEVENT;
}

/*
Called by ProcessMessage to Translate a TWAIN message
*/
void CTwain::TranslateMessage(TW_EVENT& twEvent)
{
	switch(twEvent.TWMessage)
	{
	case MSG_XFERREADY:
		TransferImage();
		break;
	case MSG_CLOSEDSREQ:
		if(CanClose())
			CloseDS();
		if(m_pThread)
			m_pThread->PostThreadMessage(WM_QUIT, 0, 0);
		break;

	// No message from the Source to the App break;
	// possible new message
	case MSG_NULL:
	default:
		break;
	}
}

/*
Queries the capability of the Twain Data Source
*/
BOOL CTwain::GetCapability(TW_CAPABILITY& twCap,TW_UINT16 cap,TW_UINT16 conType)
{
	if(DSOpen())
	{
		twCap.Cap = cap;
		twCap.ConType = conType;
		twCap.hContainer = NULL;

		if(CallTwainProc(&m_AppId,&m_Source,DG_CONTROL,DAT_CAPABILITY,MSG_GET,(TW_MEMREF)&twCap))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/*
Queries the capability of the Twain Data Source
*/
BOOL CTwain::GetCapability(TW_UINT16 cap,TW_UINT32& value)
{
TW_CAPABILITY twCap;
	if(GetCapability(twCap,cap))
	{
	pTW_ONEVALUE pVal;
		pVal = (pTW_ONEVALUE )GlobalLock(twCap.hContainer);
		if(pVal)
		{
			value = pVal->Item;
			GlobalUnlock(pVal);
			GlobalFree(twCap.hContainer);
			return TRUE;
		}
	}
	return FALSE;
}


/*
Sets the capability of the Twain Data Source
*/
BOOL CTwain::SetCapability(TW_UINT16 cap,TW_UINT16 value,BOOL sign)
{
	if(DSOpen())
	{
		TW_CAPABILITY twCap;
		pTW_ONEVALUE pVal;
		BOOL ret_value = FALSE;

		twCap.Cap = cap;
		twCap.ConType = TWON_ONEVALUE;
		
		twCap.hContainer = GlobalAlloc(GHND,sizeof(TW_ONEVALUE));
		if(twCap.hContainer)
		{
			pVal = (pTW_ONEVALUE)GlobalLock(twCap.hContainer);
			pVal->ItemType = sign ? TWTY_INT16 : TWTY_UINT16;
			pVal->Item = (TW_UINT32)value;
			GlobalUnlock(twCap.hContainer);
			ret_value = SetCapability(twCap);
			GlobalFree(twCap.hContainer);
		}
		return ret_value;
	}
	return FALSE;
}

/*
Sets the capability of the Twain Data Source
*/
BOOL CTwain::SetCapability(TW_CAPABILITY& cap)
{
	if(DSOpen())
	{
		return CallTwainProc(&m_AppId,&m_Source,DG_CONTROL,DAT_CAPABILITY,MSG_SET,(TW_MEMREF)&cap);
	}
	return FALSE;
}

/*
Sets the number of images which can be accpeted by the application at one time
*/
BOOL CTwain::SetImageCount(TW_INT16 nCount)
{
	if(SetCapability(CAP_XFERCOUNT,(TW_UINT16)nCount,TRUE))
	{
		m_nImageCount = nCount;
		return TRUE;
	}
	else
	{
		if(GetRC() == TWRC_CHECKSTATUS)
		{
		TW_UINT32 count;
			if(GetCapability(CAP_XFERCOUNT,count))
			{
				nCount = (TW_INT16)count;
				if(SetCapability(CAP_XFERCOUNT,nCount))
				{
					m_nImageCount = nCount;
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/*
Called to enable the Twain Acquire Dialog. This too can be
overridden but is a helluva job . 
*/
BOOL CTwain::EnableSource(BOOL showUI)
{
	if(DSOpen() && !SourceEnabled())
	{
		TW_USERINTERFACE twUI;
		twUI.ShowUI = showUI;
//		twUI.ModalUI = FALSE;
		twUI.hParent = (TW_HANDLE)m_hMessageWnd;
		if(CallTwainProc(&m_AppId,&m_Source,DG_CONTROL,DAT_USERINTERFACE,MSG_ENABLEDS,(TW_MEMREF)&twUI))
		{
			m_bSourceEnabled = TRUE;
			m_bModalUI = twUI.ModalUI;
		}
		else
		{
			m_bSourceEnabled = FALSE;
			m_bModalUI = TRUE;
		}
		return m_bSourceEnabled;
	}
	return FALSE;
}

/*
Called to acquire images from the source. parameter numImages i the
numberof images that you an handle concurrently
*/
BOOL CTwain::Acquire(int numImages)
{
	if(DSOpen() || OpenSource())
	{
		SetCapability(ICAP_PIXELTYPE, (TW_UINT16)TWPT_GRAY);
		SetCapability(ICAP_PIXELTYPE, (TW_UINT16)TWPT_RGB);
		SetCapability(ICAP_BITDEPTH, (TW_UINT16)24);
		SetCapability(ICAP_BITDEPTH, (TW_UINT16)8);
		//setData(ICAP_PIXELTYPE,true,TWPT_GRAY,0.1);
		//setData(ICAP_BITDEPTH,true,8,0.1);

		if(SetImageCount(numImages))
		{
			if(EnableSource())	// Dialog vom Scanner mit der Bereichsauswahl und Farbtiefe anzeigen
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/*
 Called to disable the source.
*/
BOOL CTwain::DisableSource()
{
	if(SourceEnabled())
	{
	TW_USERINTERFACE twUI;
		if(CallTwainProc(&m_AppId,&m_Source,DG_CONTROL,DAT_USERINTERFACE,MSG_DISABLEDS,&twUI))
		{
			m_bSourceEnabled = FALSE;
			return TRUE;
		}
	}
	return FALSE;
}

/*
Gets Imageinfo for an image which is about to be transferred.
*/
BOOL CTwain::GetImageInfo(TW_IMAGEINFO& info)
{
	if(SourceEnabled())
	{
		return CallTwainProc(&m_AppId,&m_Source,DG_IMAGE,DAT_IMAGEINFO,MSG_GET,(TW_MEMREF)&info);
	}
	return FALSE;
}

/*
Transfers the image or cancels the transfer depending on the state of the
TWAIN system
*/
void CTwain::TransferImage()
{
	TW_IMAGEINFO info;
	BOOL bContinue=TRUE;
	while(bContinue)
	{
		if(GetImageInfo(info))
		{
			int permission;
			permission = ShouldTransfer(info);
			switch(permission)
			{
			case TWCPP_CANCELTHIS:
					bContinue=EndTransfer();
					break;
			case TWCPP_CANCELALL:
					CancelTransfer();
					bContinue=FALSE;
					break;
			case TWCPP_DOTRANSFER:
					bContinue=GetImage(info);
					break;
			}
		}
	}
}

/*
Ends the current transfer.
Returns TRUE if the more images are pending
*/
BOOL CTwain::EndTransfer()
{
TW_PENDINGXFERS twPend;
	if(CallTwainProc(&m_AppId,&m_Source,DG_CONTROL,DAT_PENDINGXFERS,MSG_ENDXFER,(TW_MEMREF)&twPend))
	{
		return twPend.Count != 0;
	}
	return FALSE;
}

/*
Aborts all transfers
*/
void CTwain::CancelTransfer()
{
	TW_PENDINGXFERS twPend;
	CallTwainProc(&m_AppId,&m_Source,DG_CONTROL,DAT_PENDINGXFERS,MSG_RESET,(TW_MEMREF)&twPend);
}

/*
Calls TWAIN to actually get the image
*/
BOOL CTwain::GetImage(TW_IMAGEINFO& info)
{
	HANDLE hBitmap;
	CallTwainProc(&m_AppId,&m_Source,DG_IMAGE,DAT_IMAGENATIVEXFER,MSG_GET,&hBitmap);
	switch(m_returnCode)
	{
	case TWRC_XFERDONE:
			SetImage(hBitmap,info);
			break;
	case TWRC_CANCEL:
			break;
	case TWRC_FAILURE:
			CancelTransfer();
			return FALSE;

	}
	GlobalFree(hBitmap);
	return EndTransfer();
}
