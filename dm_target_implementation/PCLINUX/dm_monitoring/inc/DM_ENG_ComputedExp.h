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
 * File        : DM_ENG_ComputedExp.h
 *
 * Created     : 26/07/2010
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
 * @file DM_ENG_ComputedExp.h
 *
 * @brief Module dedicated to the parsing and the processing of the expressions of the COMPUTED parameters
 *
 **/

#ifndef _DM_ENG_COMPUTED_EXP_H_
#define _DM_ENG_COMPUTED_EXP_H_

#include "DM_ENG_Common.h"

/**
 * Id of operator
 */
typedef enum _DM_ENG_ComputedExp_Operator
{
   DM_ENG_ComputedExp_UNDEF = 0,
   DM_ENG_ComputedExp_LIST,
   DM_ENG_ComputedExp_GUARD,
   DM_ENG_ComputedExp_CONS,
   DM_ENG_ComputedExp_PLUS,
   DM_ENG_ComputedExp_MINUS,
   DM_ENG_ComputedExp_TIMES,
   DM_ENG_ComputedExp_DIV,
   DM_ENG_ComputedExp_U_PLUS,
   DM_ENG_ComputedExp_U_MINUS,
   DM_ENG_ComputedExp_IDENT,
   DM_ENG_ComputedExp_QUOTE,
   DM_ENG_ComputedExp_STRING,
   DM_ENG_ComputedExp_NUM,
   DM_ENG_ComputedExp_CALL,
   DM_ENG_ComputedExp_LT,
   DM_ENG_ComputedExp_LE,
   DM_ENG_ComputedExp_GT,
   DM_ENG_ComputedExp_GE,
   DM_ENG_ComputedExp_EQ,
   DM_ENG_ComputedExp_NE,
   DM_ENG_ComputedExp_NOT,
   DM_ENG_ComputedExp_AND,
   DM_ENG_ComputedExp_OR,
   DM_ENG_ComputedExp_CONCAT,
   DM_ENG_ComputedExp_NIL

} DM_ENG_ComputedExp_Operator;

/**
 * A Structure Expression
 */
typedef struct _DM_ENG_ComputedExp
{
   DM_ENG_ComputedExp_Operator oper;
   struct _DM_ENG_ComputedExp* left;
   struct _DM_ENG_ComputedExp* right;
   unsigned int num;
   char* str;

} __attribute((packed)) DM_ENG_ComputedExp;

typedef char* (*DM_ENG_F_GET_VALUE)(char* name, const char* data);
typedef bool (*DM_ENG_F_IS_VALUE_CHANGED)(char* name, const char* data, bool* pPushed);

DM_ENG_ComputedExp* DM_ENG_ComputedExp_parse(char* sExp);
bool DM_ENG_ComputedExp_eval(char* sExp, DM_ENG_F_GET_VALUE getValue, const char* data, char** pRes);
bool DM_ENG_ComputedExp_isChanged(char* sExp, DM_ENG_F_IS_VALUE_CHANGED isValueChanged, DM_ENG_F_GET_VALUE getValue, char* data, bool* pChanged, bool* pPushed);
bool DM_ENG_ComputedExp_isPredefinedFunction(char* name);
void DM_ENG_deleteComputedExp(DM_ENG_ComputedExp* exp);

#endif
