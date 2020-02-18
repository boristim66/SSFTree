#include <Windows.h>
#include <TCHAR.h>
#include <stdio.h>
#ifndef _NWK_TREE_STANDALONE
#include "tkxb_wcd.h"
#endif
#include "xbCharTypes.h"

// Extern functions
// If the tree is drawn in the context of the database, the function must return the Record Timestamp
DWORD  get_timetsamp(NWKTREE_ENV *, int);
// If the tree is drawn in the database context, the function must return the internal database ID
int get_accessindex(NWKTREE_ENV *, std::_TString  &mys);
//If the tree is drawn in the context of the database, the function must create a printed name for the record
void nwk_format_printable_name(NWKTREE_ENV *, int accindex, std::_TString  &astr);

void getMinMaxTime(NWKTREE_ENV *, DWORD *low_t, DWORD *high_t);

bool isInternalDbUsed(NWKTREE_ENV*);

#ifndef _NWK_TREE_STANDALONE
extern "C" LRESULT VisualiseString(LPTSTR format, ...);
extern "C" LRESULT VisualiseStringAsync(LPTSTR format, ...);
#else
#define VisualiseString
#endif


/***************Newick Tree Interfaces********************************/
// Global and static variables are used here to save recursive stack
typedef enum {
	TP_NAME,
	TP_DIST,
} TREE_PARSENG;

void draw_legend(NWKTREE_ENV* prpj, LONG x, LONG y);

void DrawThinLine(POINT from, POINT to, RGBQUAD color);
void DrawLabelText(const TCHAR* ltext, int ltextlen, RECT* pos);

void DrawThinLineSVG(POINT from, POINT to, RGBQUAD color);
void DrawLabelTextSVG(const TCHAR* ltext, int ltextlen, RECT* pos);

int drawTree(NWKTREE_ENV* prpj, LONG x, LONG y);

static void set_tree_metrics(NWKTREE_ENV* prpj, LONG x, LONG y, POINT* pictdims);
void drawtrerec(struct tag_treenode* tree);

// Globals - stack overload protection
CRITICAL_SECTION drtreeCRTS; // Static guard
// IN
int _gTreeIsNucleotic = -1;
static bool _fgfReplaceNames;
static FILE  *_fgOutfile;
NWKTREE_ENV    *_g_curprj;
// OUT, set by verify_tree
static int _gNodeeCount = 0;
static double _gMaxDist = 0;
static struct tag_treenode * _gMaxLenNode = NULL;
// draw functions
static void (*_pfdrawline)(POINT, POINT, RGBQUAD)    = DrawThinLine;
static void (*_pfdrawtext)(const TCHAR*, int, RECT*) = DrawLabelText;





void char2TString(char *p, std::_TString &s)
{
	if (STARTS_WITH(p, "gb|"))
		p += 3;
#ifdef _UNICODE
	TCHAR wtemp[256];
	if (MultiByteToWideChar(CP_ACP, 0, p, -1, wtemp, sizeof(wtemp) / sizeof(wtemp[0])))
		s = wtemp;
#else 
	s = p;
#endif
}

static void identify_tree(struct tag_treenode * cur)
{//Statics are used to avoid stack overloading, protected by critical section
	static std::map<std::_TString, int>::iterator it;
	static const TCHAR *p, *pnm;
	static std::_TString  mys;
	static std::map<std::_TString, int> *paccmap;
	static int access;

	if (!cur->childs.size()) {
		pnm = cur->nodname.c_str();
		mys = (NULL != (p = _tcschr(pnm, ' '))) ? std::_TString(pnm, (int)(p - pnm)) : cur->nodname;
		if (STARTS_WITHW(mys.c_str(), TEXT("cds:")))
			mys = mys.substr(4);
		int access = get_accessindex(_g_curprj, mys);
		if (access >= 0)
			cur->accindex = access, _g_curprj->treeQfind[mys] = cur;
	}
	else {
		for (size_t i = 0; i < cur->childs.size(); i++) {
			identify_tree(cur->childs[i]);
		}
	}
}


static int verify_tree(struct tag_treenode * tree, double root_dist)
{

	tree->rootdist = root_dist;
	tree->num_descend = 0;
	tree->maxChildDist = tree->rootdist + tree->dist;
	if (tree->maxChildDist > _gMaxDist) {
		_gMaxDist = tree->maxChildDist;
		_gMaxLenNode = tree;
	}
	if (0 == tree->childs.size()) {
		if (_g_curprj &&   tree->accindex >= 0)
			tree->tsKey = get_timetsamp(_g_curprj, tree->accindex);
		return 1;
	}

	double max_dist = 0;
	DWORD  max_ts = 0;
	for (int i = 0; i < tree->childs.size(); ++i) {
		assert(tree->childs[i]->parent == tree);
		++_gNodeeCount;
		tree->num_descend += verify_tree(tree->childs[i], tree->rootdist + tree->dist);
		if (tree->childs[i]->maxChildDist > max_dist)
			max_dist = tree->childs[i]->maxChildDist;
		if (tree->childs[i]->tsKey > max_ts)
			max_ts = tree->childs[i]->tsKey;
	}
	if (_g_curprj->fTreeSortMode == TREE_BY_DISTANCE)
		std::sort(tree->childs.begin(), tree->childs.end(),
			[](tag_treenode *a, tag_treenode *b) {return b->maxChildDist > a->maxChildDist; });
	else if (_g_curprj->fTreeSortMode == TREE_BY_TIME)
		std::sort(tree->childs.begin(), tree->childs.end(),
			[](tag_treenode *a, tag_treenode *b) {return b->tsKey > a->tsKey; });


	tree->maxChildDist = max_dist;
	tree->tsKey = max_ts;
	return tree->num_descend;
}


