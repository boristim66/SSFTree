// ShowTreeExample.cpp : Определяет точку входа для приложения.
//
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include "ShowTreeExample.h"
#include "commdlg.h"
#include <windowsx.h>

#define  _USE_DDX
#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK SelectSequence(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#include "..\xbCharTypes.h"
#include "..\csv_utils.h"
#include "..\xb_datetime.h"
#include "DibWindow.h"


typedef struct {
	NWKTREE_ENV tre;
	DIBWND_INFO dbwi;

// часть, касающаяся внутреннего обустройства локальной метабазы
	std::vector< std::vector<std::_TString> *> stv; // загруженная база
	int _icol_name, _icol_date;
	DWORD low_time, high_time;
} TREE_PROJ_INFO;



//Переменные для отрисовки дерева
extern CRITICAL_SECTION drtreeCRTS;
static std::_TString saved_path; //Must be class or structure member.
// Прототипы
int process_newick_tree(NWKTREE_ENV *proj, const TCHAR *fn);
void drawTreeWin(HWND hWnd, NWKTREE_ENV *proj); // Нарисовать дерево в размер окна
int  drawTree(NWKTREE_ENV *prpj, LONG x, LONG y); // Чуть ниже, даются размеры окна
void set_tree_sort_mode(NWKTREE_ENV *proj, TREE_SORT_MODE newmode);
void identify_tree(struct tag_treenode * cur);
int verify_tree(struct tag_treenode * tree, double root_dist);
int tree_reroot(NWKTREE_ENV *proj, std::_TString &newrootnode);
int write_newick_tree(NWKTREE_ENV* proj, const TCHAR* fn, bool fReplaceNames);


// Заглушки для функций проекта 

static const std::_TString _snam = TEXT("Strain");
static const std::_TString _scolldate = TEXT("Raw Date");

static int _col_index(std::vector< std::vector<std::_TString> *> &stv, const std::_TString  &colname)
{
	for (size_t i = 0; i < stv[0]->size(); ++i)
		if ((*stv[0])[i] == colname)
			return (int)i;
	return -1;
}

int __cdecl _compare_strains(void *context, const void *a, const void *b)
{
	std::vector<std::_TString> **va = (std::vector<std::_TString> **)a;
	std::vector<std::_TString> **vb = (std::vector<std::_TString> **)b;
	int i = *(int *)context;
	return   _tcscmp(  (**va)[i].c_str(), (**vb)[i].c_str());
}

int __cdecl _compare_strains_name(void *context, const void *key, const void *b)
{
	std::_TString *va = (std::_TString *)key;
	std::vector<std::_TString> **vb = (std::vector<std::_TString> **)b;
	int i = *(int *)context;
	return   _tcscmp(va->c_str(), (**vb)[i].c_str());
}


// If the tree is drawn in the database context, the function must return the internal database ID
int get_accessindex(NWKTREE_ENV *tenv, std::_TString  &mys)
{
	TREE_PROJ_INFO *pi = (TREE_PROJ_INFO *)tenv;
	int  col = pi->_icol_name;
	if (-1 != col) {
		std::vector<std::_TString> **v = (std::vector<std::_TString> ** )bsearch_s(&mys, &pi->stv[1], pi->stv.size() - 1, sizeof(pi->stv[1]), _compare_strains_name, &pi->_icol_name);
		if (v) 
			return (int)(v - &pi->stv[1]);
	}
	return -1;
}
//If the tree is drawn in the context of the database, the function must create a printed name for the record
void nwk_format_printable_name(NWKTREE_ENV* tenv, int accindex, std::_TString& astr)
{
	TREE_PROJ_INFO* pi = (TREE_PROJ_INFO*)tenv;
	int  col = pi->_icol_name;
	astr = (-1 != col && accindex >= 0) ? (*(pi->stv[accindex + 1]))[col] : TEXT("");
}


DWORD  get_timetsamp(NWKTREE_ENV *tenv, int accindex)
{
	TREE_PROJ_INFO *pi = (TREE_PROJ_INFO *)tenv;
	int  col = pi->_icol_date;
	if (-1 != col  && accindex >= 0) {
		FILETIME  collection_date;
		if (mk_seq_date((*(pi->stv[accindex + 1]))[col], &collection_date)) {
			if (collection_date.dwHighDateTime < pi->low_time)
				pi->low_time = collection_date.dwHighDateTime;
			if (collection_date.dwHighDateTime > pi->high_time)
				pi->high_time = collection_date.dwHighDateTime;
			return collection_date.dwHighDateTime;  
		}
	}
	return (DWORD)(-1);
}


