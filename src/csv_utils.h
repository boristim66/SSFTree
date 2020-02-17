#pragma once

#ifndef _CSV_UTILS_H
#define _CSV_UTILS_H

//  ���������� retcode ����� ������������� ��� ������ �������� �� 256 ��� ����� � 16 ���.������� 
#define FORMAT_RETCODE(r,c) ((r + 1) | ((c + 1) << 18))
#define ROW_FRON_RETCODE(code) (code & 0x3FFFF)
#define COL_FRON_RETCODE(code) (code >> 18)

#ifndef _TString
	#ifdef _UNICODE
		#define _TString wstring
	#else
		#define _TString string
	#endif
#endif


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

int  c_csv_escape(std::    _TString &inout, TCHAR delim_sym = ';');
int  c_csv_parser(FILE *csvin, std::vector< std::vector<std::_TString> *> &stv, TCHAR delim_sym = ';', TCHAR *v_default=NULL);
typedef int (__cdecl *csvcbfun)(const TCHAR *, void *context, int row, int col);
// csvcbfun ������ ���������� ��������� ��������, ���� �������� ������ � ���� ����������, � ��������� ������ 0
int  c_csv_parser_cb(FILE *csvin, csvcbfun cb, void *param,  TCHAR delim_sym = ';', TCHAR *v_default= NULL);

typedef const TCHAR * (__cdecl *csvget)(void *context, int row, int col);
int  c_csv_writer_cb(FILE *csvout, csvget cb, void *param);
void c_csv_cleaninfo(std::vector< std::vector<std::_TString> *> &stv);
#ifdef __cplusplus
}   /* ... extern "C" */
#endif  /* __cplusplus */
#endif