int newick_tree_verify(NWKTREE_ENV* proj)
{
	if (proj && proj->proot) {
		EnterCriticalSection(&drtreeCRTS);
		_g_curprj = proj;
		_gMaxDist = 0;
		_gMaxLenNode = NULL;
		_gNodeeCount = 0;
		verify_tree(proj->proot, 0);
		proj->maxdistnode = _gMaxLenNode;
		LeaveCriticalSection(&drtreeCRTS);
	}
	return _gNodeeCount;
}

void newick_tree_identify(NWKTREE_ENV* proj)
{
	EnterCriticalSection(&drtreeCRTS);
	_g_curprj = proj;
	if (isInternalDbUsed(proj)) {
		_gTreeIsNucleotic = -1;
		identify_tree(proj->proot);
	}
	LeaveCriticalSection(&drtreeCRTS);
}

void newick_tree_set_sort_mode(NWKTREE_ENV *proj, TREE_SORT_MODE newmode)
{
	if (proj && proj->proot) {
		EnterCriticalSection(&drtreeCRTS);
		_g_curprj = proj;
		_gMaxDist = 0;
		_gMaxLenNode = NULL;
		_gNodeeCount = 0;
		verify_tree(proj->proot, 0.0);
		LeaveCriticalSection(&drtreeCRTS);
	}
}


int newick_tree_reroot(NWKTREE_ENV *proj, std::_TString &newrootnode)
{
	struct tag_treenode *cur, /* *p, */ *newroot, *nextparent, *nextroot;
	std::map<std::_TString, tag_treenode *>::iterator it;
	double prev_dist;
	if (!proj->proot || (it = proj->treeQfind.find(newrootnode)) == proj->treeQfind.end())
		return -1;
	cur = it->second;
	if (cur->parent == proj->proot)
		return 0;
	newroot = new struct tag_treenode(0);

	prev_dist = cur->parent->dist;
	cur->parent->dist = cur->dist / 20.0;
	cur->dist = cur->parent->dist*19.0;
	newroot->childs.push_back(cur->parent);
	newroot->childs.push_back(cur);
	nextroot = newroot;
	size_t i=0;
	while (cur != proj->proot) {
		for (i = 0; i < cur->parent->childs.size(); ++i)
			if (cur->parent->childs[i] == cur) {
				nextparent = cur->parent;
				cur->parent = nextroot;
				nextparent->childs[i] = nextparent->parent;
				if (nextparent->childs[i]) {
					double t = nextparent->childs[i]->dist;
					nextparent->childs[i]->dist = prev_dist;
					prev_dist = t;
				}
				nextroot = (cur == it->second) ? newroot : cur;
				cur = nextparent;
				break;
			}
	}
	proj->proot->childs.erase(proj->proot->childs.begin() + i);
	proj->proot->parent = nextroot;
	proj->proot = newroot;

	newick_tree_verify(proj);
	return 0;
}


