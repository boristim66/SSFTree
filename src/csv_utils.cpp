#include <stdio.h>
#include <vector>
#include <string>
#include <tchar.h>
#include "csv_utils.h"
#include <windows.h>


//  Преобразует строку для записи в csv-файл, добавляя, если надо, ESC-символы - кавычки
extern "C" int  c_csv_escape(std::_TString &inout, TCHAR delim_sym )
{
size_t pos = 0;
int nfound = 0;
	//  Все кавычки эскейпятся в любом случае
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

// Читает csv-файл в вектор строк.
// Ожидается, что первая строка (строка заголовков), как минимум, определяет ширину таблицы
// v_default - заполнение по умолчанию для "укороченных" строк
// Возвращает 0 в случае успеха или номер строки + столбца (в человеческой нумерации), если ошибка


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
				if ('\"' == ch) { // первую кавычку всегда пропускаем
					firstQuote = 1;
					continue; // брать следующий символ
				}
			}
			else { // предыдущий символ - кавычка
				firstQuote = 0;  // предыдущий символ - кавычка
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
			else { // новая строка вне кавычек: Заканчивает строку.
				if (!vs) vs = new std::vector<std::_TString>;
				vs->push_back(ss);
				//  Если вектор содержит только пустые строки (например, последние строки файла), выброим его 
				for (j = 0; j < vs->size(); ++j) // Проверка пустых строк
					if ((*vs)[j].size())
						break;

				if (j != vs->size()) { // Есть непустые строки
					stv.push_back(vs);
					if (!dim_vs)
						dim_vs = (int)vs->size(); // Количество заголовков определяем по первой строке
					else {
						if (dim_vs != vs->size()) {
							if (vs->size() > dim_vs) {
								// Если все значения после dim_vs пустые - укоротить
								for (j = dim_vs; j < vs->size(); ++j)
									if ((*vs)[j].size())
										break;
								if (j == vs->size()) // Все пустые
									vs->erase(vs->begin() + dim_vs, vs->end());
								else {
									retcode = (int)FORMAT_RETCODE(row_no, vs->size());
									goto Completed;
								}
							}
							else if (v_default) {
								do { // Дополнить укороченные строки значением по умолчанию
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

	// По несчастью, последняя строка может не иметь \n ?
	if (vs && vs->size()) {
		vs->push_back(ss);
		//  Если вектор содержит только пустые строки (например, последние строки файла), выброим его 
		for (j = 0; j < vs->size(); ++j) // Проверка пустых строк
			if ((*vs)[j].size())
				break;
		if (j != vs->size()) { // Есть непустые строки
			stv.push_back(vs);
			if (dim_vs)
				if (dim_vs != vs->size()) {
					if (vs->size() > dim_vs) {
						// Если все значения после dim_vs пустые - укоротить
						for (j = dim_vs; j < vs->size(); ++j)
							if ((*vs)[j].size())
								break;
						if (j == vs->size()) // Все пустые
							vs->erase(vs->begin() + dim_vs, vs->end());
						else {
							retcode = (int)FORMAT_RETCODE(row_no, vs->size());
							goto Completed;
						}
					}
					else if (v_default) {
						do { // Дополнить укороченные строки значением по умолчанию
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


// Читает csv-файл, вызывая пользовательский callback для каждой позиции
// Ожидается, что первая строка (строка заголовков), как минимум, определяет ширину таблицы
// v_default - заполнение по умолчанию для "укороченных" строк
// Возвращает 0 в случае успеха или номер строки + столбца (в человеческой нумерации), если ошибка
// Случаи пустых строки и строк длиной более длины заголовка должны обрабатываться пользовательской функцией 

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
				if ('\"' == ch) { // первую кавычку всегда пропускаем
					firstQuote = 1;
					continue; // брать следующий символ
				}
			}
			else { // предыдущий символ - кавычка
				firstQuote = 0;  // предыдущий символ - кавычка
				if ('\"' != ch)
					inQuoted = !inQuoted;
			}
			if ('\"' == ch || inQuoted)
				goto PutSymbol;

			if (delim_sym == ch) {
				if ((*cb)(ss.c_str(), param, row_no, col_no)) {// Вызов пользовательской функции для размещения данных
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
			else { // новая строка вне кавычек: Заканчивает строку.
				if ((*cb)(ss.c_str(), param, row_no, col_no)) { // Вызов пользовательской функции для размещения данных
					retcode = (int)FORMAT_RETCODE(row_no, col_no);
					goto Completed;
				}
				++col_no;
				if (!dim_vs)
					dim_vs = col_no; // Количество заголовков определяем по первой строке
				else {
					if (dim_vs != col_no) {
						if (col_no > dim_vs) {
							// Пользовательская функция должна следить за этим!
						}
						else if (v_default) {
							do { // Дополнить укороченные строки значением по умолчанию
								if ((*cb)(v_default, param, row_no, col_no)) { // Вызов пользовательской функции для размещения данных
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

	// По несчастью, последняя строка может не иметь \n ?
	if (col_no) {
		if ((*cb)(ss.c_str(), param, row_no, col_no)) // Вызов пользовательской функции для размещения данных
			return (row_no + 1) | ((col_no + 1) << 16);
		++col_no;
		if (dim_vs)
			if (dim_vs != col_no) {
				if (col_no > dim_vs) {
					// Пользовательская функция должна следить за этим!
				}
				else if (v_default) {
					do { // Дополнить укороченные строки значением по умолчанию
						if ((*cb)(v_default, param, row_no, col_no)) {// Вызов пользовательской функции для размещения данных
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

//  Печать в csv

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