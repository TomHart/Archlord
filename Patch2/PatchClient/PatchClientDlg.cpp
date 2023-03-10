// PatchClientDlg.cpp : implementation file
//

#include "stdafx.h"
#include "GetDisplayInfo.h"
#include "PatchClient.h"
#include "PatchClientDlg.h"
#include "Winbase.h"
#include "mmsystem.h"
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#include <fstream>

#include "d3d9.h"

#include "ApModule.h"
#include "ApModuleStream.h"
#include "DriverDownDlg.h"
#include ".\patchclientdlg.h"

#include "hyperlinks.h"			//. 외부 static text hyperlink 함수 사용
#include "XmlLogger/XmlLogger.h"

#ifdef _CHN
	#include "ChinaPatchClient.h"
#endif

#ifdef _JPN
	#include "JapanPatchClient.h"
#endif

#ifdef _KOR
	#include "KoreaPatchClient.h"
#endif

#ifdef _TIW
	#include "TaiwanPatchClient.h"
#endif

#ifdef _ENG
	#include "EnglishPatchClient.h"
#endif

#include "LangControl.h"
#include "AutoDetectMemoryLeak.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define PATCH_AREA_LIST_REFRESH_DELAY	1000

#ifdef _TIW
wchar_t* g_szMessageTitle = L"帝王戰記";
#else
wchar_t* g_szMessageTitle = L"Archlord";
#endif

const UINT g_uiListScrollPosx = 263;
const UINT g_uiListScrollMin = 310;
const UINT g_uiListScrollMax = 420;
const UINT g_uiListScrollSize = 110;

const ULONG_PTR JpnCookieTimer = 100;
const ULONG_PTR ChnTimer = 1;
const ULONG_PTR korTimer = 100;

#define ENABLE_START

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	enum { IDD = IDD_ABOUTBOX };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{

}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


CPatchClientDlg::CPatchClientDlg(CWnd* pParent /*=NULL*/)
: CDialog(CPatchClientDlg::IDD, pParent)
{
	
#ifdef _KOR
	m_pPatchClient	=	new	KoreaPatchClient(this);
#elif _CHN
	m_pPatchClient	=	new ChinaPatchClient(this);
#elif _JPN
	m_pPatchClient	=	new JapanPatchClient(this);
#elif _ENG
	m_pPatchClient	=	new EnglishPatchClient(this);
#elif _TIW
	m_pPatchClient	=	new TaiwanPatchClient(this);
#elif _TEST_SERVER
	m_pPatchClient	=	new TestPatchClient(this);
#endif 

	
	
#ifndef _JPN
	if (strstr( GetCommandLine() , PATCHCLIENT_OPTION_HIGH))				// High Quality Option
	{
		m_pPatchClient->SetStartupMode( AGCM_OPTION_STARTUP_HIGH );
	}
	else if (strstr( GetCommandLine() , PATCHCLIENT_OPTION_LOW))			// Low Quality Option
	{
		m_pPatchClient->SetStartupMode( AGCM_OPTION_STARTUP_LOW );
	}

	else if (strstr( GetCommandLine() , PATCHCLIENT_OPTION_NOSAVE))				// No Save Option
	{
		m_pPatchClient->SetStartupMode( AGCM_OPTION_STARTUP_LOW );
	}
#endif

	//. 패치완료 후 강제 종료하기 옵션추가.
	if (strstr( GetCommandLine() , PATCHCLIENT_OPTION_FORCEEXIT))
	{
		m_pPatchClient->SetForceExit( TRUE );
	}
}

CPatchClientDlg::~CPatchClientDlg()
{
	if ( g_pLogger )		delete g_pLogger;
}

void CPatchClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange( pDX );
	m_pPatchClient->DoDataExchange( pDX );
}