int newick_tree_process(NWKTREE_ENV *proj, const TCHAR *fn)
{
#define MAX_NUMSZ 20
	char buf[4096 + 1], bufnm[256], ch, *ptr;
	FILE *in;
	bool done = false, isquoted = false;
	int ofs = 0, rd;
	int cb = 0, occount = 0, nc = 0;  // Byte counter, parenthesis counter, name symbols counter
	TREE_PARSENG pe = TP_NAME;
	struct tag_treenode *cur, *p;
	int error = 0;
	int inComment = 0;
	int inQuoted = 0, firstQuote = 0;
	proj->treeQfind.clear();

	if (0 == _tfopen_s(&in, fn, TEXT("rt")))
	{
		if (proj->proot) {
			delete proj->proot;
			proj->proot = NULL;
		} proj->proot;

		cur = proj->proot = new struct tag_treenode(0);

		while (!done && ofs + (rd = (int)fread(buf + ofs, sizeof(buf[0]), sizeof(buf) / sizeof(buf[0]) - ofs - 1, in)) > 0) {
			ptr = buf;
			rd += ofs, ofs = 0;
			buf[rd] = 0;
			cb = 0;
			while (0 == ofs && cb < rd) {
				ch = *ptr++; ++cb;
				if (0 == firstQuote) {
					if ('\'' == ch) { // Always skip first quote
						firstQuote = 1;
						continue; // next symbol assumed
					}
				}
				else { // previous symbol is quote
					firstQuote = 0;
					if ('\'' != ch)
						inQuoted = !inQuoted;
				}
				if ('\'' == ch || inQuoted)
					goto PutSymbol;
				switch (ch) {
				case '(': 	cur->childs.push_back(p = new tag_treenode(++occount, cur));	cur = p;
					nc = 0;	pe = TP_NAME;
					break;
				case ')':   if (!cur->nodname.size()) {
					bufnm[nc++] = 0;   char2TString(bufnm, cur->nodname);
				}
							nc = 0;	pe = TP_NAME;
							cur = cur->parent;  --occount;
							break;
				case ',':	if (!cur->nodname.size()) {
					bufnm[nc++] = 0;
					char2TString(bufnm, cur->nodname);
				}
							nc = 0;	pe = TP_NAME;
							cur->parent->childs.push_back(p = new tag_treenode(occount, cur->parent));	cur = p;
							break;
				case ':':	if (!cur->nodname.size()) {
					bufnm[nc++] = 0;  char2TString(bufnm, cur->nodname);
				}
							pe = TP_DIST;
							break;
				case ';':	if (occount) {
					VisualiseString(TEXT("Error: Unbalansed Tree level %d\n"), occount);
					error = -1;
				}
							if (cur != proj->proot) {
								VisualiseString(TEXT("Error: On return root is mismatched %d\n"), occount);
								error = -2;
							}
							done = true;
				case '\n':	case '\r':  //Just ignore
					break;
				case '[':   ++inComment;
					break;
				case ']':   --inComment;
					break;

				default:   if (TP_NAME == pe) {
				PutSymbol:
					bufnm[nc++] = (inQuoted || ch != '_') ? ch : ' ';

				}
						   else if (TP_DIST == pe) {
					--cb; --ptr;
					if (rd - cb > MAX_NUMSZ || rd < sizeof(buf) - 1) {
						char *endptr;
						double d = strtod(ptr, &endptr);
						if (*endptr != 0  || (size_t)(endptr - ptr) == strlen(ptr))  {
							cur->dist = d;
							cb += (int)(endptr - ptr);   ptr = endptr;
						}
						else {
							VisualiseString(TEXT("Error: Can't process string  %s\n"), ptr);
						}
					}
					else  // A shitty case where the distance did not fit in the file reading buffer
						memmove(buf, ptr, ofs = rd - cb);
				}
						   break;
				}
			}
		}
		fclose(in);
	
		newick_tree_identify(proj);
		newick_tree_verify(proj);

	}
	return error;
}

static bool isSymbolInTheString(const TCHAR *str, const TCHAR *charset)
{
	for (; *charset; ++charset) {
		if (_tcschr(str, *charset))
			return true;
	}
	return false;
}


void printtree_recursive(tag_treenode *cur)
{
// 	The name is expected to consist of a database key separated by a space from the name of the following
	static std::_TString astr;
	static size_t pos;

	for (size_t i = 0; i < cur->childs.size(); i++) {
		_ftprintf(_fgOutfile, 0 == i ? TEXT("(") : TEXT(","));
		if (0 == cur->childs[i]->childs.size()) {
			astr = cur->childs[i]->nodname;
			if (_fgfReplaceNames) {
					nwk_format_printable_name(_g_curprj, cur->childs[i]->accindex, astr);

			}
			for (pos = 0; ; ) { // Escape internal single quotation marks
				if (std::_TString::npos != (pos = astr.find_first_of('\'', pos))) {
					astr.insert(pos, 1, '\'');
					pos += 2;
				}
				else break;
			}
			// If there is any of the listed in the name, we include the entire string in the quotation marks
			if (std::_TString::npos != astr.find_first_of(TEXT("()[];:,_ "))) {
				astr.insert(0, 1, '\'');
				astr.push_back('\'');
			}
		
			if (0.0 == cur->childs[i]->dist)
				_ftprintf(_fgOutfile, TEXT("%s:0.0"), astr.c_str());
			else
				_ftprintf(_fgOutfile, TEXT("%s:%.8lg"), astr.c_str(), cur->childs[i]->dist);
		}
		else {
			printtree_recursive(cur->childs[i]);
		}
		if (i == cur->childs.size() - 1) {
			if (cur->nodname.size() || cur->dist) {
				if (0.0 == cur->dist)
					_ftprintf(_fgOutfile, TEXT(")%s:0.0"), cur->nodname.c_str());
				else
					_ftprintf(_fgOutfile, TEXT(")%s:%.05lf"), cur->nodname.c_str(), cur->dist);
			}
			else
				_ftprintf(_fgOutfile, TEXT(")"));
		}
	}
}

