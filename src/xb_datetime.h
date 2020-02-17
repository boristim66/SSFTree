#ifndef _XB_DATETIME_H 
#define _XB_DATETIME_H

#include <time.h>
#include <limits>
#include <string>
#include <Windows.h>


#ifndef _TString
#ifdef _UNICODE
#define _TString wstring
#else
#define _TString string
#endif
#endif


void process_seq_date(std::_TString& date, time_t* begin, time_t* end);
bool mk_seq_date(std::_TString& date, FILETIME* out);

#endif