BEGIN_MESSAGE_MAP(CPatchClientDlg, CDialog)
	//{{AFX_MSG_MAP(CPatchClientDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_EXitButton, OnEXitButton)
	ON_BN_CLICKED(IDC_START_GAME, OnStartGame)
	ON_BN_CLICKED(IDC_RegisterButton, OnRegisterButton)
	ON_BN_CLICKED(IDC_OptionButton, OnOptionButton)
	ON_BN_CLICKED(IDC_HomepageButton, OnHomepageButton)
	ON_WM_MOUSEMOVE()
	ON_WM_CTLCOLOR()
	ON_WM_KEYDOWN()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_SeverSelect1, OnBnClickedSeverselect1)
	ON_BN_CLICKED(IDC_SeverSelect2, OnBnClickedSeverselect2)
	ON_BN_CLICKED(IDC_SeverSelect3, OnBnClickedSeverselect3)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_MenuBtn1, OnBnClickedMenubtn1)
	ON_BN_CLICKED(IDC_MenuBtn2, OnBnClickedMenubtn2)
	ON_BN_CLICKED(IDC_MenuBtn3, OnBnClickedMenubtn3)
	ON_BN_CLICKED(IDC_MenuBtn4, OnBnClickedMenubtn4)
	ON_BN_CLICKED(IDC_MenuBtn5, OnBnClickedMenubtn5)
	ON_WM_NCHITTEST()
	ON_BN_CLICKED(IDC_BUTTON_MORE, &CPatchClientDlg::OnBnClickedButtonMore)
	ON_MESSAGE( WM_USER+1 , OnMessageSelfPatch )

	ON_WM_ERASEBKGND()
	ON_LBN_SELCHANGE(IDC_ServerList, &CPatchClientDlg::OnLbnSelchangeServerlist)
END_MESSAGE_MAP()