int newick_tree_write(NWKTREE_ENV *proj, const TCHAR *fn, bool fReplaceNames)
{
	FILE *out;
	bool done = false;
	int error = -1;
	const TCHAR* ext;
	enum {
		WR_NWK,
		WR_SVG
	} wr_mode = WR_NWK;

	if (NULL != (ext = _tcsrchr(fn, '.'))) {
		if (!_tcsicmp(ext + 1, TEXT("svg")))
			wr_mode = WR_SVG;
	}


	if (proj->proot && 0 == _tfopen_s(&out, fn, TEXT("wt")) && NULL != out)
	{
		if (wr_mode == WR_SVG) {
		}  		
		EnterCriticalSection(&drtreeCRTS);
			_fgfReplaceNames = fReplaceNames; 
			_fgOutfile = out;
			_g_curprj = proj;
		
			if (wr_mode == WR_SVG) {
				POINT pd;
				NWKTREE_ENV* oldprpj = new NWKTREE_ENV;
				memcpy(oldprpj, proj, sizeof(*proj));
                // Set draw functions
				_pfdrawline = DrawThinLineSVG;
				_pfdrawtext = DrawLabelTextSVG;
				proj->fExpand = true; proj->fAscend = false;
				set_tree_metrics(proj, 0, -1, &pd);
				pd.x += 2 * OFFSET_MARGINS; pd.y += 2 * OFFSET_MARGINS;
				_ftprintf(out,
					TEXT("<svg  width=\"%d\" height=\"%d\" version=\"1.0\""	" viewBox = \"0 0 %d %d\" "
						" xmlns = \"http://www.w3.org/2000/svg\">\n"
						"<style>\n"	".small{ font: normal 14px \"Times New Roman\"; } \n" "</style>\n"), 
					pd.x, pd.y, pd.x, pd.y);

				draw_legend(proj, 40, pd.y/10);

				proj->_gY = 0;
				proj->_gYStep = (proj->fExpand == false && pd.y) ? 1 : OFFSET_MARGINS;
				drawtrerec(proj->proot);

				// Restore  draw functions after
				_pfdrawline = DrawThinLine;
				_pfdrawtext = DrawLabelText;
				 memcpy(proj, oldprpj, sizeof(*proj));
				 delete oldprpj;
			}
			else if (wr_mode == WR_NWK)
			    printtree_recursive(proj->proot);
			
		LeaveCriticalSection(&drtreeCRTS);
		if (wr_mode == WR_SVG) {
			_ftprintf(out, TEXT("</svg>\n"));
		}
		else if (wr_mode == WR_NWK) {
			_ftprintf(out, TEXT(";\n"));
		}
		error = 0;
		fclose(out);

	}
	return error;
}



// Windows GDI
#define _UZE_NEW_DIBIFACE
#define NUM_PAL_ENTRIES 512
#define TKXB_PCDFORM_GRAY8  0xA
#define TKXB_PCDFORM_RGB24  0xB
#define TKXB_PCDFORM_YCC24  0xC
#define TKXB_PCDFORM_RGB8   0xD


static const RGBQUAD _gc = { 0x00, 0x00, 0x0, 0 };


XB_DIBHDRPTR *XB_AllocImgBitmap(HWND hw, int width, int height, int format)
{
	DWORD  sizeImage, sizeBits;
	BITMAPINFOHEADER *pbmph;
	DWORD multiplier, szpal;

	if (format == TKXB_PCDFORM_GRAY8 || format == TKXB_PCDFORM_RGB8)
		multiplier = 1, szpal = 0x400;
	else
		multiplier = 3, szpal = 0;

	int stride = ((((width * multiplier * 8) + 31) & ~31) >> 3);
	sizeImage = (sizeBits = stride * abs(height)) + sizeof(BITMAPINFOHEADER) + szpal;
#if !defined _UZE_NEW_DIBIFACE
	HANDLE  hbitmap;
	if (NULL != (hbitmap = GlobalAlloc(GMEM_MOVEABLE, sizeImage)))
	{
		if (NULL != (pbmph = (BITMAPINFOHEADER *)GlobalLock(hbitmap)))
		{
			memset(pbmph, 0, sizeof(BITMAPINFOHEADER));
#else
	XB_DIBHDRPTR *pdibinfo = NULL;
	if (NULL != (pbmph = (BITMAPINFOHEADER *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BITMAPINFOHEADER) + szpal)))
	{
		{
			pdibinfo = (XB_DIBHDRPTR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pdibinfo));
			pdibinfo->gDibStride = stride;
			pdibinfo->pbmihdr = pbmph;
#endif
			pbmph->biSize = sizeof(BITMAPINFOHEADER);
			pbmph->biWidth = width;
			pbmph->biHeight = -height;
			pbmph->biPlanes = 1;
			pbmph->biBitCount = (USHORT)(multiplier * 8);
			pbmph->biSizeImage = sizeBits;
			pbmph->biCompression = BI_RGB;
#if !defined _UZE_NEW_DIBIFACE
			memset((unsigned char *)(&pbmph[1]) + szpal, 0xFF, sizeBits);
#else
			pdibinfo->hbm = CreateDIBSection(GetDC(hw), (BITMAPINFO *)pbmph, DIB_RGB_COLORS,
				(void **)&(pdibinfo->bits), NULL, NULL);
			memset(pdibinfo->bits, 0xFF, sizeBits);
#endif			
#if !defined _UZE_NEW_DIBIFACE			
			GlobalUnlock(hbitmap);
		}
	else { GlobalFree(hbitmap); hbitmap = NULL; }
	}
	return hbitmap;
#else
		}
	}
	return pdibinfo;
