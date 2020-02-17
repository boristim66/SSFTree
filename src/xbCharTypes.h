#pragma once

#define _Tsizeof(a) sizeof(a)/sizeof((a)[0])

#ifdef _UNICODE
#define _TString wstring
#define XML_UNICODE_WCHAR_T
#else
#define _TString string
#endif
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <assert.h>

#define STARTS_WITH(a, patt)  ((a) == strstr((a),patt))
#define STARTS_WITHW(a, patt)  ((a) == _tcsstr((a),patt))
#define SKIP_WITHW(a, patt)   ( ((a) == _tcsstr((a),patt)) ? _tcslen(patt) : 0)

#define OFFSET_MARGINS 10

typedef enum _tagTreeSort {
	TREE_BY_DISTANCE,
	TREE_BY_TIME,
	TREE_ALL_MODES,
} TREE_SORT_MODE;


struct tag_treenode {
	std::_TString nodname; // имя узла
	int depth;             // глубина вложенности
	double dist;           // длина данной ветви
	double rootdist;       // расстояние от корневого узла
	double maxChildDist;   // расстояние до наиболее далекого потомка
	int num_descend;       // суммарное количество листьев - потомков    
	int accindex;          //  индекс доступа (взять информацию, например, имя)
	DWORD tsKey;           // Ключ таймстампа для сравнения и быстрой раскраски
	tag_treenode *parent;
	std::vector <tag_treenode *> childs; // дети узла
	POINT p;
	tag_treenode(int level, struct tag_treenode *upnode = NULL) {
		depth = level;
		dist = rootdist = 0.0;
		parent = upnode;
		accindex = -1;
		num_descend = 0;
		tsKey = 0;
	}
	~tag_treenode() {
		for (size_t i = 0; i < childs.size(); ++i)
			delete childs[i];
	}
};


typedef struct {
	LONG top, left;
	LONG width, height;
} WHRECT;

#ifndef _TKXB_WCD_H
typedef struct {
	BITMAPINFOHEADER *pbmihdr;
	unsigned char *bits;
	HBITMAP hbm;
	HBITMAP old_hbm;
	LONG  gDibStride;
} XB_DIBHDRPTR;
#endif

typedef struct tag_tree_draw_env {

	// Окружение отрисовки деревьев
HWND  treePH;  //  placeholder for tree
HWND  hWndDib;
HDC    _gm_hDC;
LONG   _gDibH, _gDibW;
WHRECT _gclipR;
TEXTMETRIC _GTtm;
XB_DIBHDRPTR *gHbm;
int _gYBase;
int _gY;
int _gYStep;
double gmy;
double gmx;
RGBQUAD *_g_prgb;
DWORD  _g_timebase;
double _g_timescale;
bool   fAscend;
bool   fExpand;
TREE_SORT_MODE fTreeSortMode;
struct tag_treenode *proot;
struct tag_treenode *maxdistnode;
std::map<std::_TString, tag_treenode *> treeQfind;

tag_tree_draw_env()
{
	treePH = hWndDib = NULL;  //  placeholder for tree
	_gm_hDC = NULL;
	fAscend = true;
	fExpand = true;
	fTreeSortMode = TREE_BY_DISTANCE;
	_gY = OFFSET_MARGINS;
	_gYStep = 1;
	_gYBase = 0;
	gHbm = NULL;
	_g_prgb = NULL;
	proot = NULL;
}

} NWKTREE_ENV;

typedef struct {
	HDC  ownhdc;
	int  bits_per_pix;  // bits per pixel
	POINT worigin;
	POINT szclient;
	POINT screenpix;
	int xvscroll, yhscroll;
} TKH_VM;
