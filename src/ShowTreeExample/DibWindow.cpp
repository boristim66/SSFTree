#include <windows.h>
#include "DibWindow.h"
#include "..\xbCharTypes.h"
#include <windowsx.h>


// Отрисовка растра
#define BOUND(x,min,max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))


WORD DibNumColors(VOID FAR* pv)
{
	INT                 bits;
	LPBITMAPINFOHEADER  lpbi = ((LPBITMAPINFOHEADER)pv);
	LPBITMAPCOREHEADER  lpbc = ((LPBITMAPCOREHEADER)pv);
	/*  With the BITMAPINFO format headers, the size of the palette
	 *  is in biClrUsed, whereas in the BITMAPCORE - style headers, it
	 *  is dependent on the bits per pixel ( = 2 raised to the power of
	 *  bits/pixel).
	 */
	if (lpbi->biSize != sizeof(BITMAPCOREHEADER))
	{
		if (lpbi->biClrUsed != 0)
			return (WORD)lpbi->biClrUsed;
		bits = lpbi->biBitCount;
	}
	else   bits = lpbc->bcBitCount;

	switch (bits) {
	case 1:  return 2;
	case 4:  return 16;
	case 8:  return 256;
	case 24: return 0;
	case 16:
	case 32: return 3;
	default: return 0;
	}
}

WORD PaletteSize(VOID FAR* pv)
{
	LPBITMAPINFOHEADER lpbi;
	WORD               NumColors;

	lpbi = (LPBITMAPINFOHEADER)pv;
	NumColors = DibNumColors(lpbi);

	if (lpbi->biSize == sizeof(BITMAPCOREHEADER))
		return (WORD)(NumColors * sizeof(RGBTRIPLE));
	else
		return (WORD)(NumColors * sizeof(RGBQUAD));
}
VOID GetRealClientRect(HWND hwnd, PRECT lprc)
{
	DWORD dwStyle;

	dwStyle = GetWindowLong(hwnd, GWL_STYLE);
	GetClientRect(hwnd, lprc);
	if (dwStyle & WS_HSCROLL)
		lprc->bottom += GetSystemMetrics(SM_CYHSCROLL);
	if (dwStyle & WS_VSCROLL)
		lprc->right += GetSystemMetrics(SM_CXVSCROLL);
}

VOID SetScrollRanges(HWND hwnd, PDIBWND_INFO pDBWi)
{
	RECT       rc;
	INT        iRangeH, iRangeV, i;
	static INT iSem = 0;

	if (!iSem) {
		iSem++;
		GetRealClientRect(hwnd, &rc);

		for (i = 0; i < 2; i++) {
			iRangeV = pDBWi->DibHeight - rc.bottom;
			iRangeH = pDBWi->DibWidth - rc.right;
			if (iRangeH < 0) iRangeH = 0;
			if (iRangeV < 0) iRangeV = 0;

			if (GetScrollPos(hwnd, SB_VERT) > iRangeV ||
				GetScrollPos(hwnd, SB_HORZ) > iRangeH)
				InvalidateRect(hwnd, NULL, TRUE);
			SetScrollRange(hwnd, SB_VERT, 0, iRangeV, TRUE);
			SetScrollRange(hwnd, SB_HORZ, 0, iRangeH, TRUE);
			GetClientRect(hwnd, &rc);
		}
		iSem--;
	}
}


void SysErrBox(TCHAR* caption)
{
	LPVOID lpMsgBuf;
	DWORD Code = GetLastError();
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, Code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (TCHAR*)lpMsgBuf, caption, MB_OK | MB_ICONSTOP);// Display the string. 
	LocalFree(lpMsgBuf);  	   // Free the buffer.
}