#endif
}



LONG  ClipLine(POINT *from, POINT *to, WHRECT* pr)
{

	LONG dx = to->x - from->x;
	LONG dy = to->y - from->y;

	if (!(dx || dy)) { //Zero length, line may not be drawn, but inscriptions should be cut too
		if (to->x < pr->left || to->x >= pr->left + pr->width)
			return 0;
		if (to->y < pr->top || to->y >= pr->top + pr->height)
			return 0;
	}
	if (dx) { // Horizontal line
		if (to->x < pr->left || from->x >= pr->left + pr->width)
			return 0;
		if (to->y < pr->top || to->y >= pr->top + pr->height)
			return 0;
	}
	if (dy) {
		if (to->y < pr->top || from->y >= pr->top + pr->height)
			return 0;
		if (to->x < pr->left || to->x >= pr->left + pr->width)
			return 0;
	}

	POINT newfrom = { from->x - pr->left, from->y - pr->top };
	POINT newto = { to->x - pr->left, to->y - pr->top };

	if (newfrom.x < 0) newfrom.x = 0;
	else if (newfrom.x > pr->width) newfrom.x = pr->width;

	if (newfrom.y < 0) newfrom.y = 0;
	else if (newfrom.y > pr->height) newfrom.y = pr->height;

	if (newto.x < 0) newto.x = 0;
	else if (newto.x > pr->width) newto.x = pr->width;

	if (newto.y < 0) newto.y = 0;
	else if (newto.y > pr->height) newto.y = pr->height;

	*from = newfrom;
	*to = newto;

	return (newfrom.x | newfrom.y | newto.x | newto.y);
}


void DrawThinLine(POINT from, POINT to, RGBQUAD color)
{
#if !defined _UZE_NEW_DIBIFACE
	char *pdest = ((char *)bh) + sizeof(BITMAPINFOHEADER);
	if (bh->biBitCount == 8)
		pdest += 0x400;
#else
	unsigned char *pdest = _g_curprj->gHbm->bits;
#endif
	int stepptr = (_g_curprj->gHbm->pbmihdr->biBitCount / 8);
	if (from.x > to.x) { POINT t = from;	  from = to;		to = t; }
	if (from.y > to.y) { POINT t = from;	  from = to;		to = t; }

	int stride = _g_curprj->gHbm->gDibStride;
	int pt0 = stride * from.y + stepptr * from.x;
	int len, i = 0, offset = 0;

	pdest += pt0;
	if ((len = (to.x - from.x)) == 0) { // Вертикаль
		len = to.y - from.y;
		stepptr = stride;
		goto DrawHVLoop;
	}
	else {
		if (color.rgbRed == color.rgbGreen  && color.rgbGreen == color.rgbBlue)
			memset(pdest, color.rgbRed, len*(_g_curprj->gHbm->pbmihdr->biBitCount / 8));
		else {
		DrawHVLoop:
			for (; i < len; ++i) {
				pdest[offset + 0] = color.rgbBlue;
				pdest[offset + 1] = color.rgbGreen;
				pdest[offset + 2] = color.rgbRed;
				offset += stepptr;
			}
		}
	}
}

void DrawThinLineSVG(POINT from, POINT to, RGBQUAD color)
{
	_ftprintf(_fgOutfile, TEXT( "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
		"stroke=\"#%02x%02x%02x\" stroke-width=\"%.0f\" />\n"),
		from.x, from.y, to.x, to.y, color.rgbRed, color.rgbGreen, color.rgbBlue, 1.0);
}


void DrawLabelText(const TCHAR *ltext, int ltextlen, RECT *pos)
{
	DrawText(_g_curprj->_gm_hDC, ltext, ltextlen, pos, DT_SINGLELINE | DT_NOCLIP);
}


void DrawLabelTextSVG(const TCHAR* ltext, int ltextlen, RECT* pos)
{
	_ftprintf(_fgOutfile, TEXT("<text x=\"%d\" y=\"%d\" class=\"small\" >%s</text>\n"),
		pos->left, pos->top + _g_curprj->_GTtm.tmHeight/2, ltext);
}

