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
 * File        : CMN_Type_Def.h
 *
 * Created     : 2008/06/05
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
 * @file CMN_Type_Def.h
 *
 * @brief definition of some usefull type, and declaration
 *
 **/

#ifndef _CMN_TYPE_DEF_H_
#define _CMN_TYPE_DEF_H_

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>

/***************************************************************************************************
 *                   			 CONST DEFINITION
 **************************************************************************************************/

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

// Adding of two ugly definitions for compapatibility with others modules
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#if defined(__GNUC__)
#  define UNUSED __attribute__((unused))
#else
#  define UNUSED
#endif

#ifndef dateTime
typedef time_t	dateTime;
#endif

/***************************************************************************************************
 *                   			 MACRO DEFINITION
 **************************************************************************************************/

#ifndef MIN
#define MIN(a,b) (((a)>(b)) ? (b) : (a))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#endif /* _CMN_TYPE_DEF_H_ */