VOID AppPaint(HWND hWnd, PDIBWND_INFO pDBWi, HDC hDC, INT x, INT y, RECT* prc)
{
	BITMAPINFOHEADER* pbmph;
	int Width, Height, diffx, diffy;
	int lflag = 0;
	unsigned char* pBits;
	RECT drect;

	SetWindowOrgEx(hDC, x, y, NULL);
	SetBkMode(hDC, TRANSPARENT);
	if (pDBWi->hDib && *(pDBWi->hDib))
	{
		if (pDBWi->hSema && *(pDBWi->hSema))  WaitForSingleObject(*(pDBWi->hSema), 4000);
#ifdef _USE_DDX
		if (pDBWi->ObjOptions == DBW_OPT_LDDRSTRUC)
		{
			LPDIRECTDRAWSURFACE psrc = ((TKXB_DDRAW_ENV*)(pDBWi->hDib))->lpDDSBack;
			LPDIRECTDRAWSURFACE pdst = ((TKXB_DDRAW_ENV*)(pDBWi->hDib))->lpDDSPrimary;
			RECT  srect = *prc;
			POINT pt;
			pt.x = pt.y = 0;
			GetClientRect(hWnd, &drect);
			ClientToScreen(hWnd, &pt);
			OffsetRect(&drect, pt.x, pt.y);
			OffsetRect(&srect, pDBWi->Xorigin, pDBWi->Yorigin);
			IDirectDrawSurface_Blt(pdst, &drect, psrc, &srect, DDBLT_WAIT, 0);
		}
		else
#endif
		{
			pbmph = (pDBWi->ObjOptions & DBW_OPT_DIBPTR) ?
				((pDBWi->ObjOptions & DBW_OPT_LDDRSTRUC) ? ((XB_DIBHDRPTR*)(pDBWi->hDib))->pbmihdr : (BITMAPINFOHEADER*)((LPBITMAPINFO)(pDBWi->hDib))) :
				(BITMAPINFOHEADER*)(LPBITMAPINFO)GlobalLock(*(pDBWi->hDib));


			Width = pbmph->biWidth;
			Height = abs(pbmph->biHeight);
			if (pDBWi->ObjOptions & DBW_OPT_LDDRSTRUC)
				pBits = ((XB_DIBHDRPTR*)(pDBWi->hDib))->bits;
			else
				pBits = (unsigned char*)pbmph + (WORD)(pbmph->biSize) + PaletteSize(pbmph);

			if (pDBWi->ObjOptions & DBW_OPT_DIBSTRETCH)
			{
				double xratio, yratio;
				int destWidth, destHeight;
				GetClientRect(hWnd, &drect);
				xratio = (double)drect.right / (double)pbmph->biWidth;
				yratio = (double)drect.bottom / (double)Height;
				if (xratio > yratio)
				{ // Сжимаем по у
					destHeight = drect.bottom;
					destWidth = (int)(drect.right * yratio / xratio);
				}
				else
				{
					// Сжимаем по у
					destHeight = (int)(drect.bottom * xratio / yratio);
					destWidth = drect.right;
				}
				StretchDIBits(hDC, 0, 0, destWidth, destHeight,
					0, 0, Width, Height,
					pBits, (LPBITMAPINFO)pbmph, DIB_RGB_COLORS, SRCCOPY);
				Width = destWidth;
				Height = destHeight;


			}
			else  if (0 == SetDIBitsToDevice(hDC,
				0, 0, Width, Height,
				0, 0, 0, Height,
				pBits, (LPBITMAPINFO)pbmph,
				DIB_RGB_COLORS))
			{
				SysErrBox((TCHAR*)TEXT("SetDIBitsToDevice"));
			}
		}
		if (pDBWi->hSema && *(pDBWi->hSema))
			ReleaseSemaphore(*(pDBWi->hSema), 1, NULL);

		if ((diffx = Width - x) < prc->right - prc->left)
		{
			SelectObject(hDC, (HBRUSH)(COLOR_WINDOW));
			PatBlt(hDC, diffx, prc->top, prc->right - diffx,
				prc->bottom - prc->top + y, PATCOPY);
			SelectObject(hDC, (HBRUSH)(NULL_BRUSH));
			lflag++;
		}
		if ((diffy = Height - y) < prc->bottom - prc->top)
		{
			SelectObject(hDC, (HBRUSH)(COLOR_WINDOW));
			PatBlt(hDC, x, diffy,     // prc->left
				((lflag) ? diffx : prc->right - prc->left),
				prc->bottom - diffy, PATCOPY);
			SelectObject(hDC, (HBRUSH)(NULL_BRUSH));
		}
		if (!(pDBWi->ObjOptions & DBW_OPT_DIBPTR)) GlobalUnlock(*(pDBWi->hDib));
	}

}

LRESULT MsgObjTypeAssign(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam)
{ // WPARAM => MASK // Set, and, or, xor, 
	PDIBWND_INFO pDBWi = GetWindowRegistrationInfo(hwnd);
	if (pDBWi)
	{
		switch (wparam)
		{
		case DBW_OPT_SET: pDBWi->ObjOptions = (DWORD)lparam; break;
		case DBW_OPT_AND: pDBWi->ObjOptions &= (DWORD)lparam; break;
		case DBW_OPT_OR:  pDBWi->ObjOptions |= (DWORD)lparam; break;
		case DBW_OPT_XOR: pDBWi->ObjOptions ^= (DWORD)lparam; break;
		default: break;
		}
		return 0;
	}
	return -1;
}


