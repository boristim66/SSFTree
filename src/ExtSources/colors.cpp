#include <string.h>
#include <stdlib.h>
#include <vector>
#include <TCHAR.h>

// Adapted from MathGL project

#ifdef  UNICODE                     
#define __TEXT(quote) L##quote      
#else   /* UNICODE */               
#define __TEXT(quote) quote         
#endif /* UNICODE */                
#define TEXT(quote) __TEXT(quote)   

typedef float mreal;
#define MGL_DEF_PAL	TEXT("bgrcmyhlnqeupH")	// default palette
#define MGL_DEF_SCH	TEXT("BbcyrR")	// default palette
#define MGL_COLORS	TEXT("kwrgbcymhWRGBCYMHlenpquLENPQU")

void mgl_chrrgb(TCHAR p, float c[3]);

const TCHAR *mglchr(const TCHAR *str, TCHAR ch)
{
	if (!str || !str[0])	return NULL;
	size_t l = _tcslen(str), k = 0;
	for (size_t i = 0; i<l; i++)
	{
		register TCHAR c = str[i];
		if (c == '{')	k++;
		if (c == '}')	k--;
		if (c == ch && k == 0)	return str + i;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
/// Class for RGBA color
struct  mglColor
{
	float r;	///< Red component of color
	float g;	///< Green component of color
	float b;	///< Blue component of color
	float a;	///< Alpha component of color

				/// Constructor for RGB components manualy
	mglColor(float R, float G, float B, float A = 1) :r(R), g(G), b(B), a(A) {}
	/// Constructor set default color
	mglColor() :r(0), g(0), b(0), a(1) {}
	/// Constructor set color from character id
	mglColor(TCHAR c, float bright = 1) { Set(c, bright); }
	/// Copy constructor
	mglColor(const mglColor &d) :r(d.r), g(d.g), b(d.b), a(d.a) {}
#if MGL_HAVE_RVAL
	mglColor(mglColor &&d) : r(d.r), g(d.g), b(d.b), a(d.a) {}
#endif
	/// Set color as Red, Green, Blue values
	void Set(float R, float G, float B, float A = 1) { r = R;	g = G;	b = B;	a = A; }
	/// Set color as Red, Green, Blue values
	void Set(mglColor c, float bright = 1)
	{
		if (bright<0)	bright = 0;	if (bright>2.f)	bright = 2.f;
		r = bright <= 1 ? c.r*bright : 1 - (1 - c.r)*(2 - bright);
		g = bright <= 1 ? c.g*bright : 1 - (1 - c.g)*(2 - bright);
		b = bright <= 1 ? c.b*bright : 1 - (1 - c.b)*(2 - bright);	a = 1;
	}
	/// Check if color is valid
	inline bool Valid()
	{
		return (r >= 0 && r <= 1 && g >= 0 && g <= 1 && b >= 0 && b <= 1 && a >= 0 && a <= 1);
	}
	/// Get maximal spectral component
	inline float Norm()
	{
		return r>g ? r : (g>b ? g : b);
	}
	inline float NormS()
	{
		return r*r + g*g + b*b;
	}
	/// Set color from symbolic id
	inline void Set(TCHAR p, float bright = 1)
	{
		float rgb[3];	mgl_chrrgb(p, rgb);
		Set(mglColor(rgb[0], rgb[1], rgb[2]), bright);
	}
	inline const mglColor &operator=(const mglColor &p)
	{
		r = p.r;	g = p.g;	b = p.b;	a = p.a;	return p;
	}
	/// Copy color from other one
	inline bool operator==(const mglColor &c) const
	{
		return (r - c.r)*(r - c.r) + (g - c.g)*(g - c.g) + (b - c.b)*(b - c.b) + (a - c.a)*(a - c.a) == 0;
	}
	//	{	return !memcmp(this, &c, sizeof(mglColor));	}
	inline bool operator!=(const mglColor &c) const
	{
		return (r - c.r)*(r - c.r) + (g - c.g)*(g - c.g) + (b - c.b)*(b - c.b) + (a - c.a)*(a - c.a) != 0;
	}
	//	{	return memcmp(this, &c, sizeof(mglColor));		}
	inline bool operator<(const mglColor &c) const
	{
		return memcmp(this, &c, sizeof(mglColor))<0;
	}
	// transparency still the same
	inline void operator*=(float v) { r *= v;	g *= v;	b *= v;	a *= v; }
	inline void operator+=(const mglColor &c) { r += c.r;	g += c.g;	b += c.b;	a += c.a; }
	inline void operator-=(const mglColor &c) { r -= c.r;	g -= c.g;	b -= c.b;	a -= c.a; }
};
#ifndef SWIG
inline mglColor operator+(const mglColor &a, const mglColor &b)
{
	return mglColor(a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a);
}
inline mglColor operator-(const mglColor &a, const mglColor &b)
{
	return mglColor(a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a);
}
inline mglColor operator*(const mglColor &a, float b)
{
	return mglColor(a.r*b, a.g*b, a.b*b, a.a*b);
}
inline mglColor operator*(float b, const mglColor &a)
{
	return mglColor(a.r*b, a.g*b, a.b*b, a.a*b);
}
inline mglColor operator/(const mglColor &a, float b)
{
	return mglColor(a.r / b, a.g / b, a.b / b, a.a / b);
}
inline mglColor operator!(const mglColor &a)
{
	return mglColor(1 - a.r, 1 - a.g, 1 - a.b, a.a);
}
#endif
//-----------------------------------------------------------------------------

static const mglColor NC(-1, -1, -1);
static const mglColor BC(0, 0, 0);
static const mglColor WC(1, 1, 1);
static const mglColor RC(1, 0, 0);

struct  mglColorID
{
	TCHAR id;
	mglColor col;
};
#pragma warning(disable:4305 4244)   

mglColorID mglColorIds[31] = { { 'k', mglColor(0,0,0) },
{ 'r', mglColor(1,0,0) },{ 'R', mglColor(0.5,0,0) },
{ 'g', mglColor(0,1,0) },{ 'G', mglColor(0,0.5,0) },
{ 'b', mglColor(0,0,1) },{ 'B', mglColor(0,0,0.5) },
{ 'w', mglColor(1,1,1) },{ 'W', mglColor(0.7,0.7,0.7) },
{ 'c', mglColor(0,1,1) },{ 'C', mglColor(0,0.5,0.5) },
{ 'm', mglColor(1,0,1) },{ 'M', mglColor(0.5,0,0.5) },
{ 'y', mglColor(1,1,0) },{ 'Y', mglColor(0.5,0.5,0) },
{ 'h', mglColor(0.5,0.5,0.5) },{ 'H', mglColor(0.3,0.3,0.3) },
{ 'l', mglColor(0,1,0.5) },{ 'L', mglColor(0,0.5,0.25) },
{ 'e', mglColor(0.5,1,0) },{ 'E', mglColor(0.25,0.5,0) },
{ 'n', mglColor(0,0.5,1) },{ 'N', mglColor(0,0.25,0.5) },
{ 'u', mglColor(0.5,0,1) },{ 'U', mglColor(0.25,0,0.5) },
{ 'q', mglColor(1,0.5,0) },{ 'Q', mglColor(0.5,0.25,0) },
{ 'p', mglColor(1,0,0.5) },{ 'P', mglColor(0.5,0,0.25) },
{ ' ', mglColor(-1,-1,-1) },{ 0, mglColor(-1,-1,-1) }	// the last one MUST have id=0
};
   
void mgl_chrrgb(TCHAR p, float c[3])
{
	c[0] = c[1] = c[2] = -1;
	for (long i = 0; mglColorIds[i].id; i++)
		if (mglColorIds[i].id == p)
		{
			c[0] = mglColorIds[i].col.r;
			c[1] = mglColorIds[i].col.g;
			c[2] = mglColorIds[i].col.b;
			break;
		}
}

#define MGL_TEXTURE_COLOURS 512
/// Structure for texture (color scheme + palette) representation
struct mglTexture
{
	mglColor *col;	///< Colors itself
	long n;				///< Number of initial colors along u

	TCHAR Sch[260];		///< Color scheme used
	int Smooth;			///< Type of texture (smoothing and so on)
	mreal Alpha;			///< Transparency

	mglTexture() :n(0), Smooth(0), Alpha(1)
	{
		col = new mglColor[MGL_TEXTURE_COLOURS];
	}
	mglTexture(const TCHAR *cols, int smooth = 0, mreal alpha = 1) :n(0)
	{
		col = new mglColor[MGL_TEXTURE_COLOURS];	Set(cols, smooth, alpha);
	}
	mglTexture(const mglTexture &aa) : n(aa.n), Smooth(aa.Smooth), Alpha(aa.Alpha)
	{
		col = new mglColor[MGL_TEXTURE_COLOURS];	memcpy(Sch, aa.Sch, 260);
		memcpy(col, aa.col, MGL_TEXTURE_COLOURS * sizeof(mglColor));
	}
#if MGL_HAVE_RVAL
	mglTexture(mglTexture &&aa) : n(aa.n), Smooth(aa.Smooth), Alpha(aa.Alpha)
	{
		col = aa.col;	memcpy(Sch, aa.Sch, 260);	aa.col = 0;
	}
#endif
	~mglTexture() { if (col)	delete[]col; }
	void Clear() { n = 0; }
	void Set(const TCHAR *cols, int smooth = 0, mreal alpha = 1);
//	void Set(HCDT val, const char *cols);
//	void GetC(mreal u, mreal v, mglPnt &p) const;
//	mglColor GetC(mreal u, mreal v = 0) const MGL_FUNC_PURE;
	inline bool IsSame(const mglTexture &t) const
	{
		return n == t.n && !memcmp(col, t.col, MGL_TEXTURE_COLOURS * sizeof(mglColor));
	}
	void GetRGBA(unsigned char *f) const;
	void GetRGBAPRC(unsigned char *f) const;
	void GetRGBAOBJ(unsigned char *f) const;	// Export repeating border colors, since OBJ by default wraps textures and we need an extra boundary to work around implementation quirks
	inline const mglTexture &operator=(const mglTexture &aa)
	{
		n = aa.n;	Smooth = aa.Smooth;	Alpha = aa.Alpha;
		memcpy(col, aa.col, MGL_TEXTURE_COLOURS * sizeof(mglColor));
		memcpy(Sch, aa.Sch, 260);	return aa;
	}
};


void mglTexture::Set(const TCHAR *s, int smooth, mreal alpha)
{
	// NOTE: New syntax -- colors are CCCCC or {CNCNCCCN}; options inside []
	if (!s || !s[0])	return;
	_tcsncpy(Sch, s, 259);	Smooth = smooth;	Alpha = alpha;

	register long i, j = 0, m = 0, l = (long)_tcslen(s);
	bool map = smooth == 2 || mglchr(s, '%'), sm = smooth >= 0 && !_tcschr(s, '|');	// Use mapping, smoothed colors
	for (i = n = 0; i<l; i++)		// find number of colors
	{
		if (smooth >= 0 && s[i] == ':' && j<1)	break;
		if (s[i] == '{' && _tcschr(MGL_COLORS"x", s[i + 1]) && j<1)	n++;
		if (s[i] == '[' || s[i] == '{')	j++;	if (s[i] == ']' || s[i] == '}')	j--;
		if (_tcschr(MGL_COLORS, s[i]) && j<1)	n++;
		//		if(smooth && s[i]==':')	break;	// NOTE: should use []
	}
	if (!n)
	{
		if (_tcschr(s, '|') && !smooth)	// sharp colors
		{
			n = l = 6;	s = MGL_DEF_SCH;	sm = false;
		}
		else if (smooth == 0)		// none colors but color scheme
		{
			n = l = 6;	s = MGL_DEF_SCH;
		}
	}
	if (n <= 0)	return;
	bool man = sm;
	mglColor *c = new mglColor[2 * n];		// Colors itself
	mreal *val = new mreal[n];
	for (i = j = n = 0; i<l; i++)	// fill colors
	{
		if (smooth >= 0 && s[i] == ':' && j<1)	break;
		if (s[i] == '[')	j++;	if (s[i] == ']')	j--;
		if (s[i] == '{')	m++;	if (s[i] == '}')	m--;
		if (_tcschr(MGL_COLORS, s[i]) && j<1 && (m == 0 || s[i - 1] == '{'))	// {CN,val} format, where val in [0,1]
		{
			if (m>0 && s[i + 1]>'0' && s[i + 1] <= '9')// ext color
			{
				c[2 * n].Set(s[i], (s[i + 1] - '0') / 5.f);	i++;
			}
			else	c[2 * n].Set(s[i]);	// usual color
			val[n] = -1;	c[2 * n].a = -1;	n++;
		}
		if (s[i] == 'x' && i>0 && s[i - 1] == '{' && j<1)	// {xRRGGBB,val} format, where val in [0,1]
		{
			uint32_t id = _tcstoul(s + 1 + i, 0, 16);
			if (memchr(s + i + 1, '}', 8) || memchr(s + i + 1, ',', 8))	c[2 * n].a = -1;
			else { c[2 * n].a = (id % 256) / 255.;	id /= 256; }
			c[2 * n].b = (id % 256) / 255.;	id /= 256;
			c[2 * n].g = (id % 256) / 255.;	id /= 256;
			c[2 * n].r = (id % 256) / 255.;
			while (_tcschr(TEXT("0123456789abcdefABCDEFx"), s[i]))	i++;
			val[n] = -1;	n++;	i--;
		}
		if (s[i] == ',' && m>0 && j<1 && n>0)
			val[n - 1] = _tstof(s + i + 1);
		// NOTE: User can change alpha if it placed like {AN}
		if (s[i] == 'A' && j<1 && m>0 && s[i + 1]>'0' && s[i + 1] <= '9')
		{
			man = false;	alpha = 0.1*(s[i + 1] - '0');	i++;
		}
	}
	for (long i = 0; i<n; i++)	// default texture
	{
		if (c[2 * i].a<0)	c[2 * i].a = alpha;
		c[2 * i + 1] = c[2 * i];
		if (man)	c[2 * i].a = 0;
	}
	if (map && sm && n>1)		// map texture
	{
		if (n == 2)
		{
			c[1] = c[2];	c[2] = c[0];	c[0] = BC;	c[3] = c[1] + c[2];
		}
		else if (n == 3)
		{
			c[1] = c[2];	c[2] = c[0];	c[0] = BC;	c[3] = c[4];	n = 2;
		}
		else
		{
			c[1] = c[4];	c[3] = c[6];	n = 2;
		}
		c[0].a = c[1].a = c[2].a = c[3].a = alpha;
		val[0] = val[1] = -1;
	}
	// TODO if(!sm && n==1)	then try to find color in palette ???

	// fill missed values  of val[]
	float  v1 = 0, v2 = 1;
	std::vector <long>  def;
	val[0] = 0;	val[n - 1] = 1;	// boundary have to be [0,1]
	for (long i = 0; i<n; i++) if (val[i]>0 && val[i]<1) 	def.push_back(i);
	def.push_back(n - 1);
	long i1 = 0, i2;
	for (size_t j = 0; j<def.size(); j++)	for (i = i1 + 1; i<def[j]; i++)
	{
		i1 = j>0 ? def[j - 1] : 0;	i2 = def[j];
		v1 = val[i1];	v2 = val[i2];
		v2 = i2 - i1>1 ? (v2 - v1) / (i2 - i1) : 0;
		val[i] = v1 + v2*(i - i1);
	}
	// fill texture itself
	mreal v = sm ? (n - 1) / 255. : n / 256.;
	if (!sm)	for (long i = 0; i<256; i++)
	{
		register long j = 2 * long(v*i);	//u-=j;
		col[2 * i] = c[j];	col[2 * i + 1] = c[j + 1];
	}
	else	for (i = i1 = 0; i<256; i++)
	{
		register mreal u = v*i;	j = long(u);	//u-=j;
		if (j<n - 1)	// advanced scheme using val
		{
			for (; i1<n - 1 && i >= 255 * val[i1]; i1++);
			v2 = i1<n ? 1 / (val[i1] - val[i1 - 1]) : 0;
			j = i1 - 1;	u = (i / 255. - val[j])*v2;
			col[2 * i] = c[2 * j] * (1 - u) + c[2 * j + 2] * u;
			col[2 * i + 1] = c[2 * j + 1] * (1 - u) + c[2 * j + 3] * u;
		}
		else
		{
			col[2 * i] = c[2 * n - 2]; col[2 * i + 1] = c[2 * n - 1];
		}
	}
	delete[]c;	delete[]val;
}

void mglTexture::GetRGBA(unsigned char *f) const
{
	for (long i = 255; i >= 0; i--)
	{
		mglColor c1 = col[2 * i], c2 = col[2 * i + 1];
		for (long j = 0; j<256; j++)
		{
			register long i0 = 4 * (j + 256 * i);
			mglColor c = c1 + (c2 - c1)*(j / 255.);
			f[i0] = int(255 * c.r);
			f[i0 + 1] = int(255 * c.g);
			f[i0 + 2] = int(255 * c.b);
			f[i0 + 3] = int(255 * c.a);
		}
	}
}

void mglTexture::GetRGBAOBJ(unsigned char *f) const
{
	const size_t bw = 128; //border width
	register size_t i, j, i0;
	mglColor c1, c2, c;
	for (i = 0; i<256; i++)
	{
		c1 = col[2 * i];  c2 = col[2 * i + 1];
		for (j = 0; j<512; j++)
		{
			i0 = 4 * (j + 512 * (255 - i));
			if (j<bw)
				c = c1;
			else if (j>511 - bw)
				c = c2;
			else
				c = c1 + (c2 - c1)*((j - bw) / (255.));
			f[i0] = int(255 * c.r);
			f[i0 + 1] = int(255 * c.g);
			f[i0 + 2] = int(255 * c.b);
			f[i0 + 3] = int(255 * c.a);
		}
	}

}



void mglTexture::GetRGBAPRC(unsigned char *f) const
{
	register size_t i, j, i0;
	mglColor c1, c2, c;
	for (i = 0; i<256; i++)
	{
		c1 = col[2 * i];	c2 = col[2 * i + 1];
		for (j = 0; j<256; j++)
		{
			i0 = 4 * (j + 256 * (255 - i));
			c = c1 + (c2 - c1)*(j / 255.);
			f[i0]     = int(255 * c.r);
			f[i0 + 1] = int(255 * c.g);
			f[i0 + 2] = int(255 * c.b);
			f[i0 + 3] = int(255 * c.a);
		}
	}
}

typedef struct tagRGBQUAD {
	unsigned char    rgbBlue;
	unsigned char    rgbGreen;
	unsigned char    rgbRed;
	unsigned char    rgbReserved;
} RGBQUAD;


RGBQUAD * get_palette(TCHAR *sch, int num)
{
	if (num > MGL_TEXTURE_COLOURS) return NULL;
	mglTexture *t = new mglTexture(sch);
	RGBQUAD *r = new RGBQUAD[num];
	for (int i = 0; i < num; ++i) {
		r[i].rgbBlue     = (255 * t->col[i].b);
		r[i].rgbGreen    = (255 * t->col[i].g);
		r[i].rgbRed      = (255 * t->col[i].r);
		r[i].rgbReserved = (255 * t->col[i].a);
	} delete t;
	return r;
}

#pragma warning(default:4305 4244)