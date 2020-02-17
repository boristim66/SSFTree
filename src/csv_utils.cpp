#include <stdio.h>
#include <vector>
#include <string>
#include <tchar.h>
#include "csv_utils.h"
#include <windows.h>


//  ����������� ������ ��� ������ � csv-����, ��������, ���� ����, ESC-������� - �������
extern "C" int  c_csv_escape(std::_TString &inout, TCHAR delim_sym )
{
size_t pos = 0;
int nfound = 0;
	//  ��� ������� ���������� � ����� ������
	while (std::_TString::npos != (pos = inout.find('\"', pos)))
		inout.insert(pos, 1, '\"'), nfound++, pos += 2;

	if (nfound || std::_TString::npos != inout.find(delim_sym) || std::_TString::npos != inout.find('\n')) {
		inout.insert(0, 1, '\"'), nfound++;
		inout.append(1, '\"');
	}
	return nfound;
}

extern "C" void c_csv_cleaninfo(std::vector< std::vector<std::_TString> *> &stv) 
{
	for (size_t i = 0; i < stv.size(); ++i)
		delete stv[i];
	stv.clear();
}

// ������ csv-���� � ������ �����.
// ���������, ��� ������ ������ (������ ����������), ��� �������, ���������� ������ �������
// v_default - ���������� �� ��������� ��� "�����������" �����
// ���������� 0 � ������ ������ ��� ����� ������ + ������� (� ������������ ���������), ���� ������


extern "C" int c_csv_parser(FILE *csvin, std::vector< std::vector<std::_TString> *> &stv, TCHAR delim_sym, TCHAR *v_default )
{
bool fCompletedRow;
int inQuoted = 0, firstQuote = 0;
size_t i, j, size = 1024, lsize;
TCHAR *buf, ch;
buf = new TCHAR[size];
std::vector<std::_TString> *vs = NULL;
std::_TString ss;
int dim_vs = 0, row_no = 0, retcode = 0;

	while (buf[size - 1] = (TCHAR)(-1), _fgetts(buf, (int)size, csvin)) {

		lsize = (0 != (fCompletedRow = (buf[size - 1] != (TCHAR)(-1)))) ? size - 1 : _tcslen(buf);
		for (i = 0; i < lsize; ++i) {
			ch = buf[i];
			if (0 == firstQuote) {
				if ('\"' == ch) { // ������ ������� ������ ����������
					firstQuote = 1;
					continue; // ����� ��������� ������
				}
			}
			else { // ���������� ������ - �������
				firstQuote = 0;  // ���������� ������ - �������
				if ('\"' != ch)
					inQuoted = !inQuoted;
			}
			if ('\"' == ch || inQuoted)
				goto PutSymbol;

			if (delim_sym == ch) {
				if (!vs) vs = new std::vector<std::_TString>;
				vs->push_back(ss);
				ss.clear();
			}
			else if ('\n' != ch) {
PutSymbol:
				ss += ch;
			}
			else { // ����� ������ ��� �������: ����������� ������.
				if (!vs) vs = new std::vector<std::_TString>;
				vs->push_back(ss);
				//  ���� ������ �������� ������ ������ ������ (��������, ��������� ������ �����), ������� ��� 
				for (j = 0; j < vs->size(); ++j) // �������� ������ �����
					if ((*vs)[j].size())
						break;

				if (j != vs->size()) { // ���� �������� ������
					stv.push_back(vs);
					if (!dim_vs)
						dim_vs = (int)vs->size(); // ���������� ���������� ���������� �� ������ ������
					else {
						if (dim_vs != vs->size()) {
							if (vs->size() > dim_vs) {
								// ���� ��� �������� ����� dim_vs ������ - ���������
								for (j = dim_vs; j < vs->size(); ++j)
									if ((*vs)[j].size())
										break;
								if (j == vs->size()) // ��� ������
									vs->erase(vs->begin() + dim_vs, vs->end());
								else {
									retcode = (int)FORMAT_RETCODE(row_no, vs->size());
									goto Completed;
								}
							}
							else if (v_default) {
								do { // ��������� ����������� ������ ��������� �� ���������
									vs->push_back(v_default);
								} while (vs->size() < dim_vs);
							}
							else {
								retcode = (int)FORMAT_RETCODE(row_no, vs->size());
								goto Completed;
							}
						}
					}
					vs = NULL;
				}
				else {
					vs->clear();
				}
				ss.clear();
				row_no++;
			}
		}
	}

	// �� ���������, ��������� ������ ����� �� ����� \n ?
	if (vs && vs->size()) {
		vs->push_back(ss);
		//  ���� ������ �������� ������ ������ ������ (��������, ��������� ������ �����), ������� ��� 
		for (j = 0; j < vs->size(); ++j) // �������� ������ �����
			if ((*vs)[j].size())
				break;
		if (j != vs->size()) { // ���� �������� ������
			stv.push_back(vs);
			if (dim_vs)
				if (dim_vs != vs->size()) {
					if (vs->size() > dim_vs) {
						// ���� ��� �������� ����� dim_vs ������ - ���������
						for (j = dim_vs; j < vs->size(); ++j)
							if ((*vs)[j].size())
								break;
						if (j == vs->size()) // ��� ������
							vs->erase(vs->begin() + dim_vs, vs->end());
						else {
							retcode = (int)FORMAT_RETCODE(row_no, vs->size());
							goto Completed;
						}
					}
					else if (v_default) {
						do { // ��������� ����������� ������ ��������� �� ���������
							vs->push_back(v_default);
						} while (vs->size() < dim_vs);
					}
					else {
						retcode = (int)FORMAT_RETCODE(row_no, vs->size());
						goto Completed;
					}
				}
		}
	}
Completed:
	delete[]buf;
	return retcode;
}