void drawtrerec(struct tag_treenode *tree)
{
	static POINT from, to; // to reduce stack load
	static DWORD dt;
	static int ci;
	static RECT pos;

	if (0 == tree->childs.size()) {
		from = { OFFSET_MARGINS + (int)(tree->rootdist*_g_curprj->gmx + 0.5), OFFSET_MARGINS + (int)(_g_curprj->_gY*_g_curprj->gmy + 0.5) };
		to = { OFFSET_MARGINS + (int)((tree->rootdist + tree->dist)*_g_curprj->gmx + 0.5), from.y };
		tree->p = from;
		_g_curprj->_gY += _g_curprj->_gYStep;

		if (!ClipLine(&from, &to, &_g_curprj->_gclipR)) return;
		if (from.x != to.x) { // Small optimisaton - skip zero-length lines
			ci = -1;
			if (_g_curprj && _g_curprj->_g_prgb && tree->accindex >= 0) {
				dt = tree->tsKey ? tree->tsKey : get_timetsamp(_g_curprj, tree->accindex);
				if ((ci = (int)((dt - _g_curprj->_g_timebase) * _g_curprj->_g_timescale)) > NUM_PAL_ENTRIES - 1)
					ci = NUM_PAL_ENTRIES - 1;
			}
			(*_pfdrawline)(from, to, (ci >= 0 ? _g_curprj->_g_prgb[ci] : _gc));
		}
		if (_g_curprj->_gYStep >= _g_curprj->_GTtm.tmHeight / 3) {
			pos = { to.x, _g_curprj->_gYBase ? _g_curprj->_gYBase - (to.y + _g_curprj->_GTtm.tmHeight / 2) : to.y - _g_curprj->_GTtm.tmHeight / 2, 0, 0 };
			assert(tree->nodname.size() < 256);
			(*_pfdrawtext)( tree->nodname.c_str(), (int)tree->nodname.size(), &pos);
		}
	}
	else {
		for (int i = 0; i < tree->childs.size(); ++i)
			drawtrerec(tree->childs[i]);

		from = { OFFSET_MARGINS + (int)(tree->rootdist*_g_curprj->gmx + 0.5), (tree->childs[0]->p.y + tree->childs[tree->childs.size() - 1]->p.y) / 2 };
		to = { OFFSET_MARGINS + (int)((tree->rootdist + tree->dist)*_g_curprj->gmx + 0.5), from.y };
		tree->p = from;
		if (ClipLine(&from, &to, &_g_curprj->_gclipR))
			(*_pfdrawline)(from, to, _gc);

		from = tree->childs[0]->p;
		to = tree->childs[tree->childs.size() - 1]->p;
		if (ClipLine(&from, &to, &_g_curprj->_gclipR))
			(*_pfdrawline)(from, to, _gc);
	}
}


RGBQUAD * get_palette(TCHAR *sch, int);

static int DrawText_Width(HDC dc, TCHAR* text, int *pWidth)
{
	RECT pos = { 0 };
	int height = DrawText(dc, text, _tcslen(text), &pos, DT_CALCRECT);
	*pWidth = pos.right - pos.left;
	return height;
}


static void set_tree_metrics(NWKTREE_ENV* prpj, LONG x, LONG y, POINT *pictdims)
{
	bool fxpand = prpj->fExpand;

	int tree_height = prpj->proot->num_descend;
	int expanded_tree_height = OFFSET_MARGINS * (2 + prpj->proot->num_descend);
	int mul_ratio = (int)((tree_height / prpj->proot->maxChildDist) + 0.5);  // для "квадратности" дерева



	prpj->gmy = (fxpand == false && y) ? ((double)y) / tree_height : 1.0;
	prpj->gmx = (fxpand == false && x) ? ((double)x) / tree_height : 1.0, prpj->gmx *= mul_ratio;

	if (!prpj->_gm_hDC) {
		HDC hDC = GetDC(prpj->hWndDib);
		prpj->_gm_hDC = CreateCompatibleDC(hDC);
		LOGFONT* m_pLF = (LOGFONT*)calloc(1, sizeof(LOGFONT));
		_tcscpy_s(m_pLF->lfFaceName, TEXT("Times New Roman"));
		m_pLF->lfHeight = 14; // 0xfffffff3; 
		m_pLF->lfWidth = 0;
		m_pLF->lfWeight = FW_NORMAL;
		m_pLF->lfItalic = 0;
		m_pLF->lfQuality = DEFAULT_QUALITY;
		m_pLF->lfCharSet = DEFAULT_CHARSET;
		m_pLF->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
		HFONT hf = CreateFontIndirect(m_pLF);
		SelectObject(prpj->_gm_hDC, hf);
		SetBkMode(prpj->_gm_hDC, TRANSPARENT);
		SetMapMode(prpj->_gm_hDC, MM_TEXT);
		GetTextMetrics(prpj->_gm_hDC, &(prpj->_GTtm));
		delete m_pLF;
		ReleaseDC(prpj->hWndDib, hDC);
	}

	// y = 0 : number of descend (small); y = -1:  (number of descend + 2)*margins (big);  other: real window size
	int h = y ? (y != (LONG)(-1) ? y : expanded_tree_height)  : tree_height;
	int w = x ? x  : tree_height;

	h += 2 * OFFSET_MARGINS;
	w += 2 * OFFSET_MARGINS;


	RECT pos = { 0, 0, 0, 0 };
	if (fxpand == true) {
		DrawText(prpj->_gm_hDC, prpj->maxdistnode->nodname.c_str(), (int)prpj->maxdistnode->nodname.size(), &pos, DT_CALCRECT);
		w += pos.right;
		while (w * 3 & 3)  ++w; // Скорректировать ширину на длину текста
	}
	prpj->_gYBase = prpj->fAscend ? h : 0;

	prpj->_gclipR.left = prpj->_gclipR.top = 0;
	prpj->_gclipR.width = w - 1;
	prpj->_gclipR.height = h - 1 - prpj->_GTtm.tmHeight / 2;


	prpj->_gDibH = (fxpand == false) ? h : expanded_tree_height + 2 * OFFSET_MARGINS;
	prpj->_gDibW = (fxpand == false) ? w : tree_height + pos.right + 2 * OFFSET_MARGINS;


    {
		DWORD low_t = 0, high_t = 0;
		if (prpj->_g_prgb)  delete[] (prpj->_g_prgb), prpj->_g_prgb = NULL;
		getMinMaxTime(prpj, &low_t, &high_t);
		if (low_t && high_t) {
			prpj->_g_timebase = low_t; prpj->_g_timescale = ((double)NUM_PAL_ENTRIES) / (high_t - low_t);
			prpj->_g_prgb = get_palette((TCHAR*)TEXT("kRry"), NUM_PAL_ENTRIES);
		}
	}




	pictdims->x = w;
	pictdims->y = h;

}