void getMinMaxTime(NWKTREE_ENV *tenv, DWORD *low_t, DWORD *high_t)
{
	TREE_PROJ_INFO *pi = (TREE_PROJ_INFO *)tenv;
	*low_t = pi->low_time;
    *high_t = pi->high_time;
}


bool isInternalDbUsed(NWKTREE_ENV *p) 
{
	TREE_PROJ_INFO *pi = (TREE_PROJ_INFO *)p;
	return pi->stv.size() != 0;
}

void Sample_DrawTree(HWND hWnd, NWKTREE_ENV *tre)
{

	if (!tre->fExpand)
		drawTreeWin(hWnd, tre);  // scale tree to window size
	else
		drawTree(tre, 0, -1);    // Full-size raster, window scrolled

	MsgObjTypeAssign(hWnd, DBWCM_SETOPTIONS, DBW_OPT_SET, (LPARAM)DBW_OPT_DIBHDRPTR);
	MsgAttachHDib(hWnd, DBWCM_SETHDIB, TRUE, (LPARAM)tre->gHbm);
}



TKH_VM HVM;

void     XB_InitGraphics(HWND hwnd)
{
	RECT rc;

	HDC hdc = GetDC(hwnd);
	HVM.bits_per_pix = GetDeviceCaps(hdc, BITSPIXEL);

	GetClientRect(hwnd, &rc);
	HVM.screenpix.x = rc.right;
	HVM.screenpix.y = rc.bottom;

	HVM.xvscroll = GetSystemMetrics(SM_CXVSCROLL);
	HVM.yhscroll = GetSystemMetrics(SM_CYHSCROLL);
	ReleaseDC(hwnd, hdc);
}

std::_TString BrowseFile(HWND own, std::_TString &workdir, std::vector<std::_TString> &filters, DWORD addflags, bool isSave = 0)
{
	OPENFILENAME ofn;       // common dialog box structure
	TCHAR szFile[MAX_PATH] = TEXT("");       // buffer for file name
	TCHAR filter[MAX_PATH] = TEXT("");
	int l;
	const TCHAR *ppt, *filt_str;
	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = own;
	ofn.hInstance = hInst;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = _Tsizeof(szFile);
	l = 0;
	for (size_t i = 0; i < filters.size(); i++) {
		_tcsncpy_s(filter + l, MAX_PATH - l, (filters)[i].c_str(), _Tsizeof(filter) - l); l += (int)(filters)[i].size() + 1;
		//		_tcsncpy_s(filter + l, MAX_PATH - l, TEXT("*."), NUM_ARREL(filter) - l); l += 2;
		//		_tcsncpy_s(filter + l, MAX_PATH - l, (filters)[i].c_str(), NUM_ARREL(filter) - l); l += (int)(filters)[i].size()+1;
	} filter[l] = 0;

	ofn.lpstrFilter = filter[0] ? filter : TEXT("All files\0*.*\0");
	ofn.lpstrDefExt = (isSave && filters.size() >=2  && filters[1] != TEXT("") && NULL!= (filt_str =filters[1].c_str()) && NULL!=(ppt=_tcschr(filt_str , '.')) )? ppt+1 : NULL;
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = workdir.size() ? (LPCTSTR)workdir.c_str() : NULL;
	ofn.Flags = OFN_EXPLORER | addflags;
	// Display the Open dialog box. 
	if ((isSave ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn)) == TRUE) {
		if (addflags & OFN_ALLOWMULTISELECT) {
			if (ofn.nFileOffset > _tcslen(ofn.lpstrFile)) {
				std::_TString dir(ofn.lpstrFile);
				dir += TEXT("\\");
				std::_TString result;
				TCHAR *pfnames;
				for (pfnames = ofn.lpstrFile + ofn.nFileOffset; *pfnames; ) {
					if (result.size()) result += TEXT(";");
					result += dir + pfnames;
					pfnames += _tcslen(pfnames) + 1;
				}
				return result;
			}
		}
		else {
			return std::_TString(ofn.lpstrFile);
		}
	}
	else {
		l = CommDlgExtendedError();
	}
	return TEXT("");
}



int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Разместите код здесь.

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SHOWTREEEXAMPLE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

	// Must be!
	InitializeCriticalSection(&drtreeCRTS);

    // Выполнить инициализацию приложения:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SHOWTREEEXAMPLE));

    MSG msg;

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SHOWTREEEXAMPLE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_SHOWTREEEXAMPLE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
//   КОММЕНТАРИИ:
//
//        В этой функции маркер экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
//
//  WM_COMMAND  - обработать меню приложения
//  WM_PAINT    - Отрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернуться
//
//