// ������ csv-����, ������� ���������������� callback ��� ������ �������
// ���������, ��� ������ ������ (������ ����������), ��� �������, ���������� ������ �������
// v_default - ���������� �� ��������� ��� "�����������" �����
// ���������� 0 � ������ ������ ��� ����� ������ + ������� (� ������������ ���������), ���� ������
// ������ ������ ������ � ����� ������ ����� ����� ��������� ������ �������������� ���������������� �������� 

extern "C" int c_csv_parser_cb(FILE *csvin, csvcbfun cb, void *param, TCHAR delim_sym, TCHAR *v_default)
{
bool fCompletedRow;
int inQuoted = 0, firstQuote = 0;
size_t i,  size = 1024, lsize;
TCHAR *buf, ch;
buf = new TCHAR[size];
std::_TString ss;
int dim_vs = 0, row_no = 0, col_no = 0;
int retcode = 0;

	while (buf[size - 1] = (TCHAR)(-1), _fgetts(buf, (int)size, csvin)) {

		lsize = (0 != (fCompletedRow = (buf[size - 1] != (TCHAR)(-1)))) ? size - 1 : _tcslen(buf);
		for (i = 0; i < lsize; ++i) {
			ch = buf[i];
			if (0 == firstQuote) {
				if ('\"' == ch) { // ������ ������� ������ ����������
					firstQuote = 1;
					continue; // ����� ��������� ������
				}
			}
			else { // ���������� ������ - �������
				firstQuote = 0;  // ���������� ������ - �������
				if ('\"' != ch)
					inQuoted = !inQuoted;
			}
			if ('\"' == ch || inQuoted)
				goto PutSymbol;

			if (delim_sym == ch) {
				if ((*cb)(ss.c_str(), param, row_no, col_no)) {// ����� ���������������� ������� ��� ���������� ������
					retcode = (int)FORMAT_RETCODE(row_no, col_no);
					goto Completed;
				}
				ss.clear();
				++col_no;
			}
			else if ('\n' != ch) {
PutSymbol:
				ss += ch;
			}
			else { // ����� ������ ��� �������: ����������� ������.
				if ((*cb)(ss.c_str(), param, row_no, col_no)) { // ����� ���������������� ������� ��� ���������� ������
					retcode = (int)FORMAT_RETCODE(row_no, col_no);
					goto Completed;
				}
				++col_no;
				if (!dim_vs)
					dim_vs = col_no; // ���������� ���������� ���������� �� ������ ������
				else {
					if (dim_vs != col_no) {
						if (col_no > dim_vs) {
							// ���������������� ������� ������ ������� �� ����!
						}
						else if (v_default) {
							do { // ��������� ����������� ������ ��������� �� ���������
								if ((*cb)(v_default, param, row_no, col_no)) { // ����� ���������������� ������� ��� ���������� ������
									retcode = (int)FORMAT_RETCODE(row_no, col_no);
									goto Completed;
								}
							} while (++col_no < dim_vs);
						}
						else {
							retcode = (int)FORMAT_RETCODE(row_no, col_no);
							goto Completed;
						}
					}
				}
				ss.clear();
				row_no++;
				col_no = 0;
			} // End '/n'
		}
	}

	// �� ���������, ��������� ������ ����� �� ����� \n ?
	if (col_no) {
		if ((*cb)(ss.c_str(), param, row_no, col_no)) // ����� ���������������� ������� ��� ���������� ������
			return (row_no + 1) | ((col_no + 1) << 16);
		++col_no;
		if (dim_vs)
			if (dim_vs != col_no) {
				if (col_no > dim_vs) {
					// ���������������� ������� ������ ������� �� ����!
				}
				else if (v_default) {
					do { // ��������� ����������� ������ ��������� �� ���������
						if ((*cb)(v_default, param, row_no, col_no)) {// ����� ���������������� ������� ��� ���������� ������
							retcode = (int)FORMAT_RETCODE(row_no, col_no);
							goto Completed;
						}
					} while (++col_no < dim_vs);
				}
				else {
					retcode = (int)FORMAT_RETCODE(row_no, col_no);
					goto Completed;
				}
			}
	}
Completed:
	delete[]buf;
	return retcode;
}