void draw_legend(NWKTREE_ENV* prpj, LONG x, LONG y)
{
	// установим scalebar as 1/10 from width
	RECT pos;
	double scbar_len = prpj->proot->maxChildDist / 10;
	double nd = log10(scbar_len);
	double tenpow = round(nd);
	int numofdigits, len_legend;
	TCHAR pform[32], legend[32];
	scbar_len /= pow(10, tenpow);
	if (scbar_len <= 0.2)
		scbar_len = 0.2;
	else if (scbar_len <= 0.5)
		scbar_len = 0.5;
	else
		scbar_len = 1.0;
	scbar_len *= pow(10, tenpow);
	int sbar_line_len = (int)(prpj->gmx * scbar_len + 0.5);
	int legend_offset, llen;
	if (scbar_len >= 1)
		numofdigits = 0;
	else
		numofdigits = (int)fabs(tenpow) + 1;
	_stprintf(pform, TEXT("%%.%df"), numofdigits);
	_stprintf(legend, pform, scbar_len);

	DrawText(prpj->_gm_hDC, legend, llen=(int)_tcslen(legend), &pos, DT_CALCRECT);
	len_legend = pos.right - pos.left;
	legend_offset = (sbar_line_len - len_legend) / 2;
	
	POINT from = { x,  y + 2 };
	POINT to =   { x + sbar_line_len,  y + 2 };
	POINT imm;

	(*_pfdrawline)(from, to, _gc);
	from.y = to.y = y-1;
	imm = { x, y + 5 };
	(*_pfdrawline)(imm, from, _gc);
	to.x--,	imm.x = to.x;
	(*_pfdrawline)(imm, to, _gc);

	pos = { x + legend_offset, y + 2 + prpj->_GTtm.tmHeight / 4, 0, 0 };
	(*_pfdrawtext)(legend, llen, &pos);

#define GRAD_BAR_LEN 128

	 if (prpj->_g_prgb) {
		 
		 int diffh = prpj->_GTtm.tmHeight + 10;
		 int bar_y = y + diffh,
			 bar_x = x + (sbar_line_len - GRAD_BAR_LEN) / 2;

		 from = { bar_x,  prpj->fAscend ? y - diffh : bar_y };
		 to = from, to.y += prpj->fAscend ? -prpj->_GTtm.tmHeight : prpj->_GTtm.tmHeight;

		 for (int i = 0; i < NUM_PAL_ENTRIES; i += NUM_PAL_ENTRIES/ GRAD_BAR_LEN) {
			(*_pfdrawline)(from, to, prpj->_g_prgb[i]);
			++to.x; ++from.x;
		}

		SYSTEMTIME st;
		TCHAR tmbuf[32];
		FILETIME ft = { 0, _g_curprj->_g_timebase };
  // Draw left label
		FileTimeToSystemTime(&ft, &st);
		_stprintf(tmbuf, TEXT("%4d.%02d"), st.wYear, st.wMonth);
		DrawText(prpj->_gm_hDC, tmbuf, llen = (int)_tcslen(tmbuf), &pos, DT_CALCRECT);
		len_legend = pos.right - pos.left;
		bar_y += prpj->_GTtm.tmHeight;
		pos = { bar_x  - len_legend/2,   bar_y + 2 };
		(*_pfdrawtext)(tmbuf, llen, &pos);

  // Draw right label
		ft.dwHighDateTime +=  (DWORD)(NUM_PAL_ENTRIES/_g_curprj->_g_timescale);
		FileTimeToSystemTime(&ft, &st);
		_stprintf(tmbuf, TEXT("%4d.%02d"), st.wYear, st.wMonth);
		DrawText(prpj->_gm_hDC, tmbuf, llen = (int)_tcslen(tmbuf), &pos, DT_CALCRECT);
		len_legend = pos.right - pos.left;
		pos.left = bar_x + GRAD_BAR_LEN  - len_legend/2;
		(*_pfdrawtext)(tmbuf, llen, &pos);



	}



}