HRGN BitmapToRegion (HBITMAP hBmp, COLORREF cTransparentColor, COLORREF cTolerance)
{
	HRGN hRgn = NULL;

	if (hBmp)
	{
		// Create a memory DC inside which we will scan the bitmap content
		HDC hMemDC = ::CreateCompatibleDC(NULL);
		if (hMemDC)
		{
			// Get bitmap size
			BITMAP bm;
			::GetObject(hBmp, sizeof(bm), &bm);

			// Create a 32 bits depth bitmap and select it into the memory DC 
			BITMAPINFOHEADER RGB32BITSBITMAPINFO = { 
				sizeof(BITMAPINFOHEADER), // biSize 
				bm.bmWidth,     // biWidth; 
				bm.bmHeight,    // biHeight; 
				1,       // biPlanes; 
				32,       // biBitCount 
				BI_RGB,      // biCompression; 
				0,       // biSizeImage; 
				0,       // biXPelsPerMeter; 
				0,       // biYPelsPerMeter; 
				0,       // biClrUsed; 
				0       // biClrImportant; 
			};
			VOID * pbits32; 
			HBITMAP hbm32 = ::CreateDIBSection(hMemDC, (BITMAPINFO *)&RGB32BITSBITMAPINFO, DIB_RGB_COLORS, &pbits32, NULL, 0);
			if (hbm32)
			{
				HBITMAP holdBmp = (HBITMAP)::SelectObject(hMemDC, hbm32);

				// Create a DC just to copy the bitmap into the memory DC
				HDC hDC = ::CreateCompatibleDC(hMemDC);
				if (hDC)
				{
					// Get how many bytes per row we have for the bitmap bits (rounded up to 32 bits)
					BITMAP bm32;
					::GetObject(hbm32, sizeof(bm32), &bm32);
					while (bm32.bmWidthBytes % 4)
						bm32.bmWidthBytes++;

					// Copy the bitmap into the memory DC
					HBITMAP holdBmp = (HBITMAP)::SelectObject(hDC, hBmp);
					::BitBlt(hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, hDC, 0, 0, SRCCOPY);

					// For better performances, we will use the ExtCreateRegion() function to create the
					// region. This function take a RGNDATA structure on entry. We will add rectangles by
					// amount of ALLOC_UNIT number in this structure.
#define ALLOC_UNIT 100
					DWORD maxRects = ALLOC_UNIT;
					HANDLE hData = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(RGNDATAHEADER) + (sizeof(RECT) * maxRects));
					RGNDATA *pData = (RGNDATA *)::GlobalLock(hData);
					pData->rdh.dwSize = sizeof(RGNDATAHEADER);
					pData->rdh.iType = RDH_RECTANGLES;
					pData->rdh.nCount = pData->rdh.nRgnSize = 0;
					::SetRect(&pData->rdh.rcBound, MAXLONG, MAXLONG, 0, 0);

					// Keep on hand highest and lowest values for the "transparent" pixels
					BYTE lr = GetRValue(cTransparentColor);
					BYTE lg = GetGValue(cTransparentColor);
					BYTE lb = GetBValue(cTransparentColor);
					BYTE hr = min(0xff, lr + GetRValue(cTolerance));
					BYTE hg = min(0xff, lg + GetGValue(cTolerance));
					BYTE hb = min(0xff, lb + GetBValue(cTolerance));

					// Scan each bitmap row from bottom to top (the bitmap is inverted vertically)
					BYTE *p32 = (BYTE *)bm32.bmBits + (bm32.bmHeight - 1) * bm32.bmWidthBytes;
					for (int y = 0; y < bm.bmHeight; y++)
					{
						// Scan each bitmap pixel from left to right
						for (int x = 0; x < bm.bmWidth; x++)
						{
							// Search for a continuous range of "non transparent pixels"
							int x0 = x;
							LONG *p = (LONG *)p32 + x;
							while (x < bm.bmWidth)
							{
								BYTE b = GetRValue(*p);
								if (b >= lr && b <= hr)
								{
									b = GetGValue(*p);
									if (b >= lg && b <= hg)
									{
										b = GetBValue(*p);
										if (b >= lb && b <= hb)
											// This pixel is "transparent"
											break;
									}
								}
								p++;
								x++;
							}

							if (x > x0)
							{
								// Add the pixels (x0, y) to (x, y+1) as a new rectangle in the region
								if (pData->rdh.nCount >= maxRects)
								{
									::GlobalUnlock(hData);
									maxRects += ALLOC_UNIT;
									hData = ::GlobalReAlloc(hData, sizeof(RGNDATAHEADER) + (sizeof(RECT) * maxRects), GMEM_MOVEABLE);
									pData = (RGNDATA *)::GlobalLock(hData);
								}
								RECT *pr = (RECT *)&pData->Buffer;
								::SetRect(&pr[pData->rdh.nCount], x0, y, x, y+1);
								if (x0 < pData->rdh.rcBound.left)
									pData->rdh.rcBound.left = x0;
								if (y < pData->rdh.rcBound.top)
									pData->rdh.rcBound.top = y;
								if (x > pData->rdh.rcBound.right)
									pData->rdh.rcBound.right = x;
								if (y+1 > pData->rdh.rcBound.bottom)
									pData->rdh.rcBound.bottom = y+1;
								pData->rdh.nCount++;

								// On Windows98, ExtCreateRegion() may fail if the number of rectangles is too
								// large (ie: > 4000). Therefore, we have to create the region by multiple steps.
								if (pData->rdh.nCount == 2000)
								{
									HRGN h = ::ExtCreateRegion(NULL, sizeof(RGNDATAHEADER) + (sizeof(RECT) * maxRects), pData);
									if (hRgn)
									{
										::CombineRgn(hRgn, hRgn, h, RGN_OR);
										::DeleteObject(h);
									}
									else
										hRgn = h;
									pData->rdh.nCount = 0;
									::SetRect(&pData->rdh.rcBound, MAXLONG, MAXLONG, 0, 0);
								}
							}
						}

						// Go to next row (remember, the bitmap is inverted vertically)
						p32 -= bm32.bmWidthBytes;
					}

					// Create or extend the region with the remaining rectangles
					HRGN h = ::ExtCreateRegion(NULL, sizeof(RGNDATAHEADER) + (sizeof(RECT) * maxRects), pData);
					if (hRgn)
					{
						::CombineRgn(hRgn, hRgn, h, RGN_OR);
						::DeleteObject(h);
					}
					else
						hRgn = h;

					// Clean up
					::GlobalFree(hData);
					::SelectObject(hDC, holdBmp);
					::DeleteDC(hDC);
				}

				::DeleteObject(::SelectObject(hMemDC, holdBmp));
			}

			::DeleteDC(hMemDC);
		} 
	}

	return hRgn;
}