//  ������ � csv

extern "C" int c_csv_writer_cb(FILE *csvout, csvget cb, void *param)
{
int i, j, fContinue = true;
int col_couner = INT_MAX;
const TCHAR *pwr;
std::_TString s;
	for (i = 0;  fContinue;  ++i ) {
		for (j = 0; j <col_couner; ++j) {
			if (NULL == (pwr = (*cb)(param, i, j))) {
				if ( i == 0 ) 	col_couner = j;
				if ( j == 0 )	fContinue = false;
				break;
			}
			if (_tcslen(pwr) != _tcscspn(pwr, TEXT("\";\n"))) 			     c_csv_escape(s = pwr), 	pwr = s.c_str();
			if (_ftprintf(csvout, j ? TEXT(";%s") : TEXT("%s"), pwr) < 0)	return -1;
		}
		if (_ftprintf(csvout, TEXT("\n")) < 0)	return -1;
	}
	return 0;
}


void Write_to_csv(const TCHAR* fn, std::vector< std::vector<std::_TString>*>& stv)
{
	TCHAR symdelim = ';';
	std::_TString myfn;
	const TCHAR* fileext = _tcsrchr(fn, '.');
	FILE* csvo;

	if (fileext && !_tcsicmp(fileext + 1, TEXT("tsv")))
		symdelim = '\t';

	if (0 == _tfopen_s(&csvo, fn, TEXT("wt")) ) {
		for (size_t i = 0; i < stv.size(); ++i) {
			for (size_t j = 0; j < stv[i]->size(); ++j) {
				myfn = (*stv[i])[j]; c_csv_escape(myfn, symdelim);
				if (j)
					_ftprintf(csvo, TEXT("%c%s"), symdelim, myfn.c_str());
				else
					_ftprintf(csvo, TEXT("%s"), myfn.c_str());
			}
			_ftprintf(csvo, TEXT("\n"));
		}
		fclose (csvo);
	}
}