void SD_DlgScrollInit(HWND hWnd, int cx, int cy)
{
	RECT clirc = {};
	GetClientRect(hWnd, &clirc);
	NWKTREE_ENV *tre = (NWKTREE_ENV *)GetWindowLongPtr(hWnd, GWLP_USERDATA);	

	//struct _tag_scrollinfo 	*p = &prj->si;
	SCROLLINFO si = {};
	si.cbSize = sizeof(si);
	si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
	si.nMin = 1;
	si.nMax = cy;
	si.nPos = tre->fAscend ? si.nMax : si.nMin;
	si.nPage = 3 * (clirc.bottom - clirc.top) / 4;
	tre->_gclipR.top = 0;
	SetScrollInfo(hWnd, SB_VERT, &si, cy >= clirc.bottom - clirc.top);
	GetScrollInfo(hWnd, SB_VERT, &si);
//	p->s_prevy = si.nPos;
	si.nPos = 1;
	si.nMin = 1;
	si.nMax = cx;
	si.nPage = 3 * (clirc.right - clirc.left) / 4;
//	p->s_prevx = si.nPos;
    tre->_gclipR.left = 0;
	SetScrollInfo(hWnd, SB_HORZ, &si, cx >= clirc.right - clirc.left);
	GetScrollInfo(hWnd, SB_HORZ, &si);
//	p->s_prevx = si.nPos;
}


void WriteBitmap(HWND hWnd,  std::_TString &fname)
{
	FILE* out = 0;
	if (0 == _tfopen_s(&out, fname.c_str(), TEXT("wb"))) {
		PDIBWND_INFO pDBWi = GetWindowRegistrationInfo(hWnd);
		BITMAPINFOHEADER* pbmph = ((XB_DIBHDRPTR*)(pDBWi->hDib))->pbmihdr;
		unsigned char* pBits = ((XB_DIBHDRPTR*)(pDBWi->hDib))->bits;
		BITMAPFILEHEADER h;
		h.bfType = 0x4D42;
		h.bfSize = sizeof(h) + pbmph->biSize + PaletteSize(pbmph) + pbmph->biSizeImage;
		h.bfReserved1 = h.bfReserved2 = 0;
		h.bfOffBits = sizeof(h) + pbmph->biSize + PaletteSize(pbmph);
		fwrite(&h, 1, sizeof(h), out);
		fwrite(pbmph, 1, pbmph->biSize + PaletteSize(pbmph), out);
		fwrite(pBits, 1, pbmph->biSizeImage, out);
		fclose(out);
	}
}



LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CREATE: {
		// Создаем окружение отрисовки дерева
		TREE_PROJ_INFO *tre = new TREE_PROJ_INFO;
		tre->tre.hWndDib = hWnd;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)tre);
		XB_InitGraphics(hWnd);
		EnableMenuItem(GetMenu(hWnd), 1, MF_BYPOSITION | MF_DISABLED);
		EnableMenuItem(GetMenu(hWnd), ID_SAVEIMG, MF_BYCOMMAND | MF_DISABLED);
		return TRUE;
	}
	case WM_SIZE:
		return MsgSize(hWnd, message, wParam, lParam);
	case WM_MOUSEWHEEL:
		message = WM_VSCROLL;
		if ((short)HIWORD(wParam) > 0)
			wParam = SB_LINEUP;
		else 
			wParam = SB_LINEDOWN;
	case WM_VSCROLL:
	case WM_HSCROLL:
		return MsgScrollWindow(hWnd, message, wParam, lParam);

	case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Разобрать выбор в меню:
			switch (wmId)
			{
			case IDM_ABOUT:
				DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
				break;
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			case ID_OPENMETADATA: {
				std::_TString fname;
				std::vector<std::_TString> filters;
				filters.push_back(TEXT("Text with delimiters")); filters.push_back(TEXT("*.csv;*.tsv"));
				if ((fname = BrowseFile(hWnd, saved_path, filters, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST)).size()) {
					TCHAR symdelim = ';';
					FILE *csvin = 0;
					std::_TString myfn = fname;
					if (!_tcsicmp(myfn.substr(myfn.find_last_of(TEXT(".")) + 1).c_str(), TEXT("tsv")))
							symdelim = '\t';
					TREE_PROJ_INFO *tre = (TREE_PROJ_INFO *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
					if (0 == _tfopen_s(&csvin, fname.c_str(), TEXT("rt")) ) {
						c_csv_cleaninfo(tre->stv);
						if (0 != c_csv_parser(csvin, tre->stv, symdelim, (TCHAR *)TEXT("")))
						{
							// Some error msg
						}
						else {
							// предварительные действия
							// 1. определяем номера колонок
							tre->_icol_name = _col_index(tre->stv, _snam);
							tre->_icol_date = _col_index(tre->stv, _scolldate);
							// 2. Мин и Макс время - инверсные 
							tre->low_time  = 0xFFFFFFFF; tre->high_time = 0;
                            // 3. сортируем stv по именам штаммов для ускорения поиска
							qsort_s(&tre->stv[1], tre->stv.size()-1, sizeof(tre->stv[1]),  _compare_strains, &tre->_icol_name );
							if (tre->tre.proot) {
								EnterCriticalSection(&drtreeCRTS);
								identify_tree(tre->tre.proot);
								verify_tree(tre->tre.proot, 0.0);
								LeaveCriticalSection(&drtreeCRTS);
								Sample_DrawTree(hWnd, &tre->tre);
							}
						}
					    fclose(csvin);
					}
				}
				break;
			}
			case ID_SAVEIMG: {
				NWKTREE_ENV *tre = (NWKTREE_ENV *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
				if (tre->gHbm) {
					std::_TString fname;
					const TCHAR* ext;
					std::vector<std::_TString> filters;
					filters.push_back(TEXT("BITMAP files")); filters.push_back(TEXT("*.bmp"));
					filters.push_back(TEXT("Scalable Vector Graphics files")); filters.push_back(TEXT("*.svg"));
					filters.push_back(TEXT("NEWICK tree files")); filters.push_back(TEXT("*.newick; *.nwk; *.dnd"));

					fname = BrowseFile(hWnd, saved_path, filters, OFN_OVERWRITEPROMPT, true);
					if (fname.size()) {
						if (NULL != (ext = _tcsrchr(fname.c_str(), '.'))) {
							if (!_tcsicmp(ext + 1, TEXT("bmp"))) {
								WriteBitmap(hWnd, fname);
								break;
							}
						} 
						write_newick_tree(tre, fname.c_str(), false);
					}
				}
				break;
			}

			case ID_OPEN_TREE:
			{
				std::_TString fname;
				std::vector<std::_TString> filters;
				filters.push_back(TEXT("NEWICK tree files")); filters.push_back(TEXT("*.newick; *.nwk; *.dnd"));
				fname = BrowseFile(hWnd, saved_path, filters, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST, false);
				if (fname.size()) {
					NWKTREE_ENV *tre = (NWKTREE_ENV *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
					if (!process_newick_tree(tre, fname.c_str())) {
						tre->fExpand = false;
						Sample_DrawTree(hWnd, tre);
						EnableMenuItem(GetMenu(hWnd), 1, MF_BYPOSITION | MF_ENABLED);
						EnableMenuItem(GetMenu(hWnd), ID_SAVEIMG, MF_BYCOMMAND | MF_ENABLED);
					}
				}
			}
			break;
			case ID_EXPAND: 
			case ID_ASCEND: 
			case ID_SORTLENDAT:
			{			
				NWKTREE_ENV *tre = (NWKTREE_ENV *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
				if (ID_EXPAND == wmId)
				   tre->fExpand = !tre->fExpand;
				else if (ID_ASCEND == wmId)
				   tre->fAscend = !tre->fAscend;
				else {
					tre->fTreeSortMode = static_cast<_tagTreeSort>(static_cast<int>(tre->fTreeSortMode) + 1);
					if (tre->fTreeSortMode >= TREE_ALL_MODES)
						tre->fTreeSortMode = TREE_BY_DISTANCE;
					set_tree_sort_mode(tre, tre->fTreeSortMode);
				}
				Sample_DrawTree(hWnd, tre);
			}
				break;
			case ID_REROOT: {
				std::_TString seq_name;
				if (IDOK == DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, SelectSequence, (LPARAM)&seq_name) && seq_name.size()) {
					NWKTREE_ENV *tre = (NWKTREE_ENV *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
					if (!tree_reroot(tre, seq_name)) 
						Sample_DrawTree(hWnd, tre);
				 }
			}
				break;

            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;


    case WM_PAINT:
		return MsgPaint(hWnd, message, wParam, lParam);
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hWnd, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK SelectSequence(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	switch (message)
	{
	case WM_INITDIALOG:
	    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)lParam);
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			if (LOWORD(wParam) == IDOK) {
				int textlen;
				if (0 < (textlen = GetWindowTextLength(GetDlgItem(hWnd, IDC_EDIT1)))) {
					TCHAR *ptext = new TCHAR[textlen + 1];
					if (GetWindowText(GetDlgItem(hWnd, IDC_EDIT1), ptext, textlen + 1)) {
						std::_TString *s = (std::_TString *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
						*s = ptext;
					}
					delete ptext;
				}
			}
			EndDialog(hWnd, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}




PDIBWND_INFO GetWindowRegistrationInfo(HWND hWnd)
{
	TREE_PROJ_INFO* p = (TREE_PROJ_INFO*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	return &(p->dbwi);
}

