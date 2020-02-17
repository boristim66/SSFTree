#include "xb_datetime.h"
#include <limits>
#include <Windows.h>
#include <TCHAR.h>
#include <string>

#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

#ifndef _NWK_TREE_STANDALONE
extern "C" LRESULT VisualiseString(LPTSTR format, ...);
extern "C" LRESULT VisualiseStringAsync(LPTSTR format, ...);
#else
#define VisualiseString
#define isInternalDbUsed(a)  0
#endif




inline
void UnixTimeToFileTime(time_t t, LPFILETIME pft)
{
	// Note that LONGLONG is a 64-bit value
	LONGLONG ll;

	ll = Int32x32To64(t, WINDOWS_TICK) + SEC_TO_UNIX_EPOCH * WINDOWS_TICK;
	pft->dwLowDateTime = (DWORD)ll;
	pft->dwHighDateTime = ll >> 32;
}

inline
unsigned FileTimeToUnixTime(FILETIME* windowsTicks)
{
	return (unsigned)(*(long long*)windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}


short _get_max_day(short month, int year) {
	if (month == 0 || month == 2 || month == 4 || month == 6 || month == 7 || month == 9 || month == 11)
		return 31;
	else if (month == 3 || month == 5 || month == 8 || month == 10)
		return 30;
	else {
		if (year % 4 == 0) {
			if (year % 100 == 0) {
				if (year % 400 == 0)
					return 29;
				return 28;
			}
			return 29;
		}
		return 28;
	}
}

// case-insencitive version of strstr
const TCHAR* my_tstristr(const TCHAR* haystack, const TCHAR* needle)
{
	if (!*needle)
	{
		return haystack;
	}
	for (; *haystack; ++haystack)
	{
		if (toupper(*haystack) == toupper(*needle))
		{
			/*
			 * Matched starting char -- loop through remaining chars.
			 */
			const TCHAR* h, * n;
			for (h = haystack, n = needle; *h && *n; ++h, ++n)
			{
				if (toupper(*h) != toupper(*n))
				{
					break;
				}
			}
			if (!*n) /* matched all of 'needle' to null termination */
			{
				return haystack; /* return the start of the match */
			}
		}
	}
	return 0;
}

#define _IS_LIKE_YEAR(a) ((a) >= 1900 && (a) <= 2100)
#define _IS_LIKE_DATE_DELIM(a) ((a) == '.' || (a) == '-' || (a) == '/')
#define _IS_LIKE_MONTH(a) ((a) >= 1 && (a) <= 12 )

static const TCHAR* months[] = {
	TEXT("JAN"), TEXT("FEB"), TEXT("MAR"), TEXT("APR"),
	TEXT("MAY"), TEXT("JUN"), TEXT("JUL"), TEXT("AUG"),
	TEXT("SEP"), TEXT("OCT"), TEXT("NOV"), TEXT("DEC")
};


bool isMonth(const TCHAR* str, int* mon)
{
	int m = -1;
	if (isdigit(*str) && 1 == _stscanf(str, TEXT("%d"), &m)) {
		*mon = m;
		return _IS_LIKE_MONTH(m);
	}
	else if (isalpha(*str)) {
		for (m = 0; m < (int)sizeof(months) / sizeof(months[0]); ++m)
			if (my_tstristr(str, months[m]))
				break;
		*mon = ++m;
		return _IS_LIKE_MONTH(m);
	}
	*mon = -1;
	return false;
}

// Main "intellectual" engine for date parsing in different forms, try to eat user's shit
bool _parse_datestring(const TCHAR* str, FILETIME* out, int* order)
{
	int i, l, cnt;
	size_t n[3];
	SYSTEMTIME t0 = { 0 };
	const TCHAR* p = str;
	*order = 1;
	if (!_tcslen(str)) return false;

	for (i = 0; i < 3; ++i) {
		if ((n[i] = _tcscspn(p, TEXT(".-/"))) == _tcslen(p))
			break;  //no more delimiters
		p += n[i] + 1;
	}
	switch (i) {
	case 0:  // Year only,  must be 2 or 4 digits
		if (1 == _stscanf(str, TEXT("%d%n"), &l, &cnt) && (cnt == 2 || cnt == 4)) {
			if (cnt == 2) {
				if (l < 60) l += 2000;
				else        l += 1900;
			}
			if (_IS_LIKE_YEAR(l)) {
				t0.wDay = 1;
				t0.wMonth = 1;
				t0.wYear = l;
				t0.wHour = 2;
				return (0 != SystemTimeToFileTime(&t0, out));
			}
		} break;
	case 1: // Year and month, year must have 4 digit
		if (1 == _stscanf(str, TEXT("%d"), &l)) { // Number first
			if (_IS_LIKE_YEAR(l)) { // Lets year first
				t0.wYear = l;
				if (isMonth(str + n[0] + 1, &l)) { // Month in text or digital form
					t0.wHour = 1;
					t0.wMonth = l;
					t0.wDay = 1;
					return (0 != SystemTimeToFileTime(&t0, out));
				}
			}
			else if (_IS_LIKE_MONTH(l)) { // assume month in digital form
				t0.wHour = 1;
				t0.wMonth = l;
				t0.wDay = 1;
				if (1 == _stscanf(str + n[0] + 1, TEXT("%d"), &l) && _IS_LIKE_YEAR(l)) { // year must have 4 digit
					t0.wYear = l;
					*order = -1;
					return (0 != SystemTimeToFileTime(&t0, out));
				}
			}
		}
		else {  // May be Text first, assume month as text,  year must be 2 or 4 digits
			if (isMonth(str, &l)) {
				t0.wMonth = l;
				t0.wDay = 1;
				t0.wHour = 1;
				if (1 == _stscanf(str + n[0] + 1, TEXT("%d%n"), &l, &cnt) && (cnt == 2 || cnt == 4)) {
					if (cnt == 2) {
						if (l < 60) l += 2000;
						else        l += 1900;
					}
					if (_IS_LIKE_YEAR(l)) {
						t0.wYear = l;
						*order = -1;
						return (0 != SystemTimeToFileTime(&t0, out));
					}
				}
			}
		}
		break;
	case 2:  // YEAR-MONTH-DAY or DAY-MONTH-YEAR 
		if (isMonth(str + n[0] + 1, &l)) { // SECOND - MONTH in TEXT or DIGITAL Form
			t0.wMonth = l;
			if (1 == _stscanf(str, TEXT("%d"), &l)) {
				if (_IS_LIKE_YEAR(l)) { // YEAR FIRST
					t0.wYear = l;
					if (1 == _stscanf(str + 2 + n[0] + n[1], TEXT("%d"), &l) && l > 0 && l <= _get_max_day(t0.wMonth - 1, t0.wYear)) {
						t0.wDay = l;
						return (0 != SystemTimeToFileTime(&t0, out));
					}
				}
				else {
					t0.wDay = l;
					if (l > 0 && 1 == _stscanf(str + 2 + n[0] + n[1], TEXT("%d"), &l) &&
						_IS_LIKE_YEAR(l) && t0.wDay <= _get_max_day(t0.wMonth - 1, l)) {
						t0.wYear = l;
						*order = -1;
						return (0 != SystemTimeToFileTime(&t0, out));
					}
				}
			}
		}

	default: break;

	}
	return false;
}

bool parse_datestring(const TCHAR* str, FILETIME* out, int* order)
{
	bool result = _parse_datestring(str, out, order);
	if (!result && _tcslen(str)) {
		VisualiseString(TEXT("Error: Unknown date format\"%s\"\n"), str);
	}
	return result;
}


int like_data(const TCHAR* date, FILETIME* out)
{
	int ord = 0;
	int ret = _parse_datestring(date, out, &ord);
	return  (ret) ? ord : 0;
}

bool mk_seq_date(std::_TString& date, FILETIME* out)
{
	int ord = 0;
	int result = parse_datestring(date.c_str(), out, &ord);
	return result;
}

void process_seq_date(std::_TString& date, time_t* begin, time_t* end)
{

	int ord = 0;
	FILETIME dt;
	if (parse_datestring(date.c_str(), &dt, &ord)) {
		time_t l = FileTimeToUnixTime(&dt);
		if (l < *begin)
			*begin = l;
		if (l > * end)
			*end = l;
	}
}