BOOL CPatchClientDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_pPatchClient->OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	SetTimer( 5 , 500 , NULL );

	
	return TRUE;
}

void CPatchClientDlg::PatchThreadStart()
{
	m_pPatchClient->PatchThreadStart();
}


void CPatchClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

void CPatchClientDlg::OnPaint() 
{	
	m_pPatchClient->OnPaint();

	if (IsIconic())
	{
		CPaintDC	dc(this);

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);

		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

HCURSOR CPatchClientDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CPatchClientDlg::OnEXitButton() 
{
	CloseDlg();
	OnCancel();	
}

void CPatchClientDlg::CloseDlg()
{
	if ( g_pLogger )
	{
		delete g_pLogger;
		g_pLogger = NULL;
	}

	m_pPatchClient->Destroy();

}

BOOL CPatchClientDlg::PreTranslateMessage(MSG* pMsg) 
{
	if( pMsg->message == WM_KEYDOWN )
	{
		if( (pMsg->wParam == VK_RETURN) || (pMsg->wParam == VK_ESCAPE) )
		{
			return FALSE;
		}
	}

	return CDialog::PreTranslateMessage(pMsg);
}

// 스타트 버튼을 눌렀다..
void CPatchClientDlg::OnStartGame() 
{
	ifstream file;

	// 스타트버튼을 다시 누르지 못하도록 하기 위해서 버튼을 비활성 처리한다.
	CButton* pBtnStart = ( CButton* )GetDlgItem( IDC_START_GAME );
	if( !pBtnStart ) return;

	file.open( "NotCheckPatchCode.arc" );

	pBtnStart->EnableWindow( FALSE );

#ifdef _ENG
	// CRC 체크를 안함
	( ( EnglishPatchClient* )m_pPatchClient )->DoStartGame();
#else
	// CRC 체크
	m_pPatchClient->OnStartGame();
#endif

	file.close();
	OnClose();
	OnOK();
}

void CPatchClientDlg::OnRegisterButton() 
{
	// 등록 관련 홈피를 연다.
	ShellExecute(NULL, _T("open"), _T("IEXPLORE.EXE"), _T("http://member.hangame.com/register/index.nhn"), NULL, SW_SHOW);
}

void CPatchClientDlg::OnOptionButton() 
{
	CPatchClientOptionDlg	OptionDlg( this );

	OptionDlg.SetOptionFile( m_pPatchClient->GetPatchOptionFile() );
	OptionDlg.DoModal();
}

HBITMAP	CPatchClientDlg::LoadBitmapResource( const char* strFileName )
{
	return m_pPatchClient->LoadBitmapResource( strFileName );
}

void CPatchClientDlg::OnHomepageButton() 
{
	//아크로드 티져사이트를 연다.
	ShellExecute( NULL, _T("open"), _T("IEXPLORE.EXE"), _T("http://www.archlord.com"), NULL, SW_SHOW );
}

HBRUSH CPatchClientDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	UINT nID = pWnd->GetDlgCtrlID();

	if( nID == IDC_Static_Status )
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor( RGB( 195, 195, 195 ) );
		return (HBRUSH)::GetStockObject(NULL_BRUSH);         
	}
	else if( nID == IDC_Static_DetailInfo )
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor( RGB( 195, 195, 195 ) );

		return (HBRUSH)::GetStockObject(NULL_BRUSH);         
	}
	else if( nID == IDC_Static_Progress_Percent )
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor( RGB( 195, 195, 195 ) );

		return (HBRUSH)::GetStockObject(NULL_BRUSH);         
	}
	else if( nID == IDC_HOMEPAGE )
	{
		pDC->SetBkMode(TRANSPARENT);

		return (HBRUSH)::GetStockObject(NULL_BRUSH);         
	}
	else if( nID == IDC_JOIN )
	{
		pDC->SetBkMode(TRANSPARENT);

		return (HBRUSH)::GetStockObject(NULL_BRUSH);         
	}
	else if( nID == IDC_CUSTOMER )
	{
		pDC->SetBkMode(TRANSPARENT);

		return (HBRUSH)::GetStockObject(NULL_BRUSH);         
	}
	else if( nID == IDC_DRIVER )
	{
		pDC->SetBkMode(TRANSPARENT);

		return (HBRUSH)::GetStockObject(NULL_BRUSH);         
	}
	else if( nID == IDC_LINE )
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor( RGB( 137, 137, 137) );
		return (HBRUSH)::GetStockObject(NULL_BRUSH);         
	}

	return hbr;
}


