#pragma once
#ifdef _USE_DDX
#include <ddraw.h>
#endif
typedef struct {
#ifdef _USE_DDX
	LPDIRECTDRAW		lpDD;		   // DirectDraw object
	LPDIRECTDRAWSURFACE	lpDDSPrimary;  // DirectDraw primary surface
	LPDIRECTDRAWSURFACE	lpDDSBack;	   // DirectDraw back surface
	LPDIRECTDRAWSURFACE	lpDDSOne;	   // Offscreen surface 1
	LPDIRECTDRAWPALETTE	lpDDPal;	   // The primary surface palette
	LPDIRECTDRAWCLIPPER lpClipper;
#endif
	int			bActive;	       // is application active?
	HRESULT     last_dder;
}TKXB_DDRAW_ENV;

#define DBW_OPT_DIBHANDLE 0
#define DBW_OPT_DIBPTR    1
#define DBW_OPT_LDDRSTRUC 2
#define DBW_OPT_DIBHDRPTR 3
#define DBW_OPT_DIBSTRETCH 0x80


#define DBWCM_SETHDIB       (WM_USER+157) 
#define DBWCM_SETOPTIONS    (DBWCM_SETHDIB+2) 

#define DBW_OPT_SET 0
#define DBW_OPT_AND 1
#define DBW_OPT_OR  2
#define DBW_OPT_XOR 4

// Window-Level Variables
typedef struct tag_dbwi {
	HWND hwnd;          // Created Window
	HWND hParentWnd;    // Parent Window
	RECT g_rcClient;    // Window's Client RECT
	int DibWidth, DibHeight; // Window's Client Area
	int Xorigin, Yorigin;     // Client Area Origin
	HANDLE* hDib;        // DIB Object Access Handle
	HANDLE* hSema;       // Semaphore Handle 
	DWORD ObjOptions;    // DIB Access Options
	tag_dbwi()
	{
		hwnd = hParentWnd = NULL;
		hDib = hSema = NULL;
		ObjOptions = 0;
	}

} DIBWND_INFO, * PDIBWND_INFO;

LRESULT MsgSize(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam);
LRESULT MsgPaint(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam);
LRESULT MsgScrollWindow(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam);
LRESULT MsgAttachHDib(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam);
LRESULT MsgObjTypeAssign(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam);
PDIBWND_INFO GetWindowRegistrationInfo(HWND hWnd);
WORD PaletteSize(VOID FAR* pv);
