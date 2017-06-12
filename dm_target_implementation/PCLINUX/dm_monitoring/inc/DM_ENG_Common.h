/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C) 2014 Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'. 
 *
 *---------------------------------------------------------------------------
 * File        : DM_ENG_Common.h
 *
 * Created     : 22/05/2008
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

/**
 * @file DM_ENG_Common.h
 *
 * @brief Common definitions and all-purpose functions
 *
 **/

#ifndef _DM_ENG_COMMON_H_
#define _DM_ENG_COMMON_H_

#include "CMN_Type_Def.h"
#include <malloc.h>
#include <string.h>

#define DM_ENG_FREE(p) { if (p!=NULL) { free(p); p=NULL; } }

extern const char  DM_ENG_FALSE_CHAR;
extern const char  DM_ENG_TRUE_CHAR;
extern const char* DM_ENG_FALSE_STRING;
extern const char* DM_ENG_TRUE_STRING;

extern const char* DM_ENG_EMPTY;
extern const char* DM_ENG_UNKNOWN_TIME;

// fonctions de conversion
int DM_ENG_charToInt(char ch);
bool DM_ENG_stringToInt(char* ch, int* pRes);
bool DM_ENG_stringToUint(char* ch, unsigned int* pRes);
bool DM_ENG_stringToLong(char* ch, long* pRes);
bool DM_ENG_stringToBool(char* ch, bool* pRes);
bool DM_ENG_dateStringToTime(char* ch, time_t* pT);
char* DM_ENG_intToString(int i);
char* DM_ENG_uintToString(unsigned int i);
char* DM_ENG_longToString(long l);
char* DM_ENG_dateTimeToString(time_t t);

char* DM_ENG_strndup(const char* src, size_t size);

char* DM_ENG_encodeValue(char* str);
char* DM_ENG_decodeValue(char* str);

int DM_ENG_tablen(void* list[]);
char** DM_ENG_copyTabString(char* tCh[]);
void DM_ENG_deleteTabString(char* tCh[]);
char** DM_ENG_splitCommaSeparatedList(char* ch);
char* DM_ENG_buildCommaSeparatedList(char* tCh[]);
bool DM_ENG_belongsToCommaSeparatedList(char* csl, char* ch);
void DM_ENG_addToCommaSeparatedList(char** pCsl, char* ch);
bool DM_ENG_deleteFromCommaSeparatedList(char** pCsl, char* ch);

const char* DM_ENG_getFaultString(int code);

#endif