void CPatchClientDlg::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CDialog::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CPatchClientDlg::OnClose() 
{
	CloseDlg();
	CDialog::OnClose();
}

void CPatchClientDlg::OnBnClickedSeverselect1()
{
	m_pPatchClient->OnBnClickedServer1();
}

void CPatchClientDlg::OnBnClickedSeverselect2()
{
	m_pPatchClient->OnBnClickedServer2();
}

void CPatchClientDlg::OnBnClickedSeverselect3()
{
	m_pPatchClient->OnBnClickedServer3();
}

void CPatchClientDlg::OnTimer(UINT nIDEvent)
{
	m_pPatchClient->OnTimer( nIDEvent );
	CDialog::OnTimer(nIDEvent);
}

void CPatchClientDlg::OnBnClickedMenubtn1()
{
	m_pPatchClient->OnBnClickedMenu1();
}

void CPatchClientDlg::OnBnClickedMenubtn2()
{
	m_pPatchClient->OnBnClickedMenu2();
}

void CPatchClientDlg::OnBnClickedMenubtn3()
{
	m_pPatchClient->OnBnClickedMenu3();
}

void CPatchClientDlg::OnBnClickedMenubtn4()
{
	m_pPatchClient->OnBnClickedMenu4();
}

void CPatchClientDlg::OnBnClickedMenubtn5()
{
	m_pPatchClient->OnBnClickedMenu5();
}

LRESULT CPatchClientDlg::OnNcHitTest(CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.

	UINT nHit = CDialog::OnNcHitTest(point); 

	if( nHit == HTCLIENT )
	{
		return HTCAPTION; 
	}

	return nHit;
}

void CPatchClientDlg::OnBnClickedButtonMore()
{

}

BOOL CPatchClientDlg::OnEraseBkgnd(CDC* pDC)
{
	return CDialog::OnEraseBkgnd(pDC);
}

LRESULT	CPatchClientDlg::OnMessageSelfPatch( WPARAM wParam , LPARAM lParam )
{
	m_pPatchClient->GetPatchClientLib()->ForceExitProgram();
	DestroyWindow();

	return (LRESULT)0;
}
void CPatchClientDlg::OnLbnSelchangeServerlist()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
#ifdef _CHN
	ChinaPatchClient* pChinaClient = ( ChinaPatchClient* )m_pPatchClient;
	pChinaClient->OnCheckPVPAgreement();
#endif
}