int drawTree(NWKTREE_ENV *prpj, LONG x, LONG y)
{
	int szpal = 0;
	bool fxpand = prpj->fExpand;
	POINT sz_pict;

	set_tree_metrics(prpj, x, y, &sz_pict);

	XB_DIBHDRPTR *pdibinfo, *hbitmap;
	BITMAPINFOHEADER *pbmph;
	if (prpj->gHbm) {
#if !defined _UZE_NEW_DIBIFACE
		GlobalFree(gHbm);
#else
		pdibinfo = prpj->gHbm;
		HeapFree(GetProcessHeap(), 0, pdibinfo->pbmihdr);
		SelectObject(prpj->_gm_hDC, pdibinfo->old_hbm);
		DeleteObject(pdibinfo->hbm);
		HeapFree(GetProcessHeap(), 0, pdibinfo);
		prpj->gHbm = NULL;
#endif
	}
	if (NULL != (hbitmap = XB_AllocImgBitmap(prpj->hWndDib, sz_pict.x, prpj->fAscend ? -sz_pict.y : sz_pict.y, TKXB_PCDFORM_RGB24))) {

		pdibinfo = prpj->gHbm = hbitmap;
		pdibinfo->old_hbm = (HBITMAP)SelectObject(prpj->_gm_hDC, pdibinfo->hbm);

#if !defined _UZE_NEW_DIBIFACE
		pbmph = (BITMAPINFOHEADER *)GlobalLock(hbitmap);
#else		
		pbmph = pdibinfo->pbmihdr;
#endif
		if (pbmph->biBitCount == 8)
		{
			RGBQUAD *ppal; //, *ppcdpal;
			szpal = 0x400;
			ppal = (RGBQUAD *)((char *)pbmph + sizeof(BITMAPINFOHEADER));
		}

		prpj->_gY = 0;
		prpj->_gYStep = (fxpand == false && y) ? 1 : OFFSET_MARGINS;
		EnterCriticalSection(&drtreeCRTS);
		_g_curprj = prpj;
		drawtrerec(prpj->proot);
		LeaveCriticalSection(&drtreeCRTS);

		draw_legend(prpj, 3 * OFFSET_MARGINS, sz_pict.y / 2);

#ifndef _NWK_TREE_STANDALONE
#if !defined _UZE_NEW_DIBIFACE
		SendMessage(prpj->hWndDib, DBWCM_SETOPTIONS, DBW_OPT_SET, (LPARAM)DBW_OPT_DIBHANDLE);
#else
		SendMessage(prpj->hWndDib, DBWCM_SETOPTIONS, DBW_OPT_SET, (LPARAM)DBW_OPT_DIBHDRPTR);
#endif
		SendMessage(prpj->hWndDib, DBWCM_SETHDIB, TRUE, (LPARAM)hbitmap);
		ShowWindow(prpj->treePH, SW_SHOW);
		ShowWindow(prpj->hWndDib, SW_SHOW);
#endif // _NWK_TREE_STANDALONE

	}
	return 0;
}

extern TKH_VM HVM;
void drawTreeWin(HWND hWnd, NWKTREE_ENV *proj)
{
	RECT rc; GetClientRect(hWnd, &rc);
	SCROLLBARINFO sbi = { 0 };
	sbi.cbSize = sizeof(sbi);

	if (GetScrollBarInfo(hWnd, OBJID_VSCROLL, &sbi)) {
		if (sbi.rgstate[0] == STATE_SYSTEM_OFFSCREEN || sbi.rgstate[0] == STATE_SYSTEM_INVISIBLE)
			rc.right -= HVM.xvscroll;
	}
	memset(sbi.rgstate, 0, sizeof(sbi.rgstate));
	sbi.cbSize = sizeof(sbi);
	if (GetScrollBarInfo(hWnd, OBJID_HSCROLL, &sbi)) {
		if (sbi.rgstate[0] == STATE_SYSTEM_OFFSCREEN || sbi.rgstate[0] == STATE_SYSTEM_INVISIBLE)
			rc.bottom -= HVM.yhscroll;
	}
	//
	rc.bottom -= 2 * OFFSET_MARGINS;
	rc.right -= 2 * OFFSET_MARGINS;
	drawTree(proj, rc.right, rc.bottom);
}



#include <stack>
// seed for non-recursive variant
static void contPreOrder(tag_treenode* top) {
	std::stack<tag_treenode*> st;

	while (top != NULL || !st.empty()) {		
		if (!st.empty()) {
			top = st.top(), st.pop();
		}
		while (top != NULL) {			
		// do some work	// top.treatment();
			size_t nchild = top->childs.size();
			if (nchild) { 
				for (size_t i = 0; i < nchild - 1; ++i)        // if (top.right != NULL) 
					st.push(top->childs[nchild - 1 - i]);          // st.push(top.right);
				top = top->childs[0];	                       // top = top.left;
			} 
		}
	}
}
