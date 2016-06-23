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
 * File        : DM_ENG_ParameterType.h
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
 * @file DM_ENG_ParameterType.h
 *
 * @brief Parameter types
 *
 */

#ifndef _DM_ENG_PARAMETER_TYPE_H_
#define _DM_ENG_PARAMETER_TYPE_H_

/**
 * Parameter types
 */
typedef enum _DM_ENG_ParameterType
{
   DM_ENG_ParameterType_INT,
   DM_ENG_ParameterType_UINT,
   DM_ENG_ParameterType_LONG,
   DM_ENG_ParameterType_BOOLEAN,
   DM_ENG_ParameterType_DATE,
   DM_ENG_ParameterType_STRING,
   DM_ENG_ParameterType_ANY,
   DM_ENG_ParameterType_STATISTICS,
   DM_ENG_ParameterType_UNDEFINED

} DM_ENG_ParameterType;

DM_ENG_ParameterType DM_ENG_stringToType(char* str, int* pMin, int* pMax);
char* DM_ENG_typeToString(DM_ENG_ParameterType type, int min, int max);

#endif