LRESULT MsgAttachHDib(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
	BITMAPINFOHEADER* pbmph = NULL;
	PDIBWND_INFO pDBWi = GetWindowRegistrationInfo(hwnd);

	if (pDBWi)
	{
		if (wparam) // AttachDib
		{
			pDBWi->hDib = (HANDLE*)lparam;
#ifdef _USE_DDX
			if ((pDBWi->ObjOptions & 3) == DBW_OPT_LDDRSTRUC)
			{
				DDSURFACEDESC ddsd;
				LPDIRECTDRAWSURFACE pdds;
				ZeroMemory(&ddsd, sizeof(ddsd));
				ddsd.dwSize = sizeof(ddsd);
				pdds = ((TKXB_DDRAW_ENV*)lparam)->lpDDSBack;
				IDirectDrawSurface_GetSurfaceDesc(pdds, &ddsd);
				pDBWi->DibWidth = ddsd.dwWidth;
				pDBWi->DibHeight = ddsd.dwHeight;
			}
			else
#endif
			{
				if ((pDBWi->ObjOptions & 3) == DBW_OPT_DIBPTR)
				{
					pDBWi->hDib = (HANDLE*)(*(pDBWi->hDib));
					pbmph = (BITMAPINFOHEADER*)(pDBWi->hDib);
				}
				else if ((pDBWi->ObjOptions & 3) == DBW_OPT_DIBHANDLE)
				{
					pbmph = (BITMAPINFOHEADER*)GlobalLock(*(pDBWi->hDib));
				}
				else if ((pDBWi->ObjOptions & 3) == DBW_OPT_DIBHDRPTR)
				{
					pbmph = ((XB_DIBHDRPTR*)(pDBWi->hDib))->pbmihdr;

				}
				pDBWi->DibWidth = pbmph->biWidth;
				pDBWi->DibHeight = abs(pbmph->biHeight);
				if ((pDBWi->ObjOptions & 3) == DBW_OPT_DIBHANDLE)
					GlobalUnlock(*(pDBWi->hDib));
			}
		}
		else
		{
			pDBWi->hDib = NULL; pDBWi->DibWidth = pDBWi->DibHeight = 0;
		}
		return MsgSize(hwnd, WM_SIZE, 0, (LPARAM)MAKELONG(pDBWi->DibWidth, pDBWi->DibHeight));
	}
	return -1;

}


LRESULT MsgSize(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
	int cx = LOWORD(lparam);
	int cy = HIWORD(lparam);
	PDIBWND_INFO pDBWi = GetWindowRegistrationInfo(hwnd);
	if (pDBWi)
	{
		pDBWi->g_rcClient.right = cx;
		pDBWi->g_rcClient.bottom = cy;
		if (!(pDBWi->ObjOptions & DBW_OPT_DIBSTRETCH))
			SetScrollRanges(hwnd, pDBWi);
		InvalidateRect(hwnd, NULL, TRUE);
	}
	return 0;
}

LRESULT MsgPaint(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
	PAINTSTRUCT ps;
	RECT clientrc;
	PDIBWND_INFO pDBWi = GetWindowRegistrationInfo(hwnd);

	if (pDBWi)
	{
		HDC      hdc = BeginPaint(hwnd, &ps);

		GetClientRect(hwnd, &clientrc);
		if (pDBWi->hDib && *(pDBWi->hDib))
		{
			AppPaint(hwnd, pDBWi, hdc,
				pDBWi->Xorigin = GetScrollPos(hwnd, SB_HORZ),
				pDBWi->Yorigin = GetScrollPos(hwnd, SB_VERT), &clientrc); //&ps.rcPaint );
		}
		else
		{
			SelectObject(hdc, (HBRUSH)(COLOR_WINDOW));
			PatBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
				ps.rcPaint.bottom - ps.rcPaint.top, PATCOPY);
			SelectObject(hdc, (HBRUSH)(NULL_BRUSH));
		}
		EndPaint(hwnd, &ps);
	}
	return 0;
}

LRESULT MsgScrollWindow(HWND hwnd, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
	INT  iMax, iMin, iPos;
	INT  dnx = 0, dny = 0;
	RECT             rc;
	int nBar;
	long* pRecSide;
	INT* pdn;
	INT sign;

	if (uMessage == WM_VSCROLL)
	{
		nBar = SB_VERT;   pRecSide = &rc.bottom;  pdn = &dny;
	}
	else
	{
		nBar = SB_HORZ;   pRecSide = &rc.right;   pdn = &dnx;
	}


	GetClientRect(hwnd, &rc);
	GetScrollRange(hwnd, nBar, &iMin, &iMax);
	iPos = GetScrollPos(hwnd, nBar);

	//* Calculate new  scroll position */

	switch (GET_WM_VSCROLL_CODE(wparam, lparam))
	{
	case SB_LINEDOWN: sign = 1; goto ScrollLine;
	case SB_LINEUP:   sign = -1;
	ScrollLine:
		*pdn = sign * (*pRecSide) / 16 + sign;
		break;
	case SB_PAGEDOWN: sign = 1; goto ScrollPage;
	case SB_PAGEUP:   sign = -1;
	ScrollPage:
		*pdn = sign * (*pRecSide) / 2 + sign;
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		*pdn = GET_WM_VSCROLL_POS(wparam, lparam) - iPos;
		break;
	default:
		*pdn = 0;
		break;
	}
	/* Limit scrolling to current scroll range */
	if (*pdn = BOUND(iPos + *pdn, iMin, iMax) - iPos)
	{
		SetScrollPos(hwnd, nBar, iPos + *pdn, TRUE);
		InvalidateRect(hwnd, NULL, TRUE);
	}
	return 0;
